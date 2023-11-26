 #ifndef _LOCK_H_
#define _LOCK_H_

#include <stdlib.h>
#include <inttypes.h>
#include <stdio.h>
#include <unistd.h>
#include <stdbool.h>
#include "BSkipList.cpp"

#ifdef __cplusplus
#define __restrict__
extern "C" {
#endif

  typedef struct ReaderWriterLock {
    volatile int writer;
    volatile int64_t* readers;
  } ReaderWriterLock;


  //All lock acq and free operations require that the lock state not be acquired/free
  // by this thread before the call, undefined behavior if this is not the case
  // The benchmark does not test this precondition.

  //init the lock and any associated resources
  void rw_lock_init(ReaderWriterLock *rwlock);

  /*
  All the inputs should be changed to BSkipList block
  */
  //called by reader thread to acquire the lock
  //stall on the lock, should guarantee eventual progress
  //thread_id is the CPU ID of the calling thread - for CADE, this is between 0-7
  //may not be necessary for your implementation but this value is passed to the lock by the benchmark.
  bool read_lock(Block *block, uint8_t thread_id, int key);

  //unlock a reader thread that has acquired the lock
  void read_unlock(Block *block, uint8_t thread_id);

  //acquire a write lock
  //stall until lock is acquired - guarantee eventual progress.
  void write_lock(Block *block, int key);

  //free an acquired write lock
  void write_unlock(Block *block);

#ifdef __cplusplus
}
#endif



#endif
