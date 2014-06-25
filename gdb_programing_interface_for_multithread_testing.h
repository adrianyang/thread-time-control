/*
	INTRO: header file as the API for (only?) multithread testing program
				 to access gdb operation, including:
				 (1) reading and parsing ELF & DWARF info of an executable file
						 (compiled with '-g'), especially,
						 a. FILE:LINE to CORE_ADDR
						 b. symbol info (read-only, used for EXPECT/ASSERT) *
				 (2) setting breakpoints (ptrace), and injecting user-defined functions
						 there.
				 (3) setting child as PTRACE_TRACEME, and call "raise(SIGTRAP)"
				 (4) calling GDB "next" command to know when a statement is finished.

	CURRENT GOAL AND USAGE:
				 (1) setting sync-points (breakpoints) and index (users provide a unique
						 index for each breakpoint)
				 (2) setting running order for sync-points 
	
	FUTURE TARGET/FEATURE:
				 (1) setting watching points(EXPECT/ASSERT) for any variable at any
						 statement.
				 (2) conditional sync-points (could be complex conditions)

	IMPLEMENTATION SUMMARY:
				 (1) GDB preparation work (before "running"):
						 a. parsing ELF & DWARF info from the file indicated by "argv[0]"
						 b. building map from user-set sync-points (FILE:LINE) to the 
								addresses (CORE_ADDR); maintain the sync-points with the index
								provided by users; check if the index is available (unique).
						 c. implementing "breakpoints": (TODO: check this) replace the IP
								of the entrance with a special function address. (TODO: how
								to set arguments and return value?)
				 (2) once the child process (the process running the multithread 
						 program) is running, we need GDB no more. Namely, the parent
						 multithread test process will take full control.

	IMPLEMENTATION DETAILS:
	(including GDB & ThreadTest)
				 Notation: we aim to let the parent process of the multithread-testing
						 program work interactively with the GDB "working" part. Namely, the 
						 parent process plays the role of "GDB console", via "sending"
						 commands (actually, calling the GDB working functions), and also 
						 waiting there, monitoring the status of the child process. 

				 P.S. GDB keeps almost everything important as globle/static. Technically,
						 the child process must inherit everything from its parent.
					
				 (1) initialization:
						 in ThreadTestContainer::InitGDB(), call api to do all the necessary 
						 things before GDB begins receiving any commands.
				
						 a. the parent process calling the api;
						 
						 b. GDB doing the preparation;
						 
						 c. the parent process taking over the control again, and going on to 
								Run()

				 (2) setting breakpoints/watchpoints:
						 implemented at the ThreadTest side with a serial of macros: SET_...
						 sending src-file-name & line-num info to GDB, by calling the "break"
						 command.

						 P.S. 1) GDB holds all the knowledge about the breakpoints. The parent
						 process only cares about the map between the user-defined bk-name
						 and the GDB-fedback bk-index. Api will do the work for fetching the 
						 proper breakpoint info from GDB for the parent process with the help
						 of the user-defined bk-name.

						 P.S. 2) user-defined bk-name is a unique name (within the field of a
						 typical multithread testing file) to guide the users to recognize 
						 their settings, because the users could not set breakpoints in a
						 command-line-like interactive way, and no breakpoint info could be
						 shown on the screen while the users are writing their tests.

						 a. the parent process collecting all the breakpoints before running
						 the test (forking the child process), just like what EXPECT_/ASSERT_
						 does. A difference is that a unique name must be set for each
						 breakpoint, as illustrated above.

						 b. the parent process calling the api;

						 c. GDB doing the "break" operations;

						 d. the parent process taking over the control again, and going on to
								look for breakpoint-ordering-setting macros.

				 (3) running the child process:
						 running the tested codes. The parent process forks the child, and
						 notifies the PID to GDB. Then, the parent (main thread; other threads,
						 including communication threads with child, and one monitoring thread,
						 stay the same as the previous implementation) waits there, and 
						 receives status feedbacks from the child. Once a breakpoint is 
						 hit, the parent will be notified with a related status. The parent
						 has to then cooperate with GDB to determine which breakpoint it is (
						 TODO: refer to the logic of GDB "wait"). If the current context 
						 statisfies the condition of moving on, "continue"; otherwise, keep
						 waiting.

						 a. the parent process calling fork();

						 b. the parent process passing the child PID to GDB;

						 c. the newly created child process calling ptrace-PTRACE_TRACEME, and
								calling raise(SIGCONT);
                ***************************************************************
								a more graceful way is that the parent process calling ptrace-
								PTRACE_ATTACH, and the child process waits until that is done. (
								TODO: synchronized with what? semaphore?) In this way, few changes 
								are needed on the child process side.

						 d. the parent waiting there, monitoring the child's changes of status;

						 e. when a breakpoint is hit, calling api to ask GDB for the breakpoint
								info. If the sync-point could move on, go to step (4) to call "next"
								command; otherwise, go to substep (f);

						 f. suspend the current sync-point when it must wait all the 
								prerequisites (the other sync-points); injecting a function in the 
								child process, which uses a condition variable to ganrantee that
								the prerequisties are satisfied. After that, the child process, or
								more exactly, the specific thread going back on track.
								(TODO: how to inject with ptrace? PTRACE_POKETEXT)

						 * deadlock detection: improper sync-points could lead to unwanted 
							 deadlocks. The deadlock detection logic (the monitoring thread of 
							 the parent process) needs to take care of this, and report it as 
							 another kind of bug.

				 (4) "next", dealing with hit breakpoints:
						 any sync-point is run by calling "next" command to GDB. Then, when the
						 next (internal momentary) breakpoint is hit, the child process (or 
						 more exactly, the specific thread) broadcasts to set free all the 
						 suspending waits. The parent process needs to ignore this activity.

						 parent process sending "call" command to GDB
				
				 (5) deinitialization:
						 TODO: see what GDB needs us to do the last deinitialization work

				 data structure:
				 the child process needs a global struct/class to register the 
				 sync-points status.
				 sample:
				 class SyncPointStatus {
					public:
					 // sync-point status;
					 // lock: mutex & condition variable
				 }; 

*/

#ifndef __GDB_PROGRAMING_INTERFACE_FOR_MULTITHREAD_TESTING_H__
#define __GDB_PROGRAMING_INTERFACE_FOR_MULTITHREAD_TESTING_H__

// leave the dependent headers in the source file

typedef enum ARG_TYPE_t {
	ARG_TYPE_INTEGER,
	ARG_TYPE_ADDR,
} ARG_TYPE;

typedef union ARG_UNION_t {
	void* addr;
	int digit;
} ARG_VALUE;

int MakeGDBDoPreparation(const char* prog_name);

int SetBreakPoint(const char* break_point);

int GetNewlyAddedGdbBreakpoint(void);

int RunChild(void);

int RunParent(void);

int Next(void);

int Continue(void);

int ClearBreakpoints(void);

int QueryStatus(int* break_point, int* thread_num);

// use global variables instead of arguments?
int CallFunc(const char* func_name, int num_args, ARG_TYPE arg_type[], ARG_VALUE arg_value[]);

int DelAllBk(void);

int Recover(int tid);




#endif
