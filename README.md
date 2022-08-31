Run solution for main.cpp: ./uthread-solution-exe 100000000 8

## Final Submission Comments
To test the functionality of the uthread library, run the following commands
```
> make uthread-test
> ./uthread-test 20 10
```
This will use 10 threads to compute the 21st - 30th fibonacci numbers, along with
a handful of other tests. The programs out lists the expected and actual results
of all of the tests. For explanations of why tests should produce the expected results,
view the implementation file `uthread-test.cpp` which contains detailed documentation
for each test.

Output from a run of the uthread-test is shown below:

```
[anthony@ <<23:44:10>>]$ ./uthread-test 20 10
++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
Testing uthread_init and uthread_self

Expected thread ID: 0		uthread_self: 0
--------------------------------------------------------------------------------
++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
Testing uthread_get_total_quantums and uthread_get_quantums

Looping for 2 seconds (in real time) to build up quantums

Expecting uthread_get_total_quantums and uthread_get_quantums
to be equal or off by one if an interrupt happens between calls
to uthread_get_quantums and uthread_get_total_quantums


Main thread quantums: 225		total quantums: 225
--------------------------------------------------------------------------------
++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
Testing uthread_create and uthread_join

Creating 10 threads
Thread ID: 1          fib(21) = 10946          Expected: 10946
Thread ID: 2          fib(22) = 17711          Expected: 17711
Thread ID: 3          fib(23) = 28657          Expected: 28657
Thread ID: 4          fib(24) = 46368          Expected: 46368
Thread ID: 5          fib(25) = 75025          Expected: 75025
Thread ID: 6          fib(26) = 121393          Expected: 121393
Thread ID: 7          fib(27) = 196418          Expected: 196418
Thread ID: 8          fib(28) = 317811          Expected: 317811
Thread ID: 9          fib(29) = 514229          Expected: 514229
Thread ID: 10          fib(30) = 832040          Expected: 832040
--------------------------------------------------------------------------------
++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
Testing uthread_yield

There should be no back-to-back (A) (B) messages from the same thread 
without another thread's message in between:

Created thread
Created thread
Created thread
Created thread
Created thread
Created thread
Created thread
Created thread
Created thread
Created thread
(A) Thread ID: 11
(A) Thread ID: 12
(A) Thread ID: 13
(A) Thread ID: 14
(A) Thread ID: 15
(A) Thread ID: 16
(A) Thread ID: 17
(A) Thread ID: 18
(A) Thread ID: 19
(A) Thread ID: 20
(B) Thread ID: 11
(B) Thread ID: 12
(B) Thread ID: 13
(B) Thread ID: 14
(B) Thread ID: 15
(B) Thread ID: 16
(B) Thread ID: 17
(B) Thread ID: 18
(B) Thread ID: 19
(B) Thread ID: 20
--------------------------------------------------------------------------------
++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
Testing uthread_suspend and uthread_resume

Creating 2 new threads

One thread will suspend itself while another resumes the
suspended thread 2 seconds later

Thread ID: 21 is suspending itself

Thread ID: 22 will loop for 2 seconds before resuming thread 21

Thread ID: 22 is resuming thread 21

Thread ID: 21 is running again
--------------------------------------------------------------------------------
++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
Testing uthread_exit

Note that uthread_exit is used extensively in the tests above...
Anytime a thread was joined, it first called uthread_exit from its stub

Further testing:

Expecting threads with an even tid to return true (1)
and threads with odd tid to return false (0)

Creating 10 new threads

Thread ID: 23          return value: 0
Thread ID: 24          return value: 1
Thread ID: 25          return value: 0
Thread ID: 26          return value: 1
Thread ID: 27          return value: 0
Thread ID: 28          return value: 1
Thread ID: 29          return value: 0
Thread ID: 30          return value: 1
Thread ID: 31          return value: 0
Thread ID: 32          return value: 1

We will now call uthread_exit on the main thread after printing
11 numbers in an otherwise infinite while loop:
  1
  2
  3
  4
  5
  6
  7
  8
  9
  10
  11
```
