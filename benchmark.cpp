#include <chrono>
#include <random>
#include <vector>
#include <fstream>
#include <iostream>
#include <atomic>
#include <thread>
#include <cstdlib>
#include <pthread.h>
#include <sched.h>
#include <math.h>

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


//helpers for stats
double calculate_mean(double * items, int nitems){

    double sum = 0;

    for (int i = 0; i < nitems; i++){

        sum += items[i];

    }


    return sum/nitems;

}

double calculate_std_dev(double * items, double mean, int nitems){


    double sum = 0;

    for (int i = 0; i < nitems; i++){

        sum += pow(items[i] - mean, (double) 2);

    }

    return sqrt(sum/nitems);

}


double calculate_min(double * items, int nitems){

    double min = items[0];

    for (int i=1; i < nitems; i++){


        if (items[i] < min){
            min = items[i];
        }
    }

    return min;

}

double calculate_max(double * items, int nitems){

    double max = items[0];

    for (int i=1; i < nitems; i++){


        if (items[i] > max){
            max = items[i];
        }
    }

    return max;

}
