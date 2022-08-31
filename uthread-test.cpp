#include "uthread.h"
#include <iostream>
#include <iomanip>
#include <cassert>
#include <unistd.h>
#include <time.h>

using namespace std;

unsigned long long int* fibs;

void gen_fibs(int n){
  fibs[0] = 0;
  if (n == 0)
    return;
  fibs[1] = 1;
  if (n == 1)
    return;
  for (int i = 2; i < n; i++) {
    fibs[i] = fibs[i-2] + fibs[i-1];
  } // for
} // gen_fibs

void* find_fib(void* arg) {
  int offset = *(int*) arg;
  int n = uthread_self() + offset;
  assert(n >= 0);
  if (n == 0 || n == 1)
    return new unsigned long long int(n);
  unsigned long long int fib[n + 1];
  fib[0] = 0;
  fib[1] = 1;
  for (int i = 2; i <= n; i++) {
    fib[i] = fib[i-2] + fib[i-1];
  } // for
  return new unsigned long long int(fib[n]);
} // find_fib()

void* yield_test(void* arg) {
  int tid = uthread_self();
  cerr << "(A) Thread ID: " << tid << endl; 
  int res = uthread_yield();
  if (res != 0) {
    cerr << "uthread_yield failed" << endl;
    exit(1);
  } // if
  cerr << "(B) Thread ID: " << tid << endl; 
  return nullptr;
} // yield_test()

void* suspend_test(void* arg) {
  cerr << "\nThread ID: " << uthread_self()  
       << " is suspending itself" << endl;
  int res = uthread_suspend(uthread_self());
  if (res != 0) {
    cerr << "uthread_suspend failed" << endl;
    exit(1);
  } // if
  cerr << "\nThread ID: " << uthread_self()  
       << " is running again" << endl;
  return nullptr;
} // suspend_test()

void* resume_test(void* arg) {
  int sus_tid = *(int*) arg;
  // loop for 2 seconds before resuming suspended thread 
  cerr << "\nThread ID: " << uthread_self()  
       << " will loop for 2 seconds before resuming thread " << sus_tid << endl;
  time_t time_s, time_c;
  time(&time_s);
  while(1) {
    time(&time_c);
    if (time_c - time_s >= 2)
      break;
  } // while
  // resume suspended thread
  cerr << "\nThread ID: " << uthread_self()
       << " is resuming thread " << sus_tid << endl;
  int res = uthread_resume(sus_tid);
  if (res != 0) {
    cerr << "uthread_resume failed" << endl;
    exit(1);
  } // if
  return nullptr;
} // resume_test()

void* exit_test(void* arg) {
  int tid = uthread_self();
  if (tid % 2 == 0)
    return new bool(true);
  return new bool(false);
} // yield_test()

int main(int argc, char *argv[]) {
  // Default to 1 ms time quantum
  int quantum_usecs = 1000;

  if (argc < 3) {
    cerr << "Usage: ./uthread-test <fib offset> <threads> [quantum_usecs]" << endl;
    exit(1);
  } else if (argc == 4) {
    quantum_usecs = atoi(argv[3]);
  } // else if
  
  int* fib_offset = new int(atoi(argv[1]));
  int num_threads = atoi(argv[2]);
  fibs = new unsigned long long int[*fib_offset + MAX_THREAD_NUM];
  gen_fibs(*fib_offset + MAX_THREAD_NUM);

  /* Testing uthread_init, uthread_self ------------------------------ */
  cerr << setw(80) << setfill('+') << "" << endl;
  cerr << "Testing uthread_init and uthread_self\n" << endl;

  int res = uthread_init(quantum_usecs);
  if (res != 0) {
    cerr << "uthread_init failed" << endl;
    exit(1);
  } // if

  // Only one thread--the main thread--should exist
  // this thread should have a tid of 0
  int main_tid = uthread_self();
  cerr << "Expected thread ID: 0\t\t" << "uthread_self: " << main_tid << endl;
  assert(main_tid == 0);

  cerr << setw(80) << setfill('-') << "" << endl;

  /* Testing uthread_get_total_quantums and uthread_get_quantums -------- */
  cerr << setw(80) << setfill('+') << "" << endl;
  cerr << "Testing uthread_get_total_quantums and uthread_get_quantums\n" << endl;

  // since there is only one thread the main threads quantums should 
  // equal the library's total quantums
  
  // loop for 2 seconds while to build up quantums
  cerr << "Looping for 2 seconds (in real time) to build up quantums\n" << endl;
  time_t time_s, time_c;
  time(&time_s);
  while(1) {
    time(&time_c);
    if (time_c - time_s >= 2)
      break;
  } // while
    
  cerr << "Expecting uthread_get_total_quantums and uthread_get_quantums\n"
       << "to be equal or off by one if an interrupt happens between calls\n" 
       << "to uthread_get_quantums and uthread_get_total_quantums\n\n" << endl; 
  int quantums = uthread_get_quantums(uthread_self());
  int total_quantums = uthread_get_total_quantums();
  cerr << "Main thread quantums: " << quantums 
       << "\t\ttotal quantums: " << total_quantums << endl; 

  cerr << setw(80) << setfill('-') << "" << endl;

  /* Testing uthread_create and uthread_join--------------------------------- */
  cerr << setw(80) << setfill('+') << "" << endl;
  cerr << "Testing uthread_create and uthread_join\n" << endl;

  // create threads
  cerr << "Creating " << num_threads << " threads" << endl;
  int* thread_ids = new int[num_threads];
  for (int i = 0; i < num_threads; i++) {
    thread_ids[i] = uthread_create(find_fib, (void *) fib_offset); 
  } // for

  // reference fib numbers
  // join threads
  for (int i = 0; i < num_threads; i++) {
    unsigned long long int* fib_res = nullptr;
    int n = *fib_offset + thread_ids[i];
    res = uthread_join(thread_ids[i], (void**)&fib_res);
    if (res != 0) {
      cerr << "uthread_join failed" << endl;
      exit(1);
    } // if

    cerr << "Thread ID: " << thread_ids[i] 
         << setw(10) << setfill(' ') << " " 
         << "fib(" << n << ") = " << *fib_res
         << setw(10) << " "
         << "Expected: " << fibs[n] << endl;

    // free thread result
    if (fib_res != nullptr)
      delete fib_res;
  } // for

  // cleanup
  delete fib_offset;
  delete [] fibs;
  delete [] thread_ids;

  cerr << left << setw(80) << setfill('-') << "" << endl;

  /* Testing uthread_yield ------------------------------------------ */
  cerr << setw(80) << setfill('+') << "" << endl;
  cerr << "Testing uthread_yield\n" << endl;

  // threads will print a first (A) message then yield
  // upon resuming, threads will print a second (B) message

  // we expect that there will always be a context
  // switch inbetween the (A) message and (B) message 
  // As such, there should always be a different threads message between
  // one threads (A) and (B) messages
  cerr << "There should be no back-to-back (A) (B) messages from the same thread "
       << "\nwithout another thread's message in between:\n" << endl;

  int yield_num_threads = 10;
  thread_ids = new int[yield_num_threads];
  for (int i = 0; i < yield_num_threads; i++) {
    thread_ids[i] = uthread_create(yield_test, nullptr); 
    cerr << "Created thread" << endl;
  } // for
  
  // join threads
  for (int i = 0; i < yield_num_threads; i++) {
    void* yield_res = nullptr;
    res = uthread_join(thread_ids[i], (void**)&yield_res);
    if (res != 0) {
      cerr << "uthread_join failed" << endl;
      exit(1);
    } // if
  } // for

  // cleanup
  delete [] thread_ids;

  cerr << setw(80) << setfill('-') << "" << endl;

  /* Testing uthread_suspend and uthread_resume ----------------------------- */
  cerr << setw(80) << setfill('+') << "" << endl;
  cerr << "Testing uthread_suspend and uthread_resume" << endl;

  // one thread will suspend itself immediately
  // while another thread will wait for 2 seconds before resuming
  // the suspended thread

  // create two threads for test
  cerr << "\nCreating 2 new threads\n" << endl;

  cerr << "One thread will suspend itself while another resumes the"
       << "\nsuspended thread 2 seconds later" << endl;

  int* sus_tids = new int[2];
  sus_tids[0] = uthread_create(suspend_test, nullptr);
  sus_tids[1] = uthread_create(resume_test, (void *) &sus_tids[0]);

  // join threads
  for (int i = 0; i < 2; i++) {
    void* sus_res = nullptr;
    res = uthread_join(sus_tids[i], (void**) &sus_res);
    if (res != 0) {
      cerr << "uthread_join failed" << endl;
      exit(1);
    } // if
  } // for

  // cleanup
  delete [] sus_tids;
  
  cerr << setw(80) << setfill('-') << "" << endl;
  
  /* Testing uthread_exit --------------------------------------------------- */
  cerr << setw(80) << setfill('+') << "" << endl;
  cerr << "Testing uthread_exit\n" << endl;

  cerr << "Note that uthread_exit is used extensively in the tests above..."
       << "\nAnytime a thread was joined, it first called uthread_exit from its stub\n"
       << endl;

  cerr << "Further testing:\n\n" 
       << "Expecting threads with an even tid to return true (1)"
       << "\nand threads with odd tid to return false (0)\n" << endl; 

  // create threads
  cerr << "Creating " << num_threads << " new threads\n" << endl;
  thread_ids = new int[num_threads];
  for (int i = 0; i < num_threads; i++) {
    thread_ids[i] = uthread_create(exit_test, nullptr); 
  } // for
  
  // join threads
  for (int i = 0; i < num_threads; i++) {
    bool* exit_res = nullptr;
    res = uthread_join(thread_ids[i], (void**) &exit_res);
    if (res != 0) {
      cerr << "uthread_join failed" << endl;
      exit(1);
    } // if

    cerr << "Thread ID: " << thread_ids[i] 
         << setw(10) << setfill(' ') << " " 
         << "return value: " << *exit_res << endl;

    // free thread result
    if (exit_res != nullptr)
      delete exit_res;
  } // for

  // cleanup
  delete [] thread_ids;

  cerr << "\nWe will now call uthread_exit on the main thread after printing\n"
       << "11 numbers in an otherwise infinite while loop:" << endl;

  int i = 1;
  while (1) {
    cerr << "  " << i++ << endl;
    if (i == 12)
      uthread_exit(nullptr);
  } // while
} // main()
