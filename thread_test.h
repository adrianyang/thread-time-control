#ifndef __THREAD_TEST_H__
#define __THREAD_TEST_H__

#include <stdlib.h>

#include "thread_def_common.h"
#include "thread_test_impl.h"
#include "cmdline_parser.h"
/*

structure:

class ThreadTestSimpleExample : public ThreadTestGroup {
 protected:
  virtual int SetUpEnv() {
  }
  virtual int TearDownEnv() {
  }
  // ...
};

THREAD_TEST(ThreadTestSimpleExample, Case1) {
  SET_THREAD_ACTION(...); // optional
  SET_THREAD_ATTR(...);   // optional
	EXPECT_FUNC(...).ARG(...).AT_COND(...).AFTER.SIGNAL;			// optional
	// I have to give up this idea of comparing directly, for lack of 
	// proper solution. (I guess I need to find out some inspiration from GMock)
	EXPECT_THAT(...).IS(...).AT_MUTEX(...).BEFORE.LOCK;       // optional
  BEGIN_THREAD_TEST(); 
  CREATE_THREAD(...);
  END_THREAD_TEST();
}

THREAD_TEST(ThreadTestSimpleExample, Case2) {
  SET_THREAD_ACTION(...); // optional
  SET_THREAD_ATTR(...);   // optional
	ASSERT_FUNC(...).ARG(...).AT_COND(...).AFTER.SIGNAL;      // optional
	// I have to give up this idea of comparing directly, for lack of 
  // proper solution.
  ASSERT_THAT(...).IS(...).AT_MUTEX(...).BEFORE.LOCK;       // optional
  BEGIN_THREAD_TEST(); 
  CREATE_THREAD(...);
  END_THREAD_TEST();
}

int main(int argc, char** argv) {
  RUN_THREAD_TESTS(argc, argv);

  return 0;
}

********************************************************

structure version 2:

class ThreadTestSimpleExample : public ThreadTestGroup {
 protected:
  virtual int SetUpEnv() {
		SET_SYNCPOINT_AT_FILE(src_file1).AT_LINENO(line1).WITH_NAME(sync_ptr1);
		SET_SYNCPOINT_AT_FILE(src_file2).AT_LINENO(line2).WITH_NAME(sync_ptr2);
  }
  virtual int TearDownEnv() {
  }
  // ...
};

THREAD_TEST(ThreadTestSimpleExample, Case1) {
  SET_SYNCPOINT_AT_FILE(src_file3).AT_LINENO(line3).WITH_NAME(sync_ptr3);
  SET_SYNCPOINT_AT_FILE(src_file4).AT_LINENO(line4).WITH_NAME(sync_ptr4);
	IN_SINGLE_LIST.ORDER_SYNCPOINT(sync_ptr1).AT_THREAD_ID(1).TIMES(2).ORDER_SYNCPOINT(sync_ptr3).AT_THREAD_ID(3);
	IN_SINGLE_LIST.ORDER_SYNCPOINT(sync_ptr2).AT_THREAD_ID(1).TIMES(2).ORDER_SYNCPOINT(sync_ptr4).AT_THREAD_ID(4);
	BEGIN_THREAD_TEST(); 
  CREATE_THREAD(...);
  END_THREAD_TEST();
}

THREAD_TEST(ThreadTestSimpleExample, Case2) {
  SET_SYNCPOINT_AT_FILE(src_file3).AT_LINENO(line5).WITH_NAME(sync_ptr5);
  SET_SYNCPOINT_AT_FILE(src_file4).AT_LINENO(line6).WITH_NAME(sync_ptr6);
	IN_CIRCULAR_LIST.ORDER_SYNCPOINT(sync_ptr1).AT_THREAD_ID(1).TIMES(2).ORDER_SYNCPOINT(sync_ptr5).AT_THREAD_ID(3);
  IN_CIRCULAR_LIST.ORDER_SYNCPOINT(sync_ptr2).AT_ANY_THREAD.ORDER_SYNCPOINT(sync_ptr6).AT_THREAD_ID(4);
	BEGIN_THREAD_TEST(); 
  CREATE_THREAD(...);
  END_THREAD_TEST();
}

int main(int argc, char** argv) {
  RUN_THREAD_TESTS(argc, argv);

  return 0;
}

************************************************************

TODO:

1. class ThreadTest: an abstract class
    pure virtual interfaces:
        SetUpEnv();
        TearDownEnv();
        Run();
    methods (protected):
        helper to set thread actions & attributes
        collect results after running
    member variables:
        thread action map
        attributes
        results

    Users set their own functions with the help of 
      EXPECT_THAT / ASEERT_THAT

2. macro THREAD_TEST: (with an extra "{" in the end)
    inherit ThreadTest
    construct a derived class
    implement the interface functions
    the functions in Run() 

3. macro RUN_THREAD_TESTS(..., ...)
    init configuration
    run all tests

4. macro EXPECT_THAT ... IS ... clause   check a (global) variable
	 macro EXPECT_FUNC ... ARG ... clause  check with a defined func

5. macro BEGIN_THREAD_TEST: (with an extra "}" in the beginning
      and an extra "{" in the end)
    set action
    set attr
    fork

6. macro END_THREAD_TEST: (with an extra "}" in the beginning) 
    collect test results   

7. test framework:

    ThreadTestContainer (singleton)
    |
     ->ThreadTest
    |  |
    |   ~(Concrete)ThreadTest
    |  |
    |   ~ThreadTestGroup (composite)
    |
     ->ThreadTestResult
    |
     ->ThreadTestFilter

*/

// defines:

// make up a class; register into group; register group into container
// if it occurs at the first time. 
// ...
#define THREAD_TEST(thread_test_group, thread_test_case) \
          DEFINE_CLASS_THREAD_TEST(thread_test_group, thread_test_case); \
          REGISTER_GROUP_AND_CASE(thread_test_group, thread_test_case) \
          DEFINE_RUN_FUNC(thread_test_group, thread_test_case)

#define EXPECT_FUNC(func) (thread_action_setter_ptr = \
																new ThreadActionSetter). \
																							SetFunc(func, \
																							MAKE_STRING(func), \
																							this, \
																							EXPECT_FUNC_TYPE, \
																							__FILE__, \
																							__LINE__)

#define ARG(arg) SetArg(arg, #arg, this)

#define AT_THREAD_ID(tid) SetThreadId(tid)
#define AT_ANY_THREAD SetThreadId(ALL_THREADS)

#define AT_MUTEX(mutex) SetAtMutex(mutex, #mutex, this)
#define AT_COND(cond) SetAtConditionVariable(cond, #cond, this)

#define BEFORE_OPR SetActionBeforeAfter(BEFORE_CALLED)
#define AFTER_OPR SetActionBeforeAfter(AFTER_CALLED)

// nested in:
#define PTHREAD_THREAD_CREATE SetActionName(THREAD_CREATE)
#define PTHREAD_MUTEX_INIT SetActionName(MUTEX_INIT)
#define PTHREAD_MUTEX_DESTROY SetActionName(MUTEX_DESTROY)
#define PTHREAD_MUTEX_LOCK SetActionName(MUTEX_LOCK)
#define PTHREAD_MUTEX_UNLOCK SetActionName(MUTEX_UNLOCK)
#define PTHREAD_COND_INIT SetActionName(COND_INIT)
#define PTHREAD_COND_DESTROY SetActionName(COND_DESTROY)
#define PTHREAD_COND_WAIT SetActionName(COND_WAIT)
#define PTHREAD_COND_TIMED_WAIT SetActionName(COND_TIMED_WAIT)
#define PTHREAD_COND_BROADCAST SetActionName(COND_BROADCAST)
#define PTHREAD_COND_SIGNAL SetActionName(COND_SIGNAL)

#define DO_ONCE SetActionFreq(ONLY_ONCE)
#define DO_ALWAYS SetActionFreq(ALWAYS)

#define ASSERT_FUNC(func) (thread_action_setter_ptr = \
                                new ThreadActionSetter). \
                                              SetFunc(func, \
                                              MAKE_STRING(func), \
                                              this, \
                                              ASSERT_FUNC_TYPE, \
																							__FILE__, \
																							__LINE__)

#define RUN_THREAD_TESTS(argc, argv) \
          do { \
            INIT_ENV(argc, argv) \
            ThreadTestContainer* container = \
                                ThreadTestContainer::GetInstance(); \
            container->Run(); \
            container->DisplayResults(); \
          } while (0)                                       

#define BEGIN_THREAD_TEST() } \
					thread_action_setter_ptr.Clear(); \
					sync_point_helper().PreprocessLists(); \
          SET_DEFAULT_TEST_ACTION \
          SET_DEFAULT_TEST_ATTR \
          FORK \
          {

#define END_THREAD_TEST() } \
          COLLECT_TEST_RESULTS; \
/* TODO: return 0 / 1 as successful or not based on "TestResult" */			return 0

#define CREATE_THREAD(create_thread_fun) \
          if (pid() == 0) { \
						/*this->InitServerSocket(); */\
            create_thread_fun; \
            this->JoinAllThreads(); \
	          exit(0); \
          }       

#define SET_SYNCPOINT_AT_FILE(filename) sync_point_helper().SetFileName(#filename)

#define AT_LINENO(linenum) SetLineNumber(#linenum)

#define WITH_NAME(sync_point_name) SetSyncPointName(#sync_point_name)

#define IN_SINGLE_LIST sync_point_helper().CreateList(SINGLE_LIST)

#define IN_CIRCULAR_LIST sync_point_helper().CreateList(CIRCULAR_LIST)

#define ORDER_SYNCPOINT(sync_point_name) AddListNode(#sync_point_name)

// #define AT_THREAD_ID(tid) SetThreadId(tid)
// #define AT_ANY_THREAD SetThreadId(ALL_THREADS)

// the default value is 1
#define TIMES(for_how_many_times) SetTimes(for_how_many_times)




extern "C" {
extern Tid2Action g_thread_action_info;
}

class ThreadTest {
 public:
  ThreadTest();
  virtual ~ThreadTest();

  // users must define their Run function via "THREAD_TEST";
  // otherwise, the test would be meaningless.
  virtual int Run() = 0;

  pid_t pid() {
    return pid_;
  }

  void set_pid(pid_t n) {
    pid_ = n;
  }

	std::string resource_name(void* ptr) {
		if (!ptr) {
			return std::string("ANY");
		} else if (resource_name_.count(ptr) != 0) {
			return resource_name_[ptr];
		} else {
			return std::string("WRONG PARAM");
		}		
	}

	void set_resource_name(void* ptr, std::string& name) {
		if (resource_name_.count(ptr) != 0) {
			std::cerr<<"cannot set the same pointer repeatedly: "
								<<name<<" v.s. "<<resource_name_[ptr]<<std::endl;
		} else if (ptr) {
			resource_name_[ptr] = name;
		}
	}

	virtual void DeleteTest() {
		// currently, there is nothing that has to be done here.
	}

	std::string GetResourceName(const ThreadResource& resource) {
		std::string ret;
		std::string tmp;
		
		tmp = resource_name(resource.mutex);
		if (tmp != "ANY" && tmp != "WRONG PTR") {
			ret += "[mutex: \"" + tmp + "\"]";
		}

		tmp = resource_name(resource.cond);
    if (tmp != "ANY" && tmp != "WRONG PTR") {
      ret += "[cond: \"" + tmp + "\"]";
    }

		return ret;
	}

	ThreadTestResult& thread_test_result() {
		return thread_test_result_;
	}

	void InitServerSocket();

	TestLog& log() {
		return log_;
	}

	virtual void DisplayResults(FILE* log_file);

 protected:

  virtual int SetUpEnv() { return 0; }

  virtual int TearDownEnv() { return 0; }

  virtual int SetDefaultTestAction() { return 0; }

  virtual int SetDefaultTestAttribute() { return 0; }

  virtual int ForkAndRunParentProcess();

  virtual int JoinAllThreads();

	virtual void GenerateLogs();

	SyncPointHelper& sync_point_helper() {
		return sync_point_helper_;
	}

 private:
 	int RunParentProcess();

	int RunMainThread(int listen_sock);

	static void JudgeSyncPoint(void* thread_test, int sync_point, int tid);

  static void* RunPrimeChild(void* thread_test);

  static void* RunOtherChildren(void* thread_test);

  static void* RunMonitor(void* thread_test);

	static void* RunSyncPointUpdater(void* thread_test);

  bool HasPotentialDeadLock();

	bool ConfirmDeadLock();

  void KillChildrenTest();

	void ProcessRecvbuf(const char* recv_buf);

  ThreadTestResult thread_test_result_;
  ThreadTestFilter filter_;

  int main_socket_;

  pid_t pid_;

  // when this bool variable is set as true, all the created
  // threads will quit from receiving message from the child
  // process.
  bool is_child_end_;

	// log is temperate variable, because all the info will be 
	// removed to TestResult with the help of resource_name_.
  TestLog log_;

	// get resource names: using void* to link to all kinds of 
	// resources or whatever we want, including mutex, func 
	// pointer, and argument.
	// We put this variable here, mainly because each ThreadTest
	// corresponds to a child process, and the address should be
	// maintained within the same process.
	std::map<void*, std::string> resource_name_;

	pthread_mutex_t init_mutex_;
	pthread_cond_t init_cond_;
	bool is_connected_sync_point_updater_;
	int sync_point_updater_sock_;

	pthread_mutex_t sync_point_mutex_;
	pthread_cond_t sync_point_cond_;

	SyncPointHelper sync_point_helper_;
};

class ThreadTestGroup : public ThreadTest {
 public:
  ThreadTestGroup();
  virtual ~ThreadTestGroup();

  virtual int Run();

  // do not expect any inheritated class to overwrite the function,
  // so that it is not declared as "virtual"
  void AddTest(const char* test_name, ThreadTest* thread_test) {
    if (HasTestCase(test_name) || !thread_test) {
      return;
    }

    elements_[test_name] = thread_test;
		names_.push_back(test_name);
  }

	virtual void DeleteTest() {
		for (std::map<std::string, ThreadTest*>::iterator itr = elements_.begin();
					itr != elements_.end();
					++itr) {
			itr->second->DeleteTest();
			delete itr->second;
			itr->second = NULL;
		}

		is_cleared_ = true;
	}

  ThreadTest* GetTest(const std::string test_name) {
    if (!HasTestCase(test_name)) {
      return NULL;
    }

    return elements_[test_name];
  }

 protected:

  virtual int SetUpEnv() { return 0; }

	virtual int TearDownEnv();

  virtual bool IsRunThisTest(const std::string& name);

	virtual void DisplayResults(FILE* log_file);

 private:
  bool HasTestCase(const std::string name) const {
    return (elements_.count(name) > 0);
  }

  
  // name to test case
  // we do not name the variable directly as thread_tests_
  // because it will also be used by ThreadTestContainer
  std::map<std::string, ThreadTest*> elements_;
  // order of test case
  std::vector<std::string> names_;

	bool is_cleared_;

};

class ThreadTestContainer : public ThreadTestGroup {
 public:
  ~ThreadTestContainer();

  static ThreadTestContainer* GetInstance();

	virtual int SetUpEnv() { return 0; } 

  virtual int TearDownEnv() { return 0; }

	void CollectResults(ThreadTest* test);

  virtual void DisplayResults();

	const ThreadAction* GetThreadAction(int tid, 
																			ThreadActionOrder order,
																			ThreadActionNestedIn nested_in,
																			int action_id) {
		if (threadaction_item_.count(action_id) == 0) {
			return NULL;
		}

		int action_idx = threadaction_item_.count(action_id);

		if (action_idx < 0) {
			return NULL;
		}

		if (g_thread_action_info.count(tid) == 0 || 
					g_thread_action_info[tid].size() != THREAD_ACTION_ORDER_SIZE) {
			return NULL;
		}

		if (g_thread_action_info[tid][order].count(nested_in) == 0 ||
					g_thread_action_info[tid][order][nested_in].size() <= 
																				static_cast<size_t>(action_idx)) {
			return NULL;
		}

		return static_cast<const ThreadAction*>
						(&(g_thread_action_info[tid][order][nested_in][action_idx]));
	}

	void SetThreadActionIdx(int idx) {
		threadaction_item_[num_threadaction_items_] = idx;
		++num_threadaction_items_;
	}

	void InitEnv(int argc, char** argv);

  /*
  // 1. run all the cases as long as they are not filtered.
  // 2. report the results
  virtual int Run();
  */

  // use inheritance from ThreadTestGroup:
  /*
  void AddGroup(const std::string group_name, const ThreadTestGroup* group) {
    if (HasGroup(group_name) || !group) {
      return;
    }

    groups_[group_name] = group;
    group_names_.push_back(group_name);
  }

  ThreadTestGroup* GetGroup(const std::string group_name) {
    if (HasGroup(group_name)) {
      return NULL;
    }

    return groups_[group_name];
  }
  */

 private:
  ThreadTestContainer();

	int InitGDB(const char* prog_name);

  /*
  bool HasGroup(const std::string group_name) const {
    return (groups_.count(group_name) > 0);
  }
  */

  // instance:
  static ThreadTestContainer* container_;

  /*
  // name to case
  std::map<std::string, ThreadTestGroup*> groups_;
  // order of name
  std::vector<std::string> group_names_;
  ThreadTestResult result_;
  ThreadTestFilter filter_;
  */

	
	// used to maintain id of ThreadAction
	int num_threadaction_items_;	

	// map id to the idx of ThreadAction within a certain ThreadActionInfo
	// of Tid2Action "g_thread_action_info"
	// (other keys are omitted because they could be got from the message
	// sent by the child process.)
	std::map<int, int> threadaction_item_;

	CmdlineParser cmd_parser_;	
};

#if 0
template<typename GroupType, typename CaseType>
class ThreadTestCreator {
 public:
  ThreadTestCreator() {
    REGISTER_GROUP(GroupType);
    REGISTER_CASE(GroupType, CaseType);
  }

  ~ThreadTestCreator();
};
#endif
int RegisterGroupAndCase(const char* group_name,
                          ThreadTestGroup* test_group,
                          const char* case_name,
                          ThreadTest* test_case);

#endif



