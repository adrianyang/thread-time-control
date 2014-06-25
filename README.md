thread-time-control
===================

This is a framework for testing multi-threaded programs.

This program is written in C++, with a C-compiled static lib, and it aims to provide testing service to C/C++ codes under Linux (possibly, including other *nux systems) environment.

There are two main requirements (and also challenges) when testing multi-threaded programs. 
(1) monitoring all the threads, knowing if they are running ordinarily, or waiting for a lock, or have accquired the lock.
    This is implemented by interposition and tracing the system call, including the family of "pthread_" calls, so that we know the behaviors of every thread. Then, it becomes easy to tell if deadlock occurs, based on the running status (detected from the system calls).
(2) (partially) controling the schedule of some particular threads (or statements), so that for a certain test case, we always get the same expected result.
    This is implemented by implanting a condition variable at each sync point (illustrated later), with the help of GDB. Besides, the source codes of GDB is modified to support this requirement.
* sync point: defined by the users, typically indicating the running order. e.x. if the user set a sync point A at line a of file I, and also set another sync point B at line b of file II, then he could let A run before B, as the running order of A and B. After the program is operated, if the operating system schedules in a way such that B runs ahead of A, then, the statement of B will be suppressed to wait there until the statement of A has finished.

Unfortunately, due to some not-yet-known reasons, the child process forked in GDB and also running under the environment of GDB, fails to let its shared library find a vital global symbol. That means, the above two requirements could not be met in one testing framework, yet. Of course much work will be devoted to that in future.

This GitHub repository focuses on the second requirement -- partially scheduling control.

A complete implementation includes two parts:

(1) the main architecture. 
a. three-section unit-test-like testing structure: setup, test body, and teardown
b. test results generating and reporting
c. test cases running and monitoring
d. call the modified GDB API as illustrated in the following, acting as a GDB command line interpreter, though the commands are generated automatically, based on the test status.
e. communications with child (namely, the test case). This is for the above c and d.

(2) modified GDB as multi-threaded testing API.
a. binary symbol parsing
b. 'breakpoint' command
c. breakpoint information querying
d. 'run' command (divided into two parts for parent process and child process, respectively)
e. 'next' command (only used to set a momentary breakpoint)
f. 'continue' command
g. stop status querying
h. 'call' command (GDB does not yet support an synchronized function-call, which is a most important feature we need to let the tests run smoothly. Therefore, the implementation of a fundamental function used by 'call' command, is modified to support multi-threaded, namely, asynchronizing function call)
* all the modification is marked with the macro "MULTI_THREAD_TEST", and can be easily differentiated with "#ifdef" and "#ifndef"


[Known Imperfections]

1. Bad modification of GDB Makefile
I have very limited knowledge of GNU configure and automake. Currently, the building process of GDB lib is ugly, and some extra human work is needed to add the necessary modification after the the first 'make'.

2. Not all the code branches are covered, so that some trivial situation (typically, an error or exception) could triger GDB to try to type out something, and then the testing framework crashes.

3. The synchronization used by main architecture to inform the child that a sync point is reached, is not 100% strict. In very occasional situations, one sync would run one extra time than expected. 
A perfect solution for that is to use the latest amazing feature of GDB -- async mode. We will resolve this issue by setting the communication thread of the test-case process as 'never stop'. 

4. Lack of logging system.
I use "fprintf" and "std::cout/std::cerr" as logging. I know that is ugly.

[Usage]
1. include "thread_test.h" in your codes.
2. write a simple "main" function with the macro "RUN_THREAD_TESTS".
3. write a group-test class to define some setup and teardown behaviors, as you do in GoogleTest.
4. write your test case logic, within the acro body of "ThreadTest", as you write some "TEST_F..." in GoogleTest.
  the following steps are all written within the domain of "ThreadTest"
5. define your sync points with the macro format: SET_SYNCPOINT_AT_FILE(...).AT_LINENO(...).WITH_NAME(...);
6. define your sync order with the macro format:
  a. one-time checking:
    // run sync2 5 times, and then sync1 for 6 times, and then free run
    IN_SINGLE_LIST.ORDER_SYNCPOINT(sync2).AT_THREAD_ID(2).TIMES(5).ORDER_SYNCPOINT(sync1).AT_THREAD_ID(1).TIMES(6);
  b. cyclically checking:
    // run sync2 once, and then sync1 twice, and then sync2 once, and then sync1 twice, and so on and on
    IN_CIRCULAR_LIST.ORDER_SYNCPOINT(sync2).AT_THREAD_ID(2).TIMES(1).ORDER_SYNCPOINT(sync1).AT_THREAD_ID(1).TIMES(2);
    * if some threads quit, the checks of circular list for those threads will not end, and therefore deadlock could occur.
  Note that you may add as many check lists as you like, as long as you can make sure that they will not cause deadlock.
7. write the BEGIN_THREAD_TEST() macro.
8. write the CREATE_THREAD macro, and pass your create_thread function name as the only argument.
9. write the END_THREAD_TEST macro.


Many thanks to the authors and maintainers of GDB!
I am the only author for writing the main architecture and modifying GDB for multi-threaded testing use.
Should you have any questions or advice, please just feel free to contact me at adrian.y.dm@gmail.com 

