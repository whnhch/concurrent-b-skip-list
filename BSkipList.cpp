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
#include <sstream>

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
class BSkipList;
#define NUM_COUNTERS 16

//Thread args is the struct passed
typedef struct thread_args {
    int thread_id; 
    double * duration;
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
    Block* next;
    vector<Block*> levels;
    int value;
    unsigned int offset;
    vector<Node*>::iterator it1;
    vector<Node*>::iterator it2;
    vector<Node*>::iterator it3;
} writeThreadParams;

typedef struct threadWrapper {
    BSkipList* list;
    thread_args* thread_args;
} threadWrapper;

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
        rw_lock_init();
    }

    Block(std::vector<Node *> vector, Block *next)
    {
        this->vector = vector;
        // vector.resize(3); // minimum size of each block
        this->next = next;
        
        //Initiailize the locks
        rw_lock_init();
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

    void rw_lock_init() {
        this->lock->readers = (int64_t*)malloc(sizeof(int64_t) * NUM_COUNTERS);
        this->lock->writer = 0;
        for(int i = 0; i < NUM_COUNTERS; i++){ 
            this->lock->readers[i] = 0;
        }
    }

    bool r_lock_check(int thread_id) {
        return this->lock->readers[thread_id]>0;
    }

    bool w_lock_check() {
        return this->lock->writer>0;
    }

    void read_lock(int thread_id) {
        if(!r_lock_check(thread_id)) read_lock(this->lock, thread_id);
    }
    void read_unlock(int thread_id) {
        if(r_lock_check(thread_id)) read_unlock(this->lock, thread_id);
    }

    void write_lock(void) {
        if(!w_lock_check()) write_lock(this->lock);
    }
    void write_unlock(void) {
        if(w_lock_check()) write_unlock(this->lock);
    }

    void read_lock(ReaderWriterLock *rwlock, uint8_t thread_id) {
        //acq read lock
        while (true){

            //atomic_add_fetch returns current value, but not needed
            __atomic_add_fetch(&rwlock->readers[thread_id], 1, __ATOMIC_SEQ_CST);

            if (rwlock->writer){
            //cancel
            __atomic_add_fetch(&rwlock->readers[thread_id], -1, __ATOMIC_SEQ_CST);
            //wait
            while (rwlock->writer);
            } else {
            return;
            }
        }
    }


    //release an acquired read lock for thread `thread_id`
    void read_unlock(ReaderWriterLock *rwlock, uint8_t thread_id) {
        __atomic_add_fetch(&rwlock->readers[thread_id], -1, __ATOMIC_SEQ_CST);
        return;
    }

    void write_lock(ReaderWriterLock *rwlock) {
        // acquire write lock.
        while (__sync_lock_test_and_set(&rwlock->writer, 1)){
            while (rwlock->writer != 0);
        }
        //once acquired, wait on readers
        for(int i = 0; i < NUM_COUNTERS; i++){ 
            while (rwlock->readers[i] != 0);
        }
        return;
    }

    //Release an acquired write lock.
    void write_unlock(ReaderWriterLock *rwlock) {
        __atomic_add_fetch(&rwlock->writer, -1, __ATOMIC_SEQ_CST);
        // __sync_lock_release(&rwlock->writer);
        return;
    }

};

std::atomic<int> operations_completed(0);
void timer_thread(int test_duration) {
    // Pause the thread for the duration of the test
    std::this_thread::sleep_for(std::chrono::seconds(test_duration));
}

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
void writer_insert_thread_routine(void *args) {
    //read in args
    writeThreadParams * w_args = (writeThreadParams *) args;
    thread_args * my_args = w_args->args;

    ReaderWriterLock * lock = w_args->block->lock;

    int tid = my_args->thread_id;

    unsigned int offset = w_args->offset;
    Block* block = w_args->block;
    Block* lower = w_args->lower;
    int new_val = w_args->value;

    //not needed for writers
    // int cpuid = sched_getcpu();
    //time lock acquisition

    std::chrono::high_resolution_clock::time_point start = std::chrono::high_resolution_clock::now();

    block->write_lock();

    std::chrono::high_resolution_clock::time_point end = std::chrono::high_resolution_clock::now();

    //writer determines offset set by previous writers
    //if this deviates, that means 2+ writers are in the lock.
    // int offset = items[0];

    if(offset==-1){
        block->vector.push_back(new Node(new_val, lower));
    }
    else if(offset==-2){
        // Block *newBlock = new Block(new Node(new_val, lower), w_args->next);
        w_args->block->next = w_args->next;
    }
    else if(offset==-3){
        w_args->levels.push_back(block);
    }
    else{
        block->vector.insert(block->vector.begin() + offset, new Node(new_val, lower));
    }

    block->write_unlock();

    //write output outside of critical region
    //*1000 to move to Milliseconds
    my_args->duration[tid] = (std::chrono::duration_cast<std::chrono::duration<double>>(end-start)).count()*1000;

    return;
}

void* writer_remove_thread_routine(void *args) {
    //read in args
    writeThreadParams * w_args = (writeThreadParams *) args;
    thread_args * my_args = w_args->args;

    ReaderWriterLock * lock = w_args->block->lock;

    int tid = my_args->thread_id;

    unsigned int offset = w_args->offset;
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

    if(offset!=-1){
        block->vector.erase(block->vector.begin()+offset);
    }
    else{
        block->vector.insert(it1, it2, it3);
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
std::pair<std::stack<Block *>, std::vector<bool>> getBlockStack(int value, int cpuid)
    {

        srand(time(NULL)); // initialize random seed

        std::vector<bool> coin_flip;
        while((static_cast<float>(rand()) / RAND_MAX) >= 0.25){
            coin_flip.push_back(false);
            if(coin_flip.size() >= this->levels.size()+10000){break;}
        }
        coin_flip.push_back(true);

        int lvl = this->levels.size() - 1;
        Block *current = this->levels[this->levels.size() - 1]; // starting from first block in higest level
        std::stack<Block *> blocks;                 // store the path
        Block *block = current;                     // keep track the place for value
        Node *prev;
        current->read_lock(cpuid);

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
                    block->read_lock(cpuid);
                    // block->write_lock();
                    blocks.push((block));
                    if(current && block != current) current->read_unlock(cpuid); 
                    current = prev->down;
                    if(current) current->read_lock(cpuid); 

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
                    if(current) current->read_unlock(cpuid);
                    current = current->next;
                    if(current) current->read_lock(cpuid);

                    // last in current block
                    if (value < current->vector[0]->value)
                    {
                        block->read_lock(cpuid);
                        // block->write_lock();
                        blocks.push(block);
                        
                        if(current && block != current) current->read_unlock(cpuid);
                        current = prev->down;
                        if(current) current->read_lock(cpuid);
                    }
                }
                else{ // last in this level
                    current->read_lock(cpuid);
                    // current->write_lock();
                    blocks.push(current);
                }
                if(current && block != current) current->read_unlock(cpuid);
                current = prev->down;
                if(current) current->read_lock(cpuid);
            }
            block = current;
        }
        // coinflip: H T
        // block size: 1 1 1
        if(coin_flip.size()<blocks.size()){
            for(int i=coin_flip.size(); i<blocks.size();i++){
                current = blocks.top();
                current->read_unlock(cpuid);
                // current->write_unlock();
                blocks.pop();
            }
            // coin_flip.at(blocks.size()-1)=true;
        }
        return std::make_pair(blocks, coin_flip);
    }

    void unlock_all(std::stack<Block *> list, int cpuid){
        if(!list.empty()){
        for(;!list.empty();list.pop()){
            list.top()->read_unlock(cpuid);
            list.top()->write_unlock();
        }
        }
        return;
    }

    int insert(thread_args *my_args)
    {
        writeThreadParams* param = (writeThreadParams *) malloc(sizeof(writeThreadParams));
        param->args = my_args;

        int value = my_args->value;
        param->value = value;

        int cpuid = sched_getcpu();

        srand(time(NULL)); // initialize random seed
        // std::stack<Block *> blocks = getBlockStack(my_args); //return block paths for value
        auto result = getBlockStack(value, cpuid);
        std::stack<Block *> blocks = result.first;
        std::vector<bool> coin_flip = result.second;
        int lvl = blocks.size() - 1;
        if(lvl<0) lvl =0;

        Block *lower = nullptr;

        // building block from botton
        while (!blocks.empty())
        {
            bool inserted = false;
            Block *block = blocks.top();
            blocks.pop();

            // cout << "value : " << value << endl;
            for (unsigned int i = 0; i < block->vector.size(); i++)
            {
                if (block->vector[i]->value > value)
                { // in the middle of the vector
                    // cout << value << " ((1) : " << block->vector[i]->value << endl;

                    if (coin_flip[lvl])
                    { // tail
                    
                        if(lower) lower->read_lock(cpuid);
                        param->block = block;
                        param->lower = lower;
                        param->offset= i;
                        block->read_unlock(cpuid);
                        writer_insert_thread_routine((void *)param);
                        if(lower) lower->read_unlock(cpuid);
                        unlock_all(blocks,cpuid);
                        return 1;
                    }
                    else
                    { // head
                        // cout << value << " ((2) : " << block->vector[i]->value << endl;

                        r++;
                        // split and shrink block
                        std::vector<Node *> right;
                        right.push_back(new Node(value, lower));
                        if(lower) lower->read_unlock(cpuid);
                        
                        for (unsigned int j = i; j < block->vector.size(); j++){
                            right.push_back(block->vector[j]);
                        }
                        
                        block->read_unlock(cpuid);
                        block->write_lock();
                        block->vector.resize(i);
                        block->write_unlock();

                        Block *rightBlock = new Block(right, block->next);
                        rightBlock->read_lock(cpuid);
                        block->next = rightBlock;
                        rightBlock->read_unlock(cpuid);

                        // new level
                        if (blocks.empty())
                        {
                            Block* cur_top = this->levels.back();
                            if(cur_top) cur_top->read_lock(cpuid);
                            
                            Block *up = new Block(new Node(INT_MIN, block), nullptr);
                            if(up) up->write_lock();
                            up->vector.push_back(new Node(value, block->next));
                            levels.push_back(up);
                            if(up) up->write_unlock();

                            if(cur_top) cur_top->read_unlock(cpuid);
                        }
                        inserted = true;
                                                
                        lower = block->next;
                        lower->read_lock(cpuid);
                        
                        break;
                    }
                }
            }
            if (!inserted)
            {
                // at the end of the vector
                if (coin_flip[lvl])
                { // tail

                    // cout << value << " ((3) : " << endl;

                    r = r + 1;
                    // block->vector.push_back(new Node(value, lower));
                    // cout << value << " ((3)-1 : " << endl;
                    
                    param->block = block;
                    param->lower = lower;
                    param->offset= -1;
                    // cout << value << " ((3)-2 : " << endl;
                    block->read_unlock(cpuid);
                    writer_insert_thread_routine((void *)param);
                    if(lower) lower->read_unlock(cpuid);
                    unlock_all(blocks, cpuid);

                    // cout << value << " ((3)-3 : " << endl;
                    // cout << value << " ((3)-4 : " << endl;
                    return 1;
                }

                else
                { // head
                    block->read_unlock(cpuid);
                    // cout << value << " (4) : "  << endl;

                    r = r + rand();

                    if(lower) lower->read_lock(cpuid);
                    Block *newBlock = new Block(new Node(value, lower), block->next);
                    if(lower) lower->read_unlock(cpuid);
                    newBlock->read_lock(cpuid);
                    block->next = newBlock;
                    // cout << value << " (4)-(1) : "  << endl;

                    // new level
                    if (blocks.empty())
                    {
                        // cout << value << " (4)-(2) : "  << endl;

                        Block* cur_top = this->levels[this->levels.size()-1];
                        if(cur_top) cur_top->read_lock(cpuid);
                        // if(cur_top->next)
                            // cout << "cpuid: "<< cpuid << " level size: "<< this->levels.size() <<": cur top back " << cur_top->next->vector.back()->value << endl;
                        // if(block == cur_top) cur_top->read_unlock(cpuid);
                        Block *up = new Block(new Node(INT_MIN, block), nullptr);
                        // up->write_lock();
                        up->vector.push_back(new Node(value, newBlock));
                        newBlock->read_unlock(cpuid);

                        levels.push_back(up);
                        if(cur_top) cur_top->read_unlock(cpuid);
                    }
                    newBlock->read_unlock(cpuid);

                    lower = newBlock;
                }
            }
            lvl--;
        }
        if(lower) lower->read_unlock(cpuid);
        unlock_all(blocks,cpuid);
        operations_completed.fetch_add(1, std::memory_order_relaxed);
        return 1;
    }

    void print_list(){
        Block* curr;
        for (int i = levels.size() - 1; i >= 0; i--)
        {
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

    bool search(thread_args *my_args)
    {
        readThreadParams* param = (readThreadParams *) malloc(sizeof(readThreadParams));
        param->args = my_args;

        int key = my_args->value;

        int cpuid = sched_getcpu();

        std::vector<Node *>::iterator it;
        Node *node;
        Node *prev_node;
        Block *block = levels[levels.size() - 1];

        if(block) read_lock(block->lock, cpuid);

        while (block)
        {
            for (it = block->vector.begin(); it != block->vector.end(); ++it)
            {
                node = *it;
                if (node->value < key)
                {                 
                    prev_node = node;
                    if (node == *std::prev(block->vector.end()))
                    {
                        if(block) read_unlock(block->lock, cpuid);
                        block = block->next;
                        if(block) read_lock(block->lock, cpuid);
                        break;
                    }
                    else
                    {
                        continue;
                    }
                }
                else if (node->value == key)
                {
                    if(block) read_unlock(block->lock, cpuid);
                    return true;
                }
                else if (key < node->value)
                {
                    if(block) read_unlock(block->lock, cpuid);
                    block = prev_node->down;
                    if(block) read_lock(block->lock, cpuid);
                    break;
                }
                // else if (i == 0) {return false;}
            }
        }
        // }
        operations_completed.fetch_add(1, std::memory_order_relaxed);
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

void* ReadThread(void *args){
    threadWrapper* my_args = static_cast<threadWrapper*>(args);
    BSkipList* list = static_cast<BSkipList*>(my_args->list);
    list->search(my_args->thread_args);
    pthread_exit(nullptr);
}

void* InsertThread(void *args){
    threadWrapper* my_args = static_cast<threadWrapper*>(args);
    BSkipList* list = static_cast<BSkipList*>(my_args->list);
    list->insert(my_args->thread_args);
    pthread_exit(nullptr);
}

thread_args* parse_data_from_txt(int n, string fname, double * output){
    thread_args * args = (thread_args *) malloc(n*sizeof(thread_args));
    cout << "reading the file " << endl;
    std::ifstream file(fname);
    if (!file.is_open()) {
        std::cerr << "Error opening file: " << fname << std::endl;
        return nullptr;
    }

    std::string line;
    for(int i=0; i< n ; i++){
        std::getline(file, line);

        std::istringstream iss(line);
        std::string text;
        int number;

        if(iss >> text >> number){
            args[i].thread_id = i;
            args[i].duration = output;
            args[i].value = number;
        }
    }
    return args;
}



int main(int argc, char **argv) {
    if (argc != 5) {
        std::cout << "Usage: ./BSkipList [num reader threads][num writer threads][reader_data][writer_data]\n";
        return 1;
    }

    //read inputs from command line
    int nreaders = atoi(argv[1]);
    int nwriters = atoi(argv[2]);
    std::string r_filename = argv[3];
    std::string w_filename = argv[4];
    
    // printf("Running benchmark with %u readers, %u writers, %u items, %u iterations, %u data entries\n", nreaders, nwriters, nitems, niters, n_data);

    //output buffer for reader threads
    //each iteration outputs one double of duration per thread
    //this double represents time in milliseconds taken using std::chrono::high_resolution_clock
    double * reader_output = (double *) malloc(sizeof(double));

    if (reader_output == NULL){
        perror("Malloc reader output");
        return -1;
    }

    //init reader args
    thread_args * reader_args = (thread_args *) malloc(nreaders*sizeof(thread_args));
    reader_args = parse_data_from_txt(nreaders, r_filename, reader_output);

    if (reader_args == NULL){
        perror("Malloc reader args");
        return -1;
    }

    pthread_t * readers = (pthread_t *) malloc(nreaders*sizeof(pthread_t));

    if (readers == NULL){
        perror("Malloc readers");
        return -1;
    }

    //initialize writers/writer output in same fashion as readers.
    double * writer_output = (double *) malloc(sizeof(double));

    if (writer_output == NULL){
        perror("Malloc writer output");
        return -1;
    }

    //init writer args + writers
    thread_args * writer_args = (thread_args *) malloc(nwriters*sizeof(thread_args));
    writer_args = parse_data_from_txt(nwriters, w_filename, writer_output);

    if (writer_args == NULL){
        perror("Malloc writer args");
        return -1;
    }

    pthread_t * writers = (pthread_t *) malloc(nwriters*sizeof(pthread_t));

    if (writers == NULL){
        perror("Malloc writers");
        return -1;
    }

    BSkipList* list = new BSkipList();
    
    // setup done, spawn threads
    auto start_time = std::chrono::high_resolution_clock::now();

    // just make a one more writer for deletion or distinguish inside of the file (iserting, deletion x)
    
    for (int i = 0; i < nwriters; i++){
            threadWrapper* args = (threadWrapper *) malloc(sizeof(threadWrapper));
            args->list = list;
            args->thread_args = &writer_args[i];
            
            // cout << "inserting " << writer_args[i].value << endl;
            if (pthread_create(&writers[i], NULL, InsertThread, (void *)args)){
                perror("pthread_create");
                return -1;
            }
    }

    for (int i = 0; i < nreaders; i++){
        threadWrapper* args = (threadWrapper *) malloc(sizeof(threadWrapper));

        args->list = list;
        args->thread_args = &reader_args[i];

        if (pthread_create(&readers[i], NULL, ReadThread, (void *)args)){
                perror("pthread_create");
                return -1;
        }
    }

    // join threads
    // in reverse order to try and force a collision between reader/writer threads.
    for (int i = 0; i < nwriters; i++){
        pthread_join(writers[i], NULL);
    }

    for (int i = 0; i < nreaders; i++){
        pthread_join(readers[i], NULL);
    }
    // list->print_list();

    auto end_time = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double> elapsed = end_time - start_time;
    double throughput = operations_completed.load() / elapsed.count(); 
    std::cout << "Throughput: " << throughput << " operations per second." << std::endl;

    //finally, calculate and print stats.
    printf("Threads done, stats:\n");

    double reader_mean = calculate_mean(reader_output, nreaders);
    double reader_std_dev = calculate_std_dev(reader_output, reader_mean, nreaders);
    double reader_min = calculate_min(reader_output, nreaders);
    double reader_max = calculate_max(reader_output, nreaders);

    printf("Readers: min %f ms, max %f ms, mean %f ms, std_dev %f\n", reader_min, reader_max, reader_mean, reader_std_dev);

    double writer_mean = calculate_mean(writer_output, nwriters);
    double writer_std_dev = calculate_std_dev(writer_output, writer_mean, nwriters);
    double writer_min = calculate_min(writer_output, nwriters);
    double writer_max = calculate_max(writer_output, nwriters);

    printf("Writers: min %f ms, max %f ms, mean %f ms, std_dev %f\n", writer_min, writer_max, writer_mean, writer_std_dev);

    //cleanup
    // list->free_locks();

    // free(list);

    // free(reader_output);
    // free(reader_args);
    // free(readers);

    // free(writer_output);
    // free(writer_args);
    // free(writers);

    // free(items);
    
    // for (int i = 0; i < nreaders; i++){
    //     free(reader_args[i].data);
    // }
    // for (int i = 0; i < nwriters; i++){
    //     free(writer_args[i].data);
    // }
    
    return 0;
}