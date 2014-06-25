#ifndef __THREAD_TEST_IMPL_H__
#define __THREAD_TEST_IMPL_H__

#include <stdio.h>

#include "thread_def_common.h"


// Useful implicit definitions are done here.

#define DEFINE_CLASS_THREAD_TEST(thread_test_group, thread_test_case) \
class thread_test_group ## thread_test_case : public ThreadTest { \
 public: \
  thread_test_group ## thread_test_case () { } \
  virtual ~ thread_test_group ## thread_test_case () { } \
  \
  virtual int Run(); \
	\
	static int reg; \
} 

#define MAKE_OBJECT(str) new str()
#define GET_OBJECT(str) ((dynamic_cast<str*>(ThreadTestContainer::GetInstance()->GetTest(#str))) ? \
					(dynamic_cast<str*>(ThreadTestContainer::GetInstance()->GetTest(#str))) : MAKE_OBJECT(str))
#define MAKE_STRING(str) #str

#define REGISTER_GROUP_AND_CASE(thread_test_group, thread_test_case) \
	int thread_test_group ## thread_test_case :: reg = \
				RegisterGroupAndCase(MAKE_STRING(thread_test_group), \
									GET_OBJECT(thread_test_group), \
									MAKE_STRING(thread_test_group ## thread_test_case), \
									MAKE_OBJECT(thread_test_group ## thread_test_case)); 

#if 0
#define REGISTER_GROUP_CASE(thread_test_group, thread_test_case) 

#define REGISTER_GROUP(thread_test_group) \
  ThreadTestContainer* container = ThreadTestContainer::GetInstance(); \
  ThreadTestGroup* group = new thread_test_group (); \
  container->AddTest(#thread_test_group, group) 

#define ADD_THREAD_TEST_CASE(test_name, test_case) \
  group->AddTest(#test_name, test_case)

#define REGISTER_CASE_REAL(thread_test_group, thread_test_case) \
  ThreadTest* test_case = \
              new thread_test_group ## thread_test_case (); \
	ADD_THREAD_TEST_CASE(thread_test_group ## thread_test_case, test_case)

#define REGISTER_CASE(thread_test_group, thread_test_case) \
          REGISTER_CASE_REAL(thread_test_group, thread_test_case)
#endif

// an extra "{" is added in the end, so that 
// the macro of "BEGIN_THREAD_TEST" must be called.
#define DEFINE_RUN_FUNC(thread_test_group, thread_test_case) \
int thread_test_group ## thread_test_case :: Run() { \
	ThreadActionSetterPtr thread_action_setter_ptr;

#define INIT_ENV(argc, argv) \
					ThreadTestContainer::GetInstance()->InitEnv(argc, argv);

#define SET_DEFAULT_TEST_ACTION this->SetDefaultTestAction();

#define SET_DEFAULT_TEST_ATTR this->SetDefaultTestAttribute();

#define FORK this->ForkAndRunParentProcess();


// collect logs (TestLog) and test results (ThreadTestResult)
#define COLLECT_TEST_RESULTS this->GenerateLogs(); \
					ThreadTestContainer::GetInstance()->CollectResults(this)

class ThreadTest;

enum TestLogInsertRet {
	RET_NORMAL,
	RET_ASSERT_FAILED,
};

// internal helper class:
class TestLog {
 public:
  struct LogItem {
		enum MessageType {
			LIB_FUNC_BEGIN,
			LIB_FUNC_END,
			USER_FUNC,
		};
		
		LogItem() :
							thread_id(-1),
              action_type(INVALID_THREAD_ACTION),
              order(THREAD_ACTION_ORDER_SIZE),
              ret_val(0),
              ret(0),
              index(-1),
              message_type(USER_FUNC) { }

    LogItem(int tid, 
            ThreadActionNestedIn action, 
            ThreadActionOrder od, 
            int r_v, 
            int r, 
						int idx,
						MessageType m_t) : 
              thread_id(tid),
              action_type(action),
              order(od),
              ret_val(r_v),
              ret(r),
							index(idx),
							message_type(m_t) { }

		LogItem(const LogItem& item) :
							thread_id(item.thread_id),
              action_type(item.action_type),
              order(item.order),
              ret_val(item.ret_val),
              ret(item.ret),
              index(item.index),
              message_type(item.message_type) { }

    bool IsInLockState() const;
    
    // note that this is not the real process id form linux kernel
    // we use an integer pre-set by users, because we presume that 
    // users care more about the value set by themselves.
    int thread_id;
    ThreadActionNestedIn action_type;
    ThreadActionOrder order;
    int ret_val;
    int ret;
		int index;
		MessageType message_type;
  };

  TestLog();
  
  ~TestLog();

  void Init();

  TestLogInsertRet InsertLog(uint64_t timestamp, LogItem item); 

  bool IsRunNormally() {
    return run_state_ == RUNNING_NORMALLY;
  }

  bool IsDeadLocked() {
    return run_state_ == DEAD_LOCKING;
  }

  void SetDeadLock() {
    run_state_ = DEAD_LOCKING;
  }

  // TODO: access log:


  bool IsAllInLockState();

  bool IsInSuspendState();

 private:
	friend class ThreadTestResult;

  // note that there should never be an item of "CRASHED/ABORTED" here,
  // because once crashed, no info could be captured into the log.
  enum ProcessState {
    RUNNING_NORMALLY,
    DEAD_LOCKING,
  };

  // map timestamp to log:
  std::map< uint64_t, std::vector<LogItem> > time_log_;

  uint64_t latest_log_time_;

  uint64_t last_checked_time_;

  std::map<int, LogItem> latest_log_;
  
  pthread_mutex_t mutex_;

  ProcessState run_state_;
};


class ThreadTestResult {
 public:
  enum ThreadTestResultLogType {
	  LIB_FUNC_LOG = 0,
  	USER_EXPECT_LOG,
		USER_ASSERT_LOG,

		THREAD_TEST_RESULT_LOG_SIZE,
	};

	enum ThreadTestResultEndState {
		SUC = 0,
		FAILED,

		THREAD_TEST_RESULT_END_STATE_SIZE,
	};

	enum ErrType {
    EXPECT_FAILED,
    ASSERT_FAILED,
  };

  struct ErrInfo {
    ErrInfo(ErrType type, 
            std::string stat, 
            std::string file,
            std::string line,
            int id) :
                   err_type(type),
                   expect_statement(stat),
                   src_file(file),
                   line_no(line) {
      char tmp[80];
      snprintf(tmp, 80, "%d", id);
      tid = tmp;
    } 

    ErrType err_type;
    // the statement of "EXPECT_THAT/ASSERT_THAT"
    std::string expect_statement;
    std::string src_file;
    std::string line_no;    
    // thread id set by users:
    std::string tid;
  };

  enum FailureReason {
    FAILEURE_NONE = 0,

    EXPECT_FAILURE = 1,
    ASSERT_FAILURE = (EXPECT_FAILED << 1),
    DEAD_LOCK = (ASSERT_FAILED << 1),
    CRASHED = (DEAD_LOCK << 1),
  };

  ThreadTestResult();
  
  ~ThreadTestResult();

  bool IsTestSuc() const {
    return is_all_suc_;
  }

  void SetTestCrashed();

  void SetTestDeadlock();

  void SetExpectAssertErr(const ErrInfo& err_info);

  // TODO: discard this function
	std::string GetFailureReason(const ErrInfo& err_info);

	void GenerateLogs(ThreadTest* test);

	void DisplayGeneralResult(FILE* log_file) {
		if (is_all_suc_) {
			fprintf(log_file, "Succeeded.\n");
		} else {
			fprintf(log_file, "Failed: %s\n", failed_reason_str_.c_str());
		}
	}

	void DisplaySuccessLogs(FILE* log_file) {
		for (size_t i = LIB_FUNC_LOG; i < THREAD_TEST_RESULT_LOG_SIZE; ++i) {
			for (size_t j = 0; j < logs_[i][SUC].size(); ++j) {
				fprintf(log_file, "%s\n", logs_[i][SUC][j].c_str());
			}
		}
	}

	void DisplayFailureLogs(FILE* log_file) {
		for (size_t i = LIB_FUNC_LOG; i < THREAD_TEST_RESULT_LOG_SIZE; ++i) {
      for (size_t j = 0; j < logs_[i][FAILED].size(); ++j) {
				fprintf(log_file, "%s\n", logs_[i][FAILED][j].c_str());
      }
    }
	}

 private:

  bool is_all_suc_;

  // crashed? / deadlocked?
  int failure_reason_;

	std::string failed_reason_str_;

  std::vector<ErrInfo> all_errs_;
	
	// 1st dim: ThreadTestResultLogType;
	// 2nd dim: success / failure;
	// 3rd dim: each log takes an element
	std::vector< std::vector< std::vector<std::string> > > logs_;

  pthread_mutex_t mutex_;
   
};

//class ThreadActionSetterPtr;

class ThreadActionSetter {
 public:
  ThreadActionSetter();

  ~ThreadActionSetter();

 private:
	friend class ThreadActionSetterPtr;

  Func func_;
  void* arg_;
	ThreadActionType func_type_;
  int tid_;
  ThreadResource resource_;
  ThreadActionOrder order_;
  ThreadActionNestedIn pthread_func_;
  ThreadActionFreq freq_;
  int freq_num_;
	std::string file_;
	std::string line_;
};

class ThreadActionSetterPtr {
 public:
  ThreadActionSetterPtr();

  ~ThreadActionSetterPtr();

  ThreadActionSetterPtr& operator=(ThreadActionSetter* setter);

	void Clear();

	ThreadActionSetterPtr& SetFunc(Func func,
																	std::string resource_name,
																	ThreadTest* test,
																	ThreadActionType func_type,
																	std::string file,
																	int line);

  ThreadActionSetterPtr& SetArg(void* arg,
																std::string resource_name,
																ThreadTest* test);

  ThreadActionSetterPtr& SetThreadId(int tid);

  ThreadActionSetterPtr& SetAtMutex(pthread_mutex_t* mutex,
																		std::string resource_name,
																		ThreadTest* test);

  ThreadActionSetterPtr& SetAtConditionVariable(pthread_cond_t* cond,
																		std::string resource_name,
                                    ThreadTest* test);

  ThreadActionSetterPtr& SetActionBeforeAfter(ThreadActionOrder order);

  ThreadActionSetterPtr& SetActionName(ThreadActionNestedIn name);

  ThreadActionSetterPtr& SetActionFreq(ThreadActionFreq freq);

 private:

  ThreadActionSetter* ptr_;
};

//const int CHECK_THIS_SYNC_POINT_ALWAYS = -1;
//const int STOP_CHECKING_THIS_SYNC_POINT = 0;

//const int CHECK_THIS_SYNC_POINT_AT_ALL_THREADS = ALL_THREADS;

enum OrderListType {
	SINGLE_LIST,
	CIRCULAR_LIST,
};

class SyncPointHelper {
	struct SyncPoint {
		SyncPoint() : is_done(false),
									remained_check_num(1),
									hit_num(0),
									thread_num(ALL_THREADS) {
		}

		bool is_done;
		int remained_check_num;
		int hit_num;
		int thread_num;
	};

	struct SyncPointOrderList {
		SyncPointOrderList(OrderListType t) : type(t),
																					is_done(false),
																					sync_point_v() {
		}

		OrderListType type;
		// used for SINGLE_LIST:
		bool is_done;
		std::vector<SyncPoint> sync_point_v;
	};

	struct SyncPointOrderIndex {
		SyncPointOrderIndex(int list_n, int element_n) :
													list_num(list_n) {
			element_num.push_back(element_n);
		}		

		int list_num;
		std::vector<int> element_num;
	};

	typedef	std::vector<SyncPointOrderIndex> SyncPointOrderListIndex;

 public:
	SyncPointHelper();
	~SyncPointHelper();

	SyncPointHelper& SetFileName(const char* filename);
	SyncPointHelper& SetLineNumber(const char* linenum);
	int SetSyncPointName(const char* sync_point_name);	

	SyncPointHelper& CreateList(OrderListType type);
	SyncPointHelper& AddListNode(const char* sync_point_name);
	SyncPointHelper& SetThreadId(int tid);
	SyncPointHelper& SetTimes(int must_run_times);

	void PreprocessLists();	

	void Update(int bk, int tid);

	bool Check(int bk, int tid);
	
 private:

	std::string tmp_file_name_;
	std::string tmp_line_num_;

	std::map<int, std::string> gdb_bk_2_sync_name_;
	std::map<std::string, int> sync_name_2_gdb_bk_;
	std::map<int, SyncPointOrderListIndex> sync_point_index_list_;

	std::vector<SyncPointOrderList> sync_point_order_list_;
	bool is_skip_;
	int curr_sync_point_order_list_;
	int curr_sync_point_index_;
};

class ThreadTestFilter {
 public:


 private:

};

#endif
