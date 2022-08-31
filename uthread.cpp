#include "uthread.h"
#include "TCB.h"
#include <cassert>
#include <deque>
#include <map>

using namespace std;

typedef struct uthread_info {
  int num_threads;
  int quantum_usecs;
  int running_tid;
  bool interrupts_enabled;
  struct sigaction sig_act;
  TCB* threads[MAX_THREAD_NUM];
} uthread_info_t;


// Book-keeping structures ----------------------------------------------------

// global uthread library info
static uthread_info_t uthread_info;
static deque<int> available_tids;

static deque<TCB*> ready_queue;

// key for finished_map is the finished thread's tid
// value is the threads return pointer
static map<int, void*> finished_map;

// key is the tid of the thread that join_map member is waiting for
static map<int, TCB*> join_map;

// key is tid of suspended thread
static map<int, TCB*> suspend_map;

// Interrupt Management --------------------------------------------------------

// Start a countdown timer to fire an interrupt
static void startInterruptTimer() {
  // initialize itmerval structs needed for setitimer call
  struct itimerval it;
  it.it_value.tv_sec = uthread_info.quantum_usecs / 1000000;
  it.it_value.tv_usec = uthread_info.quantum_usecs % 1000000;
  it.it_interval = it.it_value;
  int res = setitimer(ITIMER_VIRTUAL, &it, NULL);  
  if (res == -1)
    cerr << "Error - failed to set interrupt timer" << endl;
} // startInterruptTimer()

// Block signals from firing timer interrupt
static void disableInterrupts() {
  assert(uthread_info.interrupts_enabled);
  uthread_info.interrupts_enabled = false;
  sigset_t imask;
  if (sigemptyset(&imask) == -1)
    cerr << "Error - failed to create empty signal mask" << endl;
  else if (sigaddset(&imask, SIGVTALRM) == -1)
    cerr << "Error - failed to add SIGVTALRM to signal mask" << endl;
  else if (sigprocmask(SIG_BLOCK, &imask, NULL) == -1)
    cerr << "Error - failed to disable SIGVTALRM" << endl;
} // disableInterrupts()

// Unblock signals to re-enable timer interrupt
static void enableInterrupts() {
  assert(! uthread_info.interrupts_enabled);
  uthread_info.interrupts_enabled = true;
  sigset_t imask;
  if (sigemptyset(&imask) == -1)
    cerr << "Error - failed to create empty signal mask" << endl;
  else if (sigaddset(&imask, SIGVTALRM) == -1)
    cerr << "Error - failed to add SIGVTALRM to signal mask" << endl;
  else if (sigprocmask(SIG_UNBLOCK, &imask, NULL) == -1)
    cerr << "Error - failed to enable SIGVTALRM" << endl;
} // enableInterrupts()

static void timer_handler(int signo) {
  // preempt current running thread, and switch to next thread in ready queue
  uthread_yield();
} // timer_handler()

// Queue Management ------------------------------------------------------------

// Add TCB to the back of the ready queue
void addToReadyQueue(TCB *tcb) {
  ready_queue.push_back(tcb);
} // addToReadyQueue()

// Removes and returns the first TCB on the ready queue
// NOTE: Assumes at least one thread on the ready queue
TCB* popFromReadyQueue() {
  assert(!ready_queue.empty());
  TCB *ready_queue_head = ready_queue.front();
  ready_queue.pop_front();
  return ready_queue_head;
} // popFromReadyQueue()

// Removes the thread specified by the TID provided from the ready queue
// Returns 0 on success, and -1 on failure (thread not in ready queue)
int removeFromReadyQueue(int tid) {
  for (deque<TCB*>::iterator iter = ready_queue.begin(); iter != ready_queue.end(); ++iter) {
    if (tid == (*iter)->getId()) {
      ready_queue.erase(iter);
      return 0;
    } // if
  } // for
  // Thread not found
  return -1;
} // removeFromReadyQueue()

// Helper functions ------------------------------------------------------------

// Switch to the next ready thread
static void switchThreads(TCB* tcb_old, TCB* tcb_new) {
  // NOTE: assumes that interrupts are disabled prior to calling switchThreads()
  assert(!uthread_info.interrupts_enabled);
  // increment old thread's quantum count
  tcb_old->increaseQuantum();
  // flag is used to differentiate between the return from getcontext call
  // and the moment when the thread is resumed
  volatile int flag = 0;
  int res = getcontext(&(tcb_old->_context));
  if (res == -1) {
    cerr << "Error - failed to save thread context" << endl;
  } // if
  // If flag == 1 then it was already set below so the thread should resume
  if (flag == 1) {
    return;
  } // if
  // set flag for when thread is resumed
  flag = 1;
  // update running_tid field in global uthread_info struct
  uthread_info.running_tid = tcb_new->getId();
  // reset timer and run next thread
  startInterruptTimer();
  setcontext(&(tcb_new->_context));
  assert(false); // should never reach here
} // switchThreads()

// Library functions -----------------------------------------------------------

// Starting point for thread. Calls top-level thread function
void stub(void *(*start_routine)(void *), void *arg) {
  // enable interrupts in case they were disabled during 
  // the context switch to the new thread
  enableInterrupts();
  // call the top-level thread funtion and then call uthread_exit after the
  // top-level function has returned
  void* result = start_routine(arg);
  uthread_exit(result);
} // stub()

int uthread_init(int quantum_usecs) {
  // Initialize any data structures
  uthread_info.num_threads = 0;
  uthread_info.quantum_usecs = quantum_usecs;
  uthread_info.interrupts_enabled = false;
  for (int i = 0; i < MAX_THREAD_NUM; i++) {
    uthread_info.threads[i] = nullptr;
    available_tids.push_back(i);
  } // for
  // Create a thread for the caller (main) thread.
  // Does not use uthread_create because it is already running
  // will have the thread id of 0
  int tid = available_tids.front();  
  assert(tid == 0);
  available_tids.pop_front();
  TCB* tcb = new TCB(tid, nullptr, nullptr, RUNNING);
  uthread_info.threads[tid] = tcb;
  uthread_info.num_threads ++;
  // Setup timer interrupt handler
  uthread_info.sig_act.sa_handler = timer_handler;
  uthread_info.sig_act.sa_flags = 0;
  int res = sigemptyset(&uthread_info.sig_act.sa_mask);
  if (res == -1 || (sigaction(SIGVTALRM, &uthread_info.sig_act, NULL) == -1)) {
    cerr << "Error - failed to set SIGVTALRM handler" << endl;
    return -1;
  } // if
  // Start timer interrupt
  startInterruptTimer();
  enableInterrupts();
  // Return 0 on success, -1 on failure
  return 0;
} // uthread_init()

int uthread_create(void* (*start_routine)(void*), void* arg) {
  assert(uthread_info.interrupts_enabled);
  // Disable timer interrupts to avoid context switch during critical area
  disableInterrupts();
  // Check to see if able to make thread
  if (uthread_info.num_threads >= MAX_THREAD_NUM) {
    cerr << "Error - there are already MAX_THREAD_NUM threads running" << endl;
    enableInterrupts();
    return -1;
  } // if
  // Create a new thread and add it to the ready queue
  assert(!available_tids.empty());
  int tid = available_tids.front();  
  available_tids.pop_front();
  TCB* tcb = new TCB(tid, start_routine, arg, READY);
  uthread_info.threads[tid] = tcb;
  uthread_info.num_threads ++;
  addToReadyQueue(tcb);
  enableInterrupts();
  // Return new thread ID on success
  return tid;
} // uthread_create()

int uthread_yield(void) {
  assert(uthread_info.interrupts_enabled);
  // running thread voluntarily gives up the processor
  // disable interrupts to avoid preemption during context switches
  disableInterrupts();
  // get TCB for current thread
  TCB* tcb = uthread_info.threads[uthread_self()];
  // obtain next ready thread from ready queue
  if (! ready_queue.empty()) {
    TCB* next_thread = popFromReadyQueue();
    // set current thread to READY state and place at end of ready queue
    tcb->setState(READY);
    addToReadyQueue(tcb);
    // switch to new thread
    switchThreads(tcb, next_thread);
    // set state to reflect running state
    tcb->setState(RUNNING);
  } else { // no ready threads so just resume with new quantum
    // increment current thread quantum
    tcb->increaseQuantum();
    // reset timer
    startInterruptTimer();
  } // else
  // enable interrupts 
  enableInterrupts();
  return 0;
} // uthread_yield()

int uthread_join(int tid, void **retval) {
  assert(uthread_info.interrupts_enabled);
  disableInterrupts();
  if (tid >= MAX_THREAD_NUM || tid < 0) { // make sure tid is valid
    cerr << "Error - tid does not exist" << endl;
    enableInterrupts();
    return -1;
  } else if (uthread_info.threads[tid] == nullptr) { 
    // If the thread specified by tid is already terminated, just return
    enableInterrupts();
    return 0;
  } else if (tid == uthread_self()) {
    cerr << "Error - thread trying to join self" << endl;
    enableInterrupts();
    return -1;
  } else if (join_map.count(tid)) {
    cerr << "Error - another thread is already waiting to join specified tid" << endl;
    enableInterrupts();
    return -1;
  } else if (! finished_map.count(tid)) { // thread trying to join has not finished
    if (ready_queue.empty()) {
      // no other threads are ready to run, as such the current thread cannot
      // block without the whole library blocking
      cerr << "Error - specified tid is not finished, but current thread cannot" 
           << " block to join specified tid as there are no ready threads" << endl;
      enableInterrupts();
      return -1;
    } else {
      // block until thread trying to join finishes
      // set state to block
      TCB* self_tcb = uthread_info.threads[uthread_self()];
      self_tcb->setState(BLOCK);
      // add self to join_map
      join_map.emplace(tid, self_tcb); 
      TCB* next_thread = popFromReadyQueue();
      // switch to a new thread
      switchThreads(self_tcb, next_thread);  
      assert(!uthread_info.interrupts_enabled);
      // thread specified by tid is ready to be joined
      // set state to reflect current thread's running state
      self_tcb->setState(RUNNING);
    } // else 
  } // else if 
  // Set *retval to be the result of thread specified by tid 
  *retval = finished_map.at(tid); 
  // cleanup thread specified by tid
  finished_map.erase(tid);
  delete uthread_info.threads[tid];
  uthread_info.threads[tid] = nullptr;
  // add tid back to available queue
  available_tids.push_back(tid);
  uthread_info.num_threads --;

  enableInterrupts();
  return 0;
} // uthread_join()

void uthread_exit(void *retval) {
  assert(uthread_info.interrupts_enabled);
  disableInterrupts();
  // If this is the main thread, exit the program
  int tid = uthread_self();
  if (tid == 0) { // uthread_exit called on main thread -- exit program
    exit(0);
  } // if
  // Move any thread joined on this thread back to the ready queue
  if (join_map.count(tid)) {
    TCB* join_thread = join_map.at(tid);
    join_map.erase(tid);
    join_thread->setState(READY);
    addToReadyQueue(join_thread);
  } // if
  // Move this thread to the finished map 
  TCB* this_thread = uthread_info.threads[tid]; 
  this_thread->setState(FINISHED);
  finished_map.emplace(tid, retval);
  // switch to next ready thread
  assert(!ready_queue.empty());
  TCB* next_thread = popFromReadyQueue();
  switchThreads(this_thread, next_thread); 
  assert(false); // should never be scheduled again
} // uthread_exit()

int uthread_suspend(int tid) {
  assert(uthread_info.interrupts_enabled);
  if (tid >= MAX_THREAD_NUM || tid < 0) {
    cerr << "Error - invalid tid" << endl;
    return -1;
  } // if
  disableInterrupts();
  // Move the thread specified by tid from whatever state it is
  // in to the block queue
  if (tid == uthread_self()) { 
    if (ready_queue.empty()) {
      cerr << "Error - Attempted to suspend only runnable thread" << endl;
      enableInterrupts();
      return -1;
    } // if
  } else if (removeFromReadyQueue(tid) == -1){ // not in ready queue and not running
    cerr << "Error - attempting to suspend an already blocked or finished thread" << endl;
    enableInterrupts();
    return -1;
  } // else if 
  // tid was in ready queue but has been removed, or tid is current thread
  TCB* next_thread = popFromReadyQueue();
  TCB* tcb = uthread_info.threads[tid];
  // move tid to blocked state
  tcb->setState(BLOCK);
  // add to the suspend map where the key is its own tid
  suspend_map.emplace(tid, tcb);
  // switch to next ready thread
  switchThreads(tcb, next_thread);
  // set state to reflect running state
  tcb->setState(RUNNING);
  enableInterrupts();
  return 0;
} // uthread_suspend()

int uthread_resume(int tid) {
  assert(uthread_info.interrupts_enabled);
  if (tid >= MAX_THREAD_NUM || tid < 0)
    return -1;
  disableInterrupts();
  // Move the thread specified by tid back to the ready queue from suspend map 
  // if thread is not suspended, nothing happens
  if (suspend_map.count(tid)) {
    TCB* resume_thread = suspend_map.at(tid);
    suspend_map.erase(tid);
    resume_thread->setState(READY);
    addToReadyQueue(resume_thread);
  } // if
  enableInterrupts();
  return 0;
} // uthread_resume()

int uthread_self() {
  return uthread_info.running_tid;
} // uthread_self()

int uthread_get_total_quantums() {
  assert(uthread_info.interrupts_enabled);
  disableInterrupts();
  int total = 0;
  TCB* tcb;
  // iterate through all threads
  for (int i = 0; i < MAX_THREAD_NUM; i++) {
    tcb = uthread_info.threads[i];
    if (tcb != nullptr)
      total += tcb->getQuantum();
  } // for
  enableInterrupts();
  return total;
} // uthread_get_total_quantums()

int uthread_get_quantums(int tid) {
  assert(uthread_info.interrupts_enabled);
  disableInterrupts();
  if (tid >= MAX_THREAD_NUM || tid < 0 || uthread_info.threads[tid] == nullptr)
    return -1;
  int quantums = uthread_info.threads[tid]->getQuantum(); 
  enableInterrupts();
  return quantums;
} // uthread_get_quantums()
