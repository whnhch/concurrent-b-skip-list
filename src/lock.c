#if !defined(_GNU_SOURCE)
#define _GNU_SOURCE
#endif

#include "lock.h"
#include "BSkipList.cpp"

#define NUM_COUNTERS 16

//init the lock
// called by the thread running the benchmark before any child threads are spawned.
void rw_lock_init(ReaderWriterLock *rwlock) {
  rwlock->readers = (int64_t*)malloc(sizeof(int64_t) * 16);
  //memset(rwlock->readers, 0, sizeof(rwlock->readers));
  rwlock->writer = 0;
}

/**
 * Try to acquire a lock and spin until the lock is available.
 * Readers should add themselves to the read counter,
 * then check if a writer is waiting
 * If a writer is waiting, decrement the counter and wait for the writer to finish
 * then retry
 */
 //Change the argument to block
bool read_lock(Block *block, uint8_t thread_id, int key) {
  //acq read lock
  while (true){

    //atomic_add_fetch returns current value, but not needed
    __atomic_add_fetch(&block->rwlock->readers[thread_id], 1, __ATOMIC_SEQ_CST);


    if (rwlock->writer){
      //cancel
      __atomic_add_fetch(&block->rwlock->readers[thread_id], -1, __ATOMIC_SEQ_CST);
      //wait
      while (rwlock->writer);      
    } else {
      //there is no writer before time to read.
      //Do BskipList
      return range(key);
    }
  }
}

//release an acquired read lock for thread `thread_id`
void read_unlock(Block *block, uint8_t thread_id) {
  __atomic_add_fetch(&block->rwlock->readers[thread_id], -1, __ATOMIC_SEQ_CST);
  return;
}

/**
 * Try to acquire a write lock and spin until the lock is available.
 * Spin on the writer mutex.
 * Once it is acquired, wait for the number of readers to drop to 0.
 */
void write_lock(Block *block, int key) {
  // acquire write lock.
  while (__sync_lock_test_and_set(&rwlock->writer, 1))
    while (block->rwlock->writer != 0);
  //once acquired, wait on readers
  for(int i = 0; i < NUM_COUNTERS; i++){ 
    while (block->rwlock->readers[0] > 0);
  }
  //Do write
  return;
}

//Release an acquired write lock.
void write_unlock(Block *block) {
  __sync_lock_release(&block->rwlock->writer);
  return;
}

