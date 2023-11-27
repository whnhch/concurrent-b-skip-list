#include <iostream>
#include <chrono>
#include <random>
#include <cstring>
#include <vector>
#include <fstream>
#include <iostream>
#include <stdlib.h>
#include <atomic>
#include <thread>
#include <cstdlib>
#include <pthread.h>
#include <sched.h>
#include <math.h>
#include <string>


#include "lock.h"


/******************

Benchmark

This benchmark tests correctness/performance of your reader-writer lock implementation

This is done by initializing an array where arr[i] = i
 - Writers increment the values in the array a set amount
 - Readers assert that arr[i+1] = arr[i] + 1

Reports min/max/mean/std_dev for both reader and writer lock acquisition

As long as your lock/PC implementation match the given header files
you should not need to modify this benchmark.

That being said, feel free to do so if you would like to test all versions of your
implementation concurrently.

******************/


//Thread args is the struct passed
typedef struct thread_args {
    ReaderWriterLock * rwlock;
    uint niters;
    uint nitems;
    uint thread_id; 
    uint * items;
    double * duration;

    data_entry * data;//data for query or inserting
    uint n_data;//num of entries
} thread_args;

typedef struct data_entry{
    uint op_code; //op_code = 0 for query, 1 for insert, 2 for delete
    uint value;
} data_entry;


//helpers for stats
double calculate_mean(double * items, uint nitems){

    double sum = 0;

    for (uint i = 0; i < nitems; i++){

        sum += items[i];

    }


    return sum/nitems;

}

double calculate_std_dev(double * items, double mean, uint nitems){


    double sum = 0;

    for (uint i = 0; i < nitems; i++){

        sum += pow(items[i] - mean, (double) 2);

    }

    return sqrt(sum/nitems);

}


double calculate_min(double * items, uint nitems){

    double min = items[0];

    for (uint i=1; i < nitems; i++){


        if (items[i] < min){
            min = items[i];
        }
    }

    return min;

}

double calculate_max(double * items, uint nitems){

    double max = items[0];

    for (uint i=1; i < nitems; i++){


        if (items[i] > max){
            max = items[i];
        }
    }

    return max;

}



//function used for reader threads
//for each iter, the thread acquires the lock, records time,
// and checks that a read/write hazard is not occuring
void *reader_thread_routine(void *args) {

    //read in args and determine cpuid
    thread_args * my_args = (thread_args *) args;
    ReaderWriterLock * lock = my_args->rwlock;
    // uint niters = my_args->niters;
    uint nitems = my_args->nitems;
    uint * items = my_args->items;

    uint n_data = my_args->n_data;
    data_entry * data = my_args->data;

    int cpuid = sched_getcpu();


    for (uint iter = 0; iter < n_iter; iter++){

        //timer measures delay in acquiring lock
        // write duration to output as double.
        std::chrono::high_resolution_clock::time_point start = std::chrono::high_resolution_clock::now();

        read_lock(lock, cpuid);

        std::chrono::high_resolution_clock::time_point end = std::chrono::high_resolution_clock::now();


        for (uint i = 0; i < n_data; i++){
            if(data[i]->op_code == 0){
                bskiplist.query(data[i]->value);
            }
            else{
                perror("Reader thread: invalid op_code");
            }
            // if (items[i]+1 != items[i+1]){
            //     perror("Panic: Readers and Writers are both in lock.");
            // }

        }

        read_unlock(lock, cpuid);

        //write output outside of critical region
        //*1000 to move to Milliseconds
        my_args->duration[iter] = (std::chrono::duration_cast<std::chrono::duration<double>>(end-start)).count()*1000;


    }

    return NULL;
}


//function used for writer threads
//every iteration, acquires the lock and records the time
// and increments the count of every item in the array by tid.
// this will cause a race condition if your lock allows for readers/writers to enter simultaneously
void *writer_thread_routine(void *args) {

    //read in args
    thread_args * my_args = (thread_args *) args;

    ReaderWriterLock * lock = my_args->rwlock;
    uint niters = my_args->niters;
    uint nitems = my_args->nitems;
    uint tid = my_args->thread_id;

    uint n_data = my_args->n_data;
    data_entry * data = my_args->data;


    uint * items = my_args->items;
  
    //not needed for writers
    //int cpuid = sched_getcpu();

    for (uint iter = 0; iter < niters; iter++){

        //time lock acquisition
        std::chrono::high_resolution_clock::time_point start = std::chrono::high_resolution_clock::now();

        write_lock(lock);

        std::chrono::high_resolution_clock::time_point end = std::chrono::high_resolution_clock::now();

        //writer determines offset set by previous writers
        //if this deviates, that means 2+ writers are in the lock.
        uint offset = items[0];

        for (uint i = 0; i < n_data; i++){

            if (data[i]->op_code == 1){
                bskiplist.insert(data[i]->value);
            }
            else if (data[i]->op_code == 2){
                bskiplist.delete(data[i]->value);
            }
            else{
                perror("Writer thread: invalid op_code");
            }
            // if (items[i] != offset + i){
            //     perror("Panic: Multiple writers in lock.");
            // }

            // items[i] += (tid+1);
        }

        write_unlock(lock);

        //write output outside of critical region
        //*1000 to move to Milliseconds
        my_args->duration[iter] = (std::chrono::duration_cast<std::chrono::duration<double>>(end-start)).count()*1000;



    }

    return NULL;
}

void parse_data_from_txt(std:string fname, data_entry * data){
    std::ifstream file(fname);
    if (!file.is_open()) {
        std::cerr << "Error opening file: " << fname << std::endl;
        return;
    }

    std::string operation;
    unsigned int value;
    size_t index = 0;

    while (file >> operation >> value) {
        if (operation == "query") {
            data[index].op_code = 0;
        } else if (operation == "insert") {
            data[index].op_code = 1;
        } else if (operation == "delete") {
            data[index].op_code = 2;
        } else {
            perror("Invalid operation");
            continue;
        }

        data[index].value = value;
        index++;
    }

    file.close();
}

int main(int argc, char **argv) {
    if (argc != 6) {
        std::cout << "Usage: ./benchmark [num reader threads][num writer threads][num items][num iterations]\n";
        return 1;
    }


    //read inputs from command line
    uint nreaders = atoi(argv[1]);
    uint nwriters = atoi(argv[2]);
    uint nitems = atoi(argv[3]);
    uint niters = atoi(argv[4]);
    uint n_data = atoi(argv[5]);



    if (nreaders == 0 || nwriters == 0 || nitems == 0 || niters == 0 || n_data == 0){
        perror("All arguments must be > 0");
        return -1;
    }


    printf("Running benchmark with %u readers, %u writers, %u items, %u iterations, %u data entries\n", nreaders, nwriters, nitems, niters, n_data);


    //first, malloc the lock and initialize
    ReaderWriterLock * lock = (ReaderWriterLock *) malloc(sizeof(ReaderWriterLock));

    if (lock == NULL){
        perror("Malloc lock");
        return -1;
    }

    //initialize the lock
    rw_lock_init(lock);


    //init the array of items, arr[i] = i;
    uint * items = (uint *) malloc(nitems*sizeof(uint));

    if (items == NULL){
        perror("Malloc items");
        return -1;
    }

    for (uint i = 0; i < nitems; i++){
        items[i] = i;
    }



    //output buffer for reader threads
    //each iteration outputs one double of duration per thread
    //this double represents time in milliseconds taken using std::chrono::high_resolution_clock
    double * reader_output = (double *) malloc(nreaders*niters*sizeof(double));

    if (reader_output == NULL){
        perror("Malloc reader output");
        return -1;
    }


    //init reader args
    thread_args * reader_args = (thread_args *) malloc(nreaders*sizeof(thread_args));

    if (reader_args == NULL){
        perror("Malloc reader args");
        return -1;
    }

    for (uint i = 0; i < nreaders; i++){

        reader_args[i].rwlock = lock;
        reader_args[i].nitems = nitems;
        reader_args[i].niters = niters;
        reader_args[i].thread_id = i;
        reader_args[i].items = items;
        reader_args[i].duration = reader_output + (i*niters);

        reader_args[i].n_data = n_data;
        reader_args[i].data = (data_entry *) malloc(n_data*sizeof(data_entry));
        parse_data_from_txt("readers_"+std::to_string(i)+".txt", reader_args[i].data);
    }

    pthread_t * readers = (pthread_t *) malloc(nreaders*sizeof(pthread_t));

    if (readers == NULL){
        perror("Malloc readers");
        return -1;
    }




    //initialize writers/writer output in same fashion as readers.
    double * writer_output = (double *) malloc(nwriters*niters*sizeof(double));

    if (writer_output == NULL){
        perror("Malloc writer output");
        return -1;
    }



    //init writer args + writers
    thread_args * writer_args = (thread_args *) malloc(nwriters*sizeof(thread_args));

    if (writer_args == NULL){
        perror("Malloc writer args");
        return -1;
    }

    for (uint i = 0; i < nwriters; i++){

       writer_args[i].rwlock = lock;
       writer_args[i].nitems = nitems;
       writer_args[i].niters = niters;
       writer_args[i].thread_id = i;
       writer_args[i].items = items;
       writer_args[i].duration = writer_output + (i*niters);
       
       writer_args[i].n_data = n_data;
       writer_args[i].data = (uint *) malloc(n_data*sizeof(uint));
       parse_data_from_txt("writers"+std::to_string(i)+".txt", writer_args[i].data);
    }

    pthread_t * writers = (pthread_t *) malloc(nwriters*sizeof(pthread_t));

    if (writers == NULL){
        perror("Malloc writers");
        return -1;
    }


    //setup done, spawn threads
    for (uint i = 0; i < nreaders; i++){

        if (pthread_create(&readers[i], NULL, reader_thread_routine, (void *)&reader_args[i])){

            perror("pthread_create");
            return -1;

        }

    }

    for (uint i = 0; i < nwriters; i++){

        if (pthread_create(&writers[i], NULL, writer_thread_routine, (void *)&writer_args[i])){

            perror("pthread_create");
            return -1;

        }

    }


    //join threads
    //in reverse order to try and force a collision between reader/writer threads.
    for (uint i = 0; i < nwriters; i++){
        pthread_join(writers[i], NULL);
    }

    for (uint i = 0; i < nreaders; i++){
        pthread_join(readers[i], NULL);
    }


    //finally, calculate and print stats.
    printf("Threads done, stats:\n");

    double reader_mean = calculate_mean(reader_output, nreaders*niters);
    double reader_std_dev = calculate_std_dev(reader_output, reader_mean, nreaders*niters);
    double reader_min = calculate_min(reader_output, nreaders*niters);
    double reader_max = calculate_max(reader_output, nreaders*niters);

    printf("Readers: min %f ms, max %f ms, mean %f ms, std_dev %f\n", reader_min, reader_max, reader_mean, reader_std_dev);

    double writer_mean = calculate_mean(writer_output, nwriters*niters);
    double writer_std_dev = calculate_std_dev(writer_output, writer_mean, nwriters*niters);
    double writer_min = calculate_min(writer_output, nwriters*niters);
    double writer_max = calculate_max(writer_output, nwriters*niters);

    printf("Writers: min %f ms, max %f ms, mean %f ms, std_dev %f\n", writer_min, writer_max, writer_mean, writer_std_dev);



    //cleanup
    free(lock);

    free(reader_output);
    free(reader_args);
    free(readers);

    free(writer_output);
    free(writer_args);
    free(writers);


    free(items);
    
    for (uint i = 0; i < nreaders; i++){
        free(reader_args[i].data);
    }
    for (uint i = 0; i < nwriters; i++){
        free(writer_args[i].data);
    }
    

    return 0;
}
