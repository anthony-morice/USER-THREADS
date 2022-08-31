#include "TCB.h"

/**
 * Constructor for TCB. Allocate a thread stack and setup the thread
 * context to call the stub function
 * @param tid id for the new thread
 * @param f the thread function that get no args and return nothing
       * @param arg the thread function argument
 * @param state current state for the new thread
 */
TCB::TCB(int tid, void *(*start_routine)(void* arg), void *arg, State state) {
  // initialize all member variables
  _tid = tid;
  _quantum = 0;
  _state = state;
  // allocate a thread stack
  _stack = new char[STACK_SIZE];
  // get the current execution context to initialize _context
  int res = getcontext(&_context);
  if (res == -1) {
    std::cerr << "Error - failed to get context in TCB constructor" << std::endl;
  } // if
  _context.uc_stack.ss_sp = _stack; 
  _context.uc_stack.ss_size = STACK_SIZE;
  _context.uc_stack.ss_flags = 0;
  // create initial thread context which points to stub
  makecontext(&_context, (void(*)()) stub, 2, start_routine, arg);
} // TCB()

TCB::~TCB() {
  // free dynamically allocated _stack member
  delete [] _stack;
} // ~TCB()

void TCB::setState(State state) {
  _state = state;
} // setState()

State TCB::getState() const {
  return _state;
} // getState()

int TCB::getId() const {
  return _tid;
} // getId()

void TCB::increaseQuantum() {
  _quantum ++;
} // increaseQuantum()

int TCB::getQuantum() const {
  return _quantum;
} // getQuantum()
