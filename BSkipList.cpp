#include <iostream>
#include <chrono>
#include <cstring>
#include <fstream>
#include <stdlib.h>
#include <atomic>
#include <thread>
#include <cstdlib>
#include <pthread.h>
#include <sched.h>
#include <math.h>
#include <string>

#include <vector>
#include <stack>
#include <random>
#include <limits.h>
#include <time.h>
#include "lock.h"
#include "benchmark.cpp"

using namespace std;
class Block;
class Node;

typedef struct data_entry{
    int op_code; //op_code = 0 for query, 1 for insert, 2 for delete
    int value;
} data_entry;


//Thread args is the struct passed
typedef struct thread_args {
    int niters;
    int nitems;
    int thread_id; 
    int * items;
    double * duration;

    data_entry * data;//data for query or inserting
    int n_data;//num of entries
    int value;

} thread_args;

typedef struct readThreadParams{
    thread_args* args;
    Block* block;
} readThreadParams;

typedef struct writeThreadParams {
    thread_args* args;
    Block* block;
    Block* lower;
    int value;
    int offset;
    vector<Node*>::iterator it1;
    vector<Node*>::iterator it2;
    vector<Node*>::iterator it3;
} writeThreadParams;

class Node
{
public:
    int value;
    Block *down; // Pointer to lower level block contains same value
    Node(int value, Block *down)
    {
        this->value = value;
        this->down = down;
    }
};

class Block
{
public:
    std::vector<Node *> vector;
    Block *next; // Pointer to the next block at the same level
    //Make new counters for reader and writer
    ReaderWriterLock * lock = (ReaderWriterLock *) malloc(sizeof(ReaderWriterLock));

    Block(Node *node, Block *next)
    {
        vector.push_back(node);
        // vector.resize(3); // minimum size of each block
        this->next = next;
        
        //Initiailize the locks
        rw_lock_init(this->lock);
    }

    Block(std::vector<Node *> vector, Block *next)
    {
        this->vector = vector;
        // vector.resize(3); // minimum size of each block
        this->next = next;
        
        //Initiailize the locks
        rw_lock_init(this->lock);
    }

    void print()
    {
        for (unsigned int i = 0; i < vector.size(); i++)
        {
            std::cout << vector[i]->value;
            if (vector[i]->down)
                std::cout << "(" << vector[i]->down->vector[0]->value << ")";
            std::cout << " ";
        }
        std::cout << "| ";
    }

    void rw_lock_init(ReaderWriterLock *rwlock) {
        rwlock->readers = (int64_t*)malloc(sizeof(int64_t) * 16);
        rwlock->writer = 0;
    }

};


//function used for reader threads
//for each iter, the thread acquires the lock, records time,
// and checks that a read/write hazard is not occuring
void* reader_thread_routine(void* args) {

    //read in args and determine cpuid
    readThreadParams * my_args = (readThreadParams *) args;
    ReaderWriterLock * lock = my_args->block->lock;

    int cpuid = sched_getcpu();

    //timer measures delay in acquiring lock
    // write duration to output as double.
    std::chrono::high_resolution_clock::time_point start = std::chrono::high_resolution_clock::now();

    read_lock(lock, cpuid);

    std::chrono::high_resolution_clock::time_point end = std::chrono::high_resolution_clock::now();

    read_unlock(lock, cpuid);

    //write output outside of critical region
    //*1000 to move to Milliseconds
    my_args->args->duration[0] = (std::chrono::duration_cast<std::chrono::duration<double>>(end-start)).count()*1000;

    return NULL;
}
//every iteration, acquires the lock and records the time
// and increments the count of every item in the array by tid.
// this will cause a race condition if your lock allows for readers/writers to enter simultaneously
void* writer_insert_thread_routine(void *args) {
    //read in args
    writeThreadParams * w_args = (writeThreadParams *) args;
    thread_args * my_args = w_args->args;

    ReaderWriterLock * lock = w_args->block->lock;

    int niters = my_args->niters;
    int nitems = my_args->nitems;
    int tid = my_args->thread_id;

    int n_data = my_args->n_data;
    data_entry * data = my_args->data;
    int * items = my_args->items;

    int offset = w_args->offset;
    Block* block = w_args->block;
    Block* lower = w_args->lower;
    int new_val = w_args->value;

    //not needed for writers
    //int cpuid = sched_getcpu();

    //time lock acquisition
    std::chrono::high_resolution_clock::time_point start = std::chrono::high_resolution_clock::now();

    write_lock(lock);

    std::chrono::high_resolution_clock::time_point end = std::chrono::high_resolution_clock::now();

    //writer determines offset set by previous writers
    //if this deviates, that means 2+ writers are in the lock.
    // int offset = items[0];

    for (int i = 0; i < n_data; i++){
        if(offset==-1){
            block->vector.push_back(new Node(new_val, lower));
        }
        else{
            block->vector.insert(block->vector.begin() + offset, new Node(new_val, lower));
        }
    }

    write_unlock(lock);

    //write output outside of critical region
    //*1000 to move to Milliseconds
    my_args->duration[0] = (std::chrono::duration_cast<std::chrono::duration<double>>(end-start)).count()*1000;

    return NULL;
}

void* writer_remove_thread_routine(void *args) {
    //read in args
    writeThreadParams * w_args = (writeThreadParams *) args;
    thread_args * my_args = w_args->args;

    ReaderWriterLock * lock = w_args->block->lock;

    int niters = my_args->niters;
    int nitems = my_args->nitems;
    int tid = my_args->thread_id;

    int n_data = my_args->n_data;
    data_entry * data = my_args->data;
    int * items = my_args->items;

    int offset = w_args->offset;
    Block* block = w_args->block;

    vector<Node*>::iterator it1 = w_args->it1;
    vector<Node*>::iterator it2 = w_args->it2;
    vector<Node*>::iterator it3 = w_args->it3;

    //not needed for writers
    //int cpuid = sched_getcpu();

    //time lock acquisition
    std::chrono::high_resolution_clock::time_point start = std::chrono::high_resolution_clock::now();

    write_lock(lock);

    std::chrono::high_resolution_clock::time_point end = std::chrono::high_resolution_clock::now();

    //writer determines offset set by previous writers
    //if this deviates, that means 2+ writers are in the lock.
    // int offset = items[0];

    for (int i = 0; i < n_data; i++){
        //if op is deletion
        if(offset!=-1){
            block->vector.erase(block->vector.begin()+offset);
        }
        else{
            block->vector.insert(it1, it2, it3);
        }
    }

    write_unlock(lock);

    //write output outside of critical region
    //*1000 to move to Milliseconds
    my_args->duration[0] = (std::chrono::duration_cast<std::chrono::duration<double>>(end-start)).count()*1000;

    return NULL;
}


class BSkipList
{
private:
    std::vector<Block *> levels; // Vector of head blocks from each level
    std::stack<Block *> getBlockStack(int value)
    {
        int lvl = levels.size() - 1;
        Block *current = levels[levels.size() - 1]; // starting from first block in higest level
        std::stack<Block *> blocks;                 // store the path
        Block *block = current;                     // keep track the place for value
        Node *prev;
        while (current)
        {
            bool found = false;
            // find a value greater than insert value
            for (unsigned int i = 0; i < current->vector.size(); i++)
            {
                if (value > current->vector[i]->value)
                { // go to next node
                    prev = current->vector[i];
                }
                else
                { // find the place
                    blocks.push((block));
                    current = prev->down;
                    lvl--;
                    block = current;
                    found = true;
                    break;
                }
            }

            if (!found)
            {
                // keep looking in next block
                if (current->next)
                {
                    current = current->next;
                    // last in current block
                    if (value < current->vector[0]->value)
                    {
                        blocks.push(block);
                        current = prev->down;
                    }
                }
                else // last in this level
                    blocks.push(current);
                current = prev->down;
            }
            block = current;
        }
        return blocks;
    }

public:
    int r = 1;
    const float P_FACTOR = 0.25;
    // const int MAX_LEVEL = 32;
    // const float P_FACTOR = 0.25;
    // static std::random_device rd; // obtain a random number from hardware
    // static std::mt19937 gen(rand()); // seed the generator
    // static std::uniform_real_distribution<> distr(0, 1); // define the range
    BSkipList()
    {
        Block *block = new Block(new Node(INT_MIN, nullptr), nullptr); // negative infinity block
        levels.push_back(block);
    }

    ~BSkipList()
    {
        // Destructor to free memory
        // ... (cleanup logic here)
    }

    int insert(pthread_t * writer, thread_args *my_args)
    {
        //let's flip the coin
        // vector<bool> coin_flip;
        // while((static_cast<float>(rand()) / RAND_MAX) >= P_FACTOR){
        //     coin_flip.push_back(false)
        // }
        // coin_flip.push_back(true);
        writeThreadParams* param = (writeThreadParams *) malloc(sizeof(writeThreadParams));
        param->args = my_args;

        int value = my_args->value;
        param->value = value;

        srand(time(NULL)); // initialize random seed
        std::stack<Block *> blocks = getBlockStack(value); //return block paths for value
        Block *lower = nullptr;
        // building block from botton

       while (!blocks.empty())
        {
            bool inserted = false;
            Block *block = blocks.top(); //from bottom
            blocks.pop(); 
            //search for the item in current block
            for (unsigned int i = 0; i < block->vector.size(); i++)
            {
                //if item is smaller than current node
                if (block->vector[i]->value > value)
                { // in the middle of the vector
                    if ((static_cast<float>(rand()) / RAND_MAX) < P_FACTOR)
                    { // tail
                        //r = r + rand();
                        // writer_insert_thread_routine(my_args, block, value, lower);
                        param->block = block;
                        param->lower = lower;
                        param->offset= i;
                        if (pthread_create(writer, NULL, writer_insert_thread_routine, (void *)param)){
                            perror("pthread_create");
                            return -1;
                        }
                        return 1;
                    }
                    else
                    { // head
                        r++;
                        // split and shrink block
                        std::vector<Node *> right;
                        right.push_back(new Node(value, lower));
                        for (unsigned int j = i; j < block->vector.size(); j++)
                            right.push_back(block->vector[j]);
                        
                        block->vector.resize(i);
                        Block *rightBlock = new Block(right, block->next);
                        block->next = rightBlock;
                        // new level
                        if (blocks.empty())
                        {
                            Block *up = new Block(new Node(INT_MIN, block), nullptr);
                            up->vector.push_back(new Node(value, block->next));
                            levels.push_back(up);
                        }
                        inserted = true;
                        lower = block->next;
                        break;
                    }
                }
            }
            if (!inserted)
            {
                // at the end of the vector
                if ((static_cast<float>(rand()) / RAND_MAX) < P_FACTOR)
                { // tail
                    r = r + 1;
                    param->block = block;
                    param->lower = lower;
                    param->offset= -1;

                    if (pthread_create(writer, NULL, writer_insert_thread_routine, (void *)param)){
                        perror("pthread_create");
                        return -1;
                    }
                    return 1;
                }
                else
                { // head
                    r = r + rand();
                    Block *newBlock = new Block(new Node(value, lower), block->next);
                    block->next = newBlock;
                    // new level
                    if (blocks.empty())
                    {
                        Block *up = new Block(new Node(INT_MIN, block), nullptr);
                        up->vector.push_back(new Node(value, newBlock));
                        levels.push_back(up);
                    }
                    lower = newBlock;
                }
            }
        }
        return 1;
    }

    int remove(pthread_t * writer, thread_args *my_args)
    {
        writeThreadParams* param = (writeThreadParams *) malloc(sizeof(writeThreadParams));
        param->args = my_args;

        int value = my_args->value;
        param->value = value;

        std::stack<Block *> blocks = getBlockStack(value);
        Block *current;
        Block *block;
        vector<Block *> update;
        Block *curr = nullptr;
        bool flag = false;
        cout << "I made param variable" << endl;

        for (int i = levels.size() - 1; i >= 0; i--)
        {
            Block *pre = nullptr;
            curr = levels[i];
            while (curr)
            {
                for (int j = 0; j < curr->vector.size(); j++)
                {
                    if (curr->vector[j]->value == value)
                    {
                        if (pre)
                        {
                            flag = true;
                            update.push_back(pre);
                            //cout << pre->vector[0]->value << "pre" << endl;
                        }
                        break;
                    }
                }
                if (flag)
                {
                    flag = false;
                    break;
                }

                pre = curr;
                curr = curr->next;
            }
        }

        int x = 0;
        while (!blocks.empty())
        {
            block = blocks.top();
            blocks.pop();

            for (unsigned int i = 0; i < block->vector.size(); i++)
            {
                if (block->vector[i]->value == value)
                {
                    Block *downBlock = block->vector[i]->down;
                    
                    param->block = block;
                    param->offset= i;
                    if (pthread_create(writer, NULL, writer_remove_thread_routine, (void *)param)){
                        perror("pthread_create");
                        return -1;
                    }

                    while (downBlock != nullptr)
                    {
                        current = downBlock->vector[0]->down;
                        param->block = downBlock;
                        param->offset= 0;
                        pthread_create(writer, NULL, writer_remove_thread_routine, (void *)param);

                        if(!downBlock->vector.empty()){
                            //Change writer_thread_routine could get all.
                            param-> block = update[x];
                            param-> it1 = update[x]->vector.end();
                            param-> it2 = downBlock->vector.begin();
                            param-> it3 = downBlock->vector.end();
                            if (pthread_create(writer, NULL, writer_remove_thread_routine, (void *)param)){
                                perror("pthread_create");
                                return -1;
                            }
                            // update[x]->vector.insert(update[x]->vector.end(), downBlock->vector.begin(),downBlock->vector.end());
                            update[x]->next = update[x]->next->next;
                            x++;
                        }else{
                            update[x]->next = update[x]->next->next;
                            x++;
                        }

                        downBlock = current;
                    }
                }
            }
        }
        return 1;
    }

    void print_list(){
        Block* curr;
        for (int i = levels.size() - 1; i >= 0; i--)
        {
            Block *pre = nullptr;
            curr = levels[i];
            while (curr)
            {
                for (int j = 0; j < curr->vector.size(); j++)
                {

                    cout << curr->vector[j]->value << " ";
                    if(curr->vector[j]->down){
                        cout << "(" << curr->vector[j]->down->vector[0]->value << ")";
                    }
                }
                curr = curr->next;
                cout << "|";
            }
            cout << " " << endl;
        }
    }

    void print()
    {
        for (unsigned int i = levels.size() - 1; i >= 0; i--)
        {
            Block *current = levels[i];
            while (current)
            {
                current->print();
                current = current->next;
            }
            std::cout << std::endl;
        }
    }

    bool search(pthread_t * reader, thread_args *my_args)
    {
        readThreadParams* param = (readThreadParams *) malloc(sizeof(readThreadParams));

        param->args = my_args;

        int key = my_args->value;

        int cpuid = sched_getcpu();
        
        std::vector<Node *>::iterator it;
        Node *node;
        Node *prev_node;
        Block *block = levels[levels.size() - 1];

        while (block)
        {
            for (it = block->vector.begin(); it != block->vector.end(); ++it)
            {
                param->block = block;
                if (pthread_create(reader, NULL, reader_thread_routine, (void *)param)){
                    perror("pthread_create");
                    return false;
                }
                node = *it;
                if (node->value < key)
                {                 
                    prev_node = node;
                    if (node == *std::prev(block->vector.end()))
                    {
                        block = block->next;
                        break;
                    }
                    else
                    {
                        continue;
                    }
                }
                else if (node->value == key)
                {
                    return true;
                }
                else if (key < node->value)
                {
                    block = prev_node->down;
                    break;
                }
                // else if (i == 0) {return false;}
            }
        }
        // }
        return false;
    }

    std::vector<bool> range_query(int _start_key, int _end_key) {
        int start_key = _start_key;
        int end_key = _end_key;

        std::vector<Node*>::iterator it;
        std::vector<bool> output;
        bool value = false;
        
        Node* node;
        Node* prev_node;

        //first find the start key value
        Block* block = levels[levels.size() - 1];

        while(true){
            while(block) {
                if(value){break;}
                
                for (it = block->vector.begin(); it != block->vector.end(); ++it) {
                    node = *it;
                    if(node->value < start_key){
                        prev_node = node; 
                        if(node==*std::prev(block->vector.end())){
                            block = block->next;
                            break;
                        }
                        else{continue;}
                    }
                    else if(node->value == start_key) { 
                        value = true;
                        break;
                    }
                    else if (start_key < node->value){
                        block = prev_node->down; 
                        break;
                    } 
                }
            }
            output.push_back(value);
            start_key+=1;
            
            if(value){++it; break;}
            //prevent when the first key is not found
            //if the first key is not found set the next key is first key
            else if(start_key == end_key){break;}
            else{
                block = levels[levels.size() - 1];
            }
        }

        //propagates to next node until the key is below than end_key
        //if there is no more blocks the break the loop.
        int cur_key = start_key;

        while(block){ 
            if(cur_key >= end_key) break;

            if(it == block->vector.end()) {
                block = block->next;
                if(block==NULL){break;}
                it = block->vector.begin();
            }

            node = *it;
            if(node->value == cur_key){
                value = true;
                ++cur_key;
                ++it;
            }
            else if(node->value > cur_key){
                value = false;
                ++cur_key;
            }

            else{
                value = false;
                ++it;
            }
            output.push_back(value);
        }

        //if the end block reached before finding all keys
        //fill the output with false
        if(cur_key<end_key){
            for(;cur_key<end_key;cur_key++){
                output.push_back(false);
            }
        }
        return output;
    }
};

void parse_data_from_txt(string fname, data_entry * data){
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
        std::cout << "Usage: ./BSkipList [num reader threads][num writer threads][num items][num iterations][reader_data][writer_data]\n";
        return 1;
    }


    //read inputs from command line
    int nreaders = atoi(argv[1]);
    int nwriters = atoi(argv[2]);
    int nitems = atoi(argv[3]);
    int niters = atoi(argv[4]);
    int n_data = atoi(argv[5]);


    
    if (nreaders == 0 || nwriters == 0 || nitems == 0 || niters == 0 || n_data == 0){
        perror("All arguments must be > 0");
        return -1;
    }


    printf("Running benchmark with %u readers, %u writers, %u items, %u iterations, %u data entries\n", nreaders, nwriters, nitems, niters, n_data);

    //init the array of items, arr[i] = i;
    int * items = (int *) malloc(nitems*sizeof(int));

    if (items == NULL){
        perror("Malloc items");
        return -1;
    }

    for (int i = 0; i < nitems; i++){
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

    for (int i = 0; i < nreaders; i++){
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

    for (int i = 0; i < nwriters; i++){
       writer_args[i].nitems = nitems;
       writer_args[i].niters = niters;
       writer_args[i].thread_id = i;
       writer_args[i].items = items;
       writer_args[i].duration = writer_output + (i*niters);
       
       writer_args[i].n_data = n_data;
       writer_args[i].data = (data_entry *) malloc(n_data*sizeof(data_entry));
       parse_data_from_txt("writers"+std::to_string(i)+".txt", writer_args[i].data);
    }

    pthread_t * writers = (pthread_t *) malloc(nwriters*sizeof(pthread_t));

    if (writers == NULL){
        perror("Malloc writers");
        return -1;
    }

    BSkipList list;

    //setup done, spawn threads
    for (int i = 0; i < nreaders; i++){
        for (int j = 0; j < n_data; j++){
            reader_args[i].value = reader_args[i].data[j].value;
            list.search(&readers[i], &reader_args[i]);
        }
    }

    //just make a one more writer for deletion or distinguish inside of the file (iserting, deletion x)
    for (int i = 0; i < nwriters; i++){
        for (int j = 0; j < n_data; j++){
            writer_args[i].value = writer_args[i].data[j].value;

            if(writer_args[i].data[j].op_code == 1){
                cout << "Start inserting value: " << writer_args[i].value << endl;
                list.insert(&writers[i], &writer_args[i]);
                list.print_list();
            }
            else{
                cout << "Start removing value: " << writer_args[i].value << endl;
                list.remove(&writers[i], &writer_args[i]);
            }
        }
    }

    //join threads
    //in reverse order to try and force a collision between reader/writer threads.
    for (int i = 0; i < nwriters; i++){
        pthread_join(writers[i], NULL);
    }

    for (int i = 0; i < nreaders; i++){
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
    // free(lock);

    free(reader_output);
    free(reader_args);
    free(readers);

    free(writer_output);
    free(writer_args);
    free(writers);


    free(items);
    
    for (int i = 0; i < nreaders; i++){
        free(reader_args[i].data);
    }
    for (int i = 0; i < nwriters; i++){
        free(writer_args[i].data);
    }
    
    return 0;
}