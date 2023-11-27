#if !defined(_GNU_SOURCE)
#define _GNU_SOURCE
#endif

#include "lock.h"

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

/**
 * Try to acquire a write lock and spin until the lock is available.
 * Spin on the writer mutex.
 * Once it is acquired, wait for the number of readers to drop to 0.
 */
void write_lock(ReaderWriterLock *rwlock) {
  // acquire write lock.
  while (__sync_lock_test_and_set(&rwlock->writer, 1))
    while (rwlock->writer != 0);
  //once acquired, wait on readers
  for(int i = 0; i < NUM_COUNTERS; i++){ 
    while (rwlock->readers[0] > 0);
  }
  return;
}

//Release an acquired write lock.
void write_unlock(ReaderWriterLock *rwlock) {
  __sync_lock_release(&rwlock->writer);
  return;
}

