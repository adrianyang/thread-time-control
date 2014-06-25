#include "thread_test.h"

#include <unistd.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <netdb.h>
#include <netinet/tcp.h>
#include <sys/wait.h>
#include <dlfcn.h>
#include <arpa/inet.h>
#include <errno.h>

#include <stdio.h>
#include <string.h>
#include <set>

extern "C" {
#include "gdb_programing_interface_for_multithread_testing.h"
}

// declared in "thread_def_common.h"
const char* kThreadActionNestedIn[] = { "pthread_create", "pthread_mutex_init",
							"pthread_mutex_destroy", "pthread_mutex_lock", "pthread_mutex_unlock",
							"pthread_cond_init", "pthread_cond_destroy", "pthread_cond_wait",
							"pthread_cond_timedwait", "pthread_cond_broadcast", "pthread_cond_signal",
							NULL }; 

// all the pthread functions are defined here:

// init:
int (*s_pthread_mutex_init)(pthread_mutex_t*, 
														const pthread_mutexattr_t*);
int (*s_pthread_cond_init)(pthread_cond_t*,
														const pthread_condattr_t*);

// create thread:
int (*s_pthread_create)(pthread_t*,
                   const pthread_attr_t*,
                   void *(*)(void*),
                   void*);

// get pthread_t:
pthread_t (*s_pthread_self)();

// send signal:
int (*s_pthread_kill)(pthread_t, int);

// join:
int (*s_pthread_join)(pthread_t, void**);

// mutex:
int (*s_pthread_mutex_lock)(pthread_mutex_t*);
int (*s_pthread_mutex_unlock)(pthread_mutex_t*);

// condition variable:
int (*s_pthread_cond_wait)(pthread_cond_t*, pthread_mutex_t*);
int (*s_pthread_cond_broadcast)(pthread_cond_t*);
int (*s_pthread_cond_signal)(pthread_cond_t*);


bool TestLog::LogItem::IsInLockState() const {
  // TODO: skip the thread who creates others,
  // because sometimes, the main thread works as a cycling one

  if (!action_type != MUTEX_LOCK && !action_type != COND_WAIT) {
    return false;
  }

  return order == BEFORE_CALLED;
}

TestLog::TestLog() {
}

TestLog::~TestLog() {
}

void TestLog::Init() {
	latest_log_time_ = 0;
	last_checked_time_ = static_cast<uint64_t>(time(NULL));
	run_state_ = RUNNING_NORMALLY;

	time_log_.clear();
	latest_log_.clear();
}

TestLogInsertRet TestLog::InsertLog(uint64_t timestamp, TestLog::LogItem item) {
	TestLogInsertRet ret = RET_NORMAL;

  s_pthread_mutex_lock(&mutex_);

  time_log_[timestamp].push_back(item);

  // update the latest timestamp:
  if (timestamp > latest_log_time_) {
    latest_log_time_ = timestamp;
  }

  // update the thread behavior:
  latest_log_[item.thread_id] = *(time_log_[timestamp].rbegin());

	//TODO:
	if (item.index >= 0 && item.ret_val == 0 && 
			g_thread_action_info[item.thread_id][item.order][item.action_type][item.index].
											type == ASSERT_FUNC_TYPE) {
		ret = RET_ASSERT_FAILED;
	}

  s_pthread_mutex_unlock(&mutex_);

	return ret;
}

bool TestLog::IsAllInLockState() {
	if (latest_log_.empty()) {
		return false;
	}

  std::map<int, LogItem>::iterator itr;

	for (itr = latest_log_.begin(); itr != latest_log_.end(); ++itr) {
    const LogItem& log = itr->second;
    if (!log.IsInLockState()) {
      return false;
    }
  }

  // register the current checking time
  last_checked_time_ = latest_log_time_;
  return true;
}

bool TestLog::IsInSuspendState() {
  // TODO:  main thread case???

  s_pthread_mutex_lock(&mutex_);

fprintf(stderr, "  checking deadlock; lock mutex; %d\n\n", getpid());


  bool ret = latest_log_time_ == last_checked_time_;
  s_pthread_mutex_unlock(&mutex_);

  return ret;
}

ThreadTestResult::ThreadTestResult() : 
		is_all_suc_(true),
		failure_reason_(FAILEURE_NONE),
		logs_(THREAD_TEST_RESULT_LOG_SIZE, 
					std::vector< std::vector<std::string> >
							(THREAD_TEST_RESULT_END_STATE_SIZE, std::vector<std::string>())) {
}
      
ThreadTestResult::~ThreadTestResult() {
} 

void ThreadTestResult::SetExpectAssertErr(const ErrInfo& err_info) {
	s_pthread_mutex_lock(&mutex_);

	is_all_suc_ = false;
	all_errs_.push_back(err_info);
	if (err_info.err_type == EXPECT_FAILED) {
		failure_reason_ |= EXPECT_FAILURE;
		failed_reason_str_ += "EXPECT_FAILURE";
	} else if (err_info.err_type == ASSERT_FAILED) {
		failure_reason_ |= ASSERT_FAILURE;
		failed_reason_str_ += "ASSERT_FAILURE";
	}

	s_pthread_mutex_unlock(&mutex_);
}

void ThreadTestResult::SetTestCrashed() {
  s_pthread_mutex_lock(&mutex_);
  is_all_suc_ = false;
  failure_reason_ |= CRASHED;
	failed_reason_str_ += "CRASHED";
  s_pthread_mutex_unlock(&mutex_);
}

void ThreadTestResult::SetTestDeadlock() {
  s_pthread_mutex_lock(&mutex_);
  is_all_suc_ = false;
  failure_reason_ |= DEAD_LOCK;
	failed_reason_str_ += "DEADLOCK";
  s_pthread_mutex_unlock(&mutex_);
}

std::string ThreadTestResult::GetFailureReason(const ErrInfo& err_info) {
	if (is_all_suc_) {
		return "\tCurrent test succeeded.\n";
	}

	std::string reason;
	if (err_info.err_type & CRASHED) {
		reason += "\tCurrent test crashed.\n";
	}
	if (err_info.err_type & DEAD_LOCK) {
		reason += "\tDeadlock was found in current test.\n";
	}
	if (err_info.err_type & ASSERT_FAILED || 
				err_info.err_type & EXPECT_FAILED) {
		reason += "\tAssertions failed.\n";
	}

	for (size_t i = 0; i < all_errs_.size(); ++i) {
		reason += "\t\t[";
		reason += 
			(all_errs_[i].err_type == EXPECT_FAILED ? "Expectation] " : "Assertion] ");
		reason += " in file: ";
		reason += all_errs_[i].src_file;
		reason += ", line ";
		reason += all_errs_[i].line_no;
		reason += "; <thread ";
		reason += all_errs_[i].tid;
		reason += ">\n";
	}
	
	return reason;
}

void ThreadTestResult::GenerateLogs(ThreadTest* test) {
	std::map< uint64_t, std::vector<TestLog::LogItem> >& time_log = test->log().time_log_;
	uint64_t last_t = 0;
	char last_date_time[80];
	char tmp[80];
	std::string str;

	for (std::map< uint64_t, std::vector<TestLog::LogItem> >::iterator itr = time_log.begin();
					itr != time_log.end();
					++itr) {
		if (itr->first != last_t) {
			time_t stored_time = static_cast<time_t>(itr->first);
			struct tm* date_time = localtime(&stored_time);
			strftime(last_date_time, 60, "%Y-%m-%d-%X ", date_time);
			last_t = itr->first;
		}

		// TODO: missed "for" here... 
		
		const std::vector<TestLog::LogItem>& item_v = itr->second;

		for (std::vector<TestLog::LogItem>::const_iterator citr = item_v.begin();
						citr != item_v.end(); 
						++citr) {
			str = last_date_time;
			const TestLog::LogItem& item = *citr;

			if (item.message_type == TestLog::LogItem::LIB_FUNC_BEGIN) {
				if (item.order == BEFORE_CALLED) {
					snprintf(tmp, 60, "thread %d is running ", item.thread_id);
					str += tmp;
					str += kThreadActionNestedIn[item.action_type];
					str += ".";
					logs_[LIB_FUNC_LOG][SUC].push_back(str);
				} else {
					snprintf(tmp, 60, "thread %d has just ended ", item.thread_id);
	        str += tmp;
  	      str += kThreadActionNestedIn[item.action_type];
					str += ".";
					// all the pthread funcs return 0 as success:
					if (item.ret_val == 0) {
						logs_[LIB_FUNC_LOG][SUC].push_back(str);
					} else {
						str += "[FAILED: \"";
						// TODO: correct "strerror"
				//		str += strerror(item.ret_val);
						str += "\"]";
						logs_[LIB_FUNC_LOG][FAILED].push_back(str);
					}				
				}	
			} else if (item.message_type == TestLog::LogItem::LIB_FUNC_END) {
				// currently, skip this one.
			} else if (item.message_type == TestLog::LogItem::USER_FUNC) {
				const ThreadAction& action_structure =
          g_thread_action_info[item.thread_id][item.order][item.action_type][item.index];
				if (action_structure.type == EXPECT_FUNC_TYPE) {
					str += "EXPECT: \"";
				} else if (action_structure.type == ASSERT_FUNC_TYPE) {
					str += "ASSERT: \"";
				}

				str += test->resource_name((void*)(action_structure.action));
				str += "(";
				str += test->resource_name(action_structure.para);
				str += ")\", ";

  	    if (item.order == BEFORE_CALLED) {
    	    snprintf(tmp, 60, "before thread %d is running ", item.thread_id);
      	  str += tmp;
        	str += kThreadActionNestedIn[item.action_type];
	        str += ".";
  	      str += test->GetResourceName(action_structure.resource);
    	  } else {
      	  snprintf(tmp, 60, "after %d has just ended ", item.thread_id);
        	str += tmp;
	        str += kThreadActionNestedIn[item.action_type];
    	    str += ".";
					str += test->GetResourceName(action_structure.resource);
				}

				str += " [FILE: " + action_structure.file_name + "]";
				str += "[LINE: " + action_structure.line_num + "] ";
			
				if (item.ret == 1) {
					if (action_structure.type == EXPECT_FUNC_TYPE) {
						logs_[USER_EXPECT_LOG][SUC].push_back(str);
					} else if (action_structure.type == ASSERT_FUNC_TYPE) {
						logs_[USER_ASSERT_LOG][SUC].push_back(str);
					}
				}	else {
					str += "FAILED";
					if (action_structure.type == EXPECT_FUNC_TYPE) {
      	    logs_[USER_EXPECT_LOG][FAILED].push_back(str);
        	} else if (action_structure.type == ASSERT_FUNC_TYPE) {
          	logs_[USER_ASSERT_LOG][FAILED].push_back(str);
	        }
				}
			}
		}
	}
}

ThreadActionSetter::ThreadActionSetter() : func_(NULL),
																						arg_(NULL),
																						func_type_(DEFAULT_TYPE),
																						tid_(INVALID_TID),
																						resource_(),
																						order_(BOTH_BEFORE_AND_AFTER_CALLED),
																						pthread_func_(INVALID_THREAD_ACTION),
																						freq_(ALWAYS),
																						freq_num_(0) {
}

ThreadActionSetter::~ThreadActionSetter() {
	// set values before being deleted.

	if (!func_ || tid_ == INVALID_TID || 
					(freq_ == ONLY_ONCE && freq_num_ <= 0) ||
					pthread_func_ == INVALID_THREAD_ACTION) {
		return;
	}

	if (g_thread_action_info.count(tid_) == 0) {
		g_thread_action_info[tid_].resize(THREAD_ACTION_ORDER_SIZE);
	}	
	
	if (freq_ == ALWAYS) {
		if (order_ != BOTH_BEFORE_AND_AFTER_CALLED) {
			g_thread_action_info[tid_][order_][pthread_func_].push_back(
						ThreadAction(freq_, func_, arg_, resource_, 
													func_type_, file_, line_));
		} else {
			g_thread_action_info[tid_][BEFORE_CALLED][pthread_func_].push_back(
            ThreadAction(freq_, func_, arg_, resource_,
                          func_type_, file_, line_));
			g_thread_action_info[tid_][AFTER_CALLED][pthread_func_].push_back(
            ThreadAction(freq_, func_, arg_, resource_,
                          func_type_, file_, line_));
		}
	} else if (freq_ == ONLY_ONCE) {
		for (int i = 0; i < freq_num_; ++i) {
			if (order_ != BOTH_BEFORE_AND_AFTER_CALLED) {
				g_thread_action_info[tid_][order_][pthread_func_].push_back(
							ThreadAction(freq_, func_, arg_, resource_, 
													func_type_, file_, line_));
			} else {
				g_thread_action_info[tid_][BEFORE_CALLED][pthread_func_].push_back(
              ThreadAction(freq_, func_, arg_, resource_,
                          func_type_, file_, line_));
				g_thread_action_info[tid_][AFTER_CALLED][pthread_func_].push_back(
              ThreadAction(freq_, func_, arg_, resource_,
                          func_type_, file_, line_));
			}
		}
	}
	
}

ThreadActionSetterPtr::ThreadActionSetterPtr() : ptr_(NULL) {
}

ThreadActionSetterPtr::~ThreadActionSetterPtr() {
	Clear();
}

ThreadActionSetterPtr& ThreadActionSetterPtr::operator=(
																			ThreadActionSetter* setter) {
	if (ptr_) {
		delete ptr_;
	}
	ptr_ = setter;

	return *this;
}

void ThreadActionSetterPtr::Clear() {
	if (ptr_) {
    delete ptr_;
    ptr_ = NULL;
  }
}

ThreadActionSetterPtr& ThreadActionSetterPtr::SetFunc(Func func,
																				std::string resource_name,
                                        ThreadTest* test,
																				ThreadActionType func_type,
																				std::string file,
                                  			int line) {
  if (ptr_) {
		ptr_->func_ = func;
  	ptr_->func_type_ = func_type;
  	ptr_->file_ = file;
		char tmp[80];
		snprintf(tmp, 60, "%d", line);
  	ptr_->line_ = tmp;
		test->set_resource_name((void*)func, resource_name);
	}

  return *this;
}

ThreadActionSetterPtr& ThreadActionSetterPtr::SetArg(void* arg,
																				std::string resource_name,
                                        ThreadTest* test) {
  if (ptr_) {
		ptr_->arg_ = arg;
		test->set_resource_name(static_cast<void*>(arg), resource_name);
	}

  return *this;
}

ThreadActionSetterPtr& ThreadActionSetterPtr::SetThreadId(int tid) {
  if (ptr_) {
		ptr_->tid_ = tid;
	}

  return *this;
}

ThreadActionSetterPtr& ThreadActionSetterPtr::SetAtMutex(
                                        pthread_mutex_t* mutex,
																				std::string resource_name,
                                    		ThreadTest* test) {
  if (mutex && ptr_) {
		ptr_->resource_.mutex = mutex;
		test->set_resource_name(static_cast<void*>(mutex), resource_name);
	}

  return *this;
}

ThreadActionSetterPtr& ThreadActionSetterPtr::SetAtConditionVariable(
                                        pthread_cond_t* cond,
																				std::string resource_name,
                                        ThreadTest* test) {
  if (cond && ptr_) {
		ptr_->resource_.cond = cond;
		test->set_resource_name(static_cast<void*>(cond), resource_name);
	}

  return *this;
}

ThreadActionSetterPtr& ThreadActionSetterPtr::SetActionBeforeAfter(
                                        ThreadActionOrder order) {
  if (order != THREAD_ACTION_ORDER_SIZE && ptr_) {
		ptr_->order_ = order;
	}

  return *this;
}

ThreadActionSetterPtr& ThreadActionSetterPtr::SetActionName(
                                        ThreadActionNestedIn name) {
  if (ptr_) {
		ptr_->pthread_func_ = name;
	}

  return *this;
}

ThreadActionSetterPtr& ThreadActionSetterPtr::SetActionFreq(
                                        ThreadActionFreq freq) {
  if (ptr_) {
		ptr_->freq_ = freq;
  	if (freq == ONLY_ONCE) {
    	++ptr_->freq_num_;
  	}
	}

  return *this;
}


ThreadTest::ThreadTest() : main_socket_(-1),
                            is_child_end_(true) {
	// make sure that no redundant data is left.
	resource_name_.clear();
}

ThreadTest::~ThreadTest() {
  if (main_socket_ > 0) {
    close(main_socket_);
    main_socket_ = -1;
  }
}

extern "C" {
extern int g_pid_passed_to_multithread_test;
}

int ThreadTest::ForkAndRunParentProcess() {
	log_.Init();
	RunChild();
	pid_ = g_pid_passed_to_multithread_test;
  if (pid_ > 0) {
		// 1. prepare socket
		// server: write-only
		int listen_sock = socket(AF_INET, SOCK_STREAM, 0);
		if (listen_sock == -1) {
	    std::cerr<<"creating socket failed for server(parent); have to quit"<<std::endl;
			// TODO: graceful exit: kill child process also
    	exit(-1);
	  }

		struct sockaddr_in serv_addr;
  	memset(&serv_addr, 0, sizeof(serv_addr));
  	serv_addr.sin_family = AF_INET;
  	// only localhost available:
  	serv_addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
  	// hard coded port:
  	serv_addr.sin_port = htons(7575);
  	if (bind(listen_sock,
    	  reinterpret_cast<struct sockaddr*>(&serv_addr),
      	sizeof(serv_addr)) == -1) {
    	std::cerr<<"server(parent) binding error"<<std::endl;
    	// TODO: see above
			exit(-1);
  	}
		// expect only one thread of the child process
  	if (listen(listen_sock, 1) == -1) {
    	std::cerr<<"listening error(parent)"<<std::endl;
    	// TODO: see above
			exit(-1);
	  }

		// TODO: check if the child runs normally before calling
		// the following function!

		return RunMainThread(listen_sock);
  }


	// BEFORE connecting with parent successfully, stop itself:
	raise(SIGTRAP);
	sleep(1);

	s_pthread_mutex_init(&sync_point_mutex_, NULL);
  s_pthread_cond_init(&sync_point_cond_, NULL);

	pthread_t child_tid;

	// client: read-only
	if ((sync_point_updater_sock_ = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
    std::cerr<<"could not create socket for sync point updater"<<std::endl;
    return -1;
		//goto END_OF_SYNC_POINT_UPDATER;
  }
	
  struct sockaddr_in serv_addr;
  serv_addr.sin_family = AF_INET;
  // hard code here, :)
  serv_addr.sin_port = htons(7575);
  // server is on the same machine
  if (inet_pton(AF_INET, "127.0.0.1", &serv_addr.sin_addr) <= 0) {
    std::cerr<<"could not convert ip address for sync point updater"<<std::endl;
    close(sync_point_updater_sock_);
		return -1;
		//goto END_OF_SYNC_POINT_UPDATER;
  }

  while (true) {
    // wait for the server for a little bit time:
    sleep(3);
    int conn_ret = connect(sync_point_updater_sock_, 
                            reinterpret_cast<struct sockaddr*>(&serv_addr),
                            sizeof(serv_addr));

    if (conn_ret == 0) {
      break;
    } else if (errno != ECONNREFUSED) {
      std::cerr<<"could not connect to server, due to "
                <<strerror(errno)<<std::endl;
			close(sync_point_updater_sock_);
			return -1;
      //goto END_OF_SYNC_POINT_UPDATER;
    }
  }

	is_child_end_ = false;
	
  s_pthread_create(&child_tid, NULL, RunSyncPointUpdater, this); 
	sleep(3);

  return 0; 
}

int ThreadTest::RunMainThread(int listen_sock) {
	// 2. let GDB wake up the child
	RunParent();

	// till now, the parent will wait until the child connects it.
	int accepted_sock = accept(listen_sock, NULL, NULL);
//	int one = 1;
//	setsockopt(accepted_sock, IPPROTO_TCP, TCP_NODELAY, &one, sizeof(one)); 
//	setsockopt(accepted_sock, IPPROTO_TCP, TCP_QUICKACK, &one, sizeof(one));
	close(listen_sock);

	// 3. wait for the child until it exits.
	int break_point;
	int thread_num;
	// map thread_num to break_point:
	std::map<int, int> tid_2_bk;
	char str[100];
	std::set<int> hit_sync_point_tid;
	do {
		QueryStatus(&break_point, &thread_num);

		if (break_point < -1 && break_point != -10) {
			// error or exit
			break;
		}

		// TODO: process:
		// 1. notify the child the change of current status

		// 2. continue with GDB commands:
		// case 1: ordinary breakpoints (to-be-done syncpoints):
		// call + next
		// case 2: momentary breakpoints (done syncpoints):
		// continue 
		if (break_point > 0) {

//fprintf(stderr, "[regular breakpoint]: thread %d; breakpoint %d\n", thread_num, break_point);

			// 1. "next" command
			// 2. "call" command to a defined func, with break_point & thread_num as arguments
			Next();
			// store bk info to map:
			tid_2_bk[thread_num] = break_point;
			// remember this tid, so that next time a momentary breakpoint with the same tid is hit, 
			// it definitely indicates that the checking function has already returns successfully,
			// so that some stack recovery (also, GDB info recovery) needs being done right away.
			hit_sync_point_tid.insert(thread_num);
			/*
			sprintf(str, "ThreadTest::JudgeSyncPoint(%p, %d, %d)", 
								this, break_point, thread_num);
			*/
			sprintf(str, "ThreadTest::JudgeSyncPoint");
			ARG_TYPE arg_type[3] = {ARG_TYPE_ADDR, ARG_TYPE_INTEGER, ARG_TYPE_INTEGER};
			ARG_VALUE arg_value[3];
			arg_value[0].addr = this;
			arg_value[1].digit = break_point;
			arg_value[2].digit = thread_num;
			CallFunc(str, 3, arg_type, arg_value);
		} else {
			// 0. recovery
			if (hit_sync_point_tid.count(thread_num) > 0) {
				hit_sync_point_tid.erase(thread_num);
				Recover(thread_num);
				continue;
			}

			// NOTE: swap step 1 and step 2; we could not continue and then inform the child
			//       because "continue" command will resume all the threads. An extra sync point 
			//       would be wrongly reached due to delayed message. 
			// 2. "continue" command
			// 1. socket: send data to child process: write  judge break_point according to thread_num
			// (push thread_num & break_point to stack; keep them)
			if (tid_2_bk.count(thread_num) == 0 || tid_2_bk[thread_num] == -1) {
				// error occurs?
				std::cerr<<" Error... no sync point info registered for current thread: "<<thread_num<<std::endl;
				Continue();
				//continue;
			} else {
				sprintf(str, "breakpoint: %d; thread id: %d", tid_2_bk[thread_num], thread_num);
				// TODO: a decent sync method here, is to let the updating thread of child
				// process never stop, by using the newest feature of GDB: async mode
				write(accepted_sock, str, 80);
				Continue();
			}
		}
	} while (true);

	// 4. maintain current testing result
	// TODO...

	close(accepted_sock);
	return 0;
}

void ThreadTest::JudgeSyncPoint(void* thread_test, int sync_point, int tid) {
	ThreadTest* curr_test = static_cast<ThreadTest*>(thread_test);
	s_pthread_mutex_lock(&(curr_test->sync_point_mutex_));
	while (!curr_test->sync_point_helper_.Check(sync_point, tid)) {
		s_pthread_cond_wait(&(curr_test->sync_point_cond_), 
												&(curr_test->sync_point_mutex_));
	}
	s_pthread_mutex_unlock(&(curr_test->sync_point_mutex_));
}

void ThreadTest::InitServerSocket() {
	void (*init_sock)();
	init_sock = (void (*)())dlsym(RTLD_DEFAULT, "InitSocket");

	if (!init_sock) {
		fprintf(stderr, "Could not start server. : %s\n\n", dlerror());
		
sleep(4);

		exit(-1);
	}
	init_sock();
fprintf(stderr, "after calling init server socket\n\n");
}

int ThreadTest::RunParentProcess() {
  // the children could run now:
  is_child_end_ = false;

  // 1. create a series of threads receiving messages from the child
  pthread_t child_tid;


  s_pthread_create(&child_tid, NULL, RunPrimeChild, this);

	// give control back to GDB
#if 0
  // 2. wait for child process to end 
  //    (we have at most one child all the time.)

  // TODO: check if we need to force the child to quit after "some time"
  

  int status;
	int wait_ret = -1;
	do {
  	wait_ret = wait(&status);
	} while (WIFCONTINUED(status));

	// 3. inform the child threads to end
  is_child_end_ = true;  

  // confirmation: wait until they really end
  void* ret_join = NULL;
	s_pthread_join(child_tid, &ret_join);

  // 4. maintain current testing result
	if (wait_ret != pid_) {
		std::cerr<<"could not know how the current test ran...";
		return -1;
	}

	if (WIFEXITED(status)) {
		if (log_.IsDeadLocked()) {
			thread_test_result_.SetTestDeadlock();
		}
	} else if (WIFSIGNALED(status)) {
		thread_test_result_.SetTestCrashed();
	} else if (WIFSTOPPED(status)) {
		// note that we may also send some moderate signal to let
		// the child process end. Therefore, we could not justify
		// it as failed.
		if (log_.IsDeadLocked()) {
			thread_test_result_.SetTestDeadlock();
		}
	}
#endif
  return 0;
}

void* ThreadTest::RunSyncPointUpdater(void* thread_test) {
  ThreadTest* curr_test = static_cast<ThreadTest*>(thread_test);
	int sock = curr_test->sync_point_updater_sock_;
	char recv_buf[100];
//	const char* ack = "Child has finished updating"; 

//	int one = 1;
//  setsockopt(sock, IPPROTO_TCP, TCP_NODELAY, &one, sizeof(one));
//	setsockopt(sock, IPPROTO_TCP, TCP_QUICKACK, &one, sizeof(one));

  while (!curr_test->is_child_end_ && curr_test->log_.IsRunNormally()) {
    // reveive data, now
    int received = read(sock, recv_buf, 80);
    if (received <= 0) {
			std::cerr<<"updating sync point ends: read size: "<<received<<std::endl;
      // EOF or error
      break;
    }

    // TODO: process this message
		int breakpoint_id;
		int thread_id;
 		sscanf(recv_buf, "breakpoint: %d; thread id: %d", &breakpoint_id, &thread_id);
		s_pthread_mutex_lock(&(curr_test->sync_point_mutex_));
		curr_test->sync_point_helper_.Update(breakpoint_id, thread_id);
		s_pthread_cond_broadcast(&(curr_test->sync_point_cond_));
		s_pthread_mutex_unlock(&(curr_test->sync_point_mutex_));
	}

	close(sock);
	return NULL;
}

void* ThreadTest::RunPrimeChild(void* thread_test) {
  ThreadTest* curr_test = static_cast<ThreadTest*>(thread_test);
  int sock = -1;

	char recv_buf[4096];
  std::vector<pthread_t> tids;	
	pthread_t child_tid;
  void* ret_join = NULL;

  if (!curr_test->is_child_end_) {
    if ((sock = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
      std::cerr<<"could not create socket for prime client"<<std::endl;
      goto END_OF_PRIME_CHILD;
    }

    struct sockaddr_in serv_addr;
    serv_addr.sin_family = AF_INET;
    // hard code here, :)
    serv_addr.sin_port = htons(7777);
    // server is on the same machine
    if (inet_pton(AF_INET, "127.0.0.1", &serv_addr.sin_addr) <= 0) {
      std::cerr<<"could not convert ip address for prime client"<<std::endl;
      goto END_OF_PRIME_CHILD;
    }
  
    // no garantee of thread safe for usage of errno here.
    // we try until the server could be connected, 
    // because the client starts earlier.
    while (!curr_test->is_child_end_) {
      // wait for the server for a little bit time:
      sleep(3);
      int conn_ret = connect(sock, 
                              reinterpret_cast<struct sockaddr*>(&serv_addr),
                              sizeof(serv_addr));
      if (conn_ret == 0) {
				std::cerr<<"main child connecting succeeded."<<std::endl<<std::endl;
        break;
      } else if (errno != ECONNREFUSED) {
        std::cerr<<"could not connect to server, due to "
                  <<strerror(errno)<<std::endl;
        goto END_OF_PRIME_CHILD;
      }
    }
  }

  s_pthread_create(&child_tid, NULL, RunMonitor, thread_test);

	tids.push_back(child_tid);

  while (!curr_test->is_child_end_ && curr_test->log_.IsRunNormally()) {
    // reveive data, now
    
    int received = read(sock, recv_buf, 4000);
    if (received <= 0) {
			std::cerr<<"Prime child could not receive any data."<<std::endl;
      // EOF or error
      break;
    }

    // TODO: process this message
    if (strncmp(recv_buf, "NEW THREAD", strlen("NEW THREAD")) == 0) {
      pthread_t child_tid;
      s_pthread_create(&child_tid, NULL, RunOtherChildren, thread_test);
    } else {
			curr_test->ProcessRecvbuf(recv_buf);
		}
  }

  for (size_t i = 0; i < tids.size(); ++i) {
    // currently, we do not care about the return value of join.
    // TODO: we do need the return value 
    //       to check if that thread exits normally.
    s_pthread_join(tids[i], &ret_join);
  }

END_OF_PRIME_CHILD:  
  close(sock);

  return NULL;
}

void* ThreadTest::RunOtherChildren(void* thread_test) {
  ThreadTest* curr_test = static_cast<ThreadTest*>(thread_test);
  int sock = -1;

	struct sockaddr_in serv_addr;
  serv_addr.sin_family = AF_INET;

	char recv_buf[4096];
  std::vector<pthread_t> tids;

  if (!curr_test->is_child_end_) {
    if ((sock = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
      std::cerr<<"could not create socket for prime client"<<std::endl;
      goto END_OF_OTHER_CHILDREN;
    }

    // hard code here, :)
    serv_addr.sin_port = htons(7777);
    // server is on the same machine
    if (inet_pton(AF_INET, "127.0.0.1", &serv_addr.sin_addr) <= 0) {
      std::cerr<<"could not convert ip address for prime client"<<std::endl;
      goto END_OF_OTHER_CHILDREN;
    }

    // no garantee of thread safe for usage of errno here.
    // we try until the server could be connected, 
    // because the client starts earlier.
    while (!curr_test->is_child_end_ && curr_test->log_.IsRunNormally()) {
      // wait for the server for a little bit time:
      sleep(3);
      int conn_ret = connect(sock, 
                              reinterpret_cast<struct sockaddr*>(&serv_addr),
                              sizeof(serv_addr));
      if (conn_ret == 0) {
				std::cerr<<"one other child connecting succeeded."<<std::endl<<std::endl;
        break;
      } else if (errno != ECONNREFUSED) {
        std::cerr<<"could not connect to server, due to "
                  <<strerror(errno)<<std::endl;
        goto END_OF_OTHER_CHILDREN;
      }
    }
  }

  while (!curr_test->is_child_end_) {
    // reveive data, now
    int received = read(sock, recv_buf, 4000);
    if (received <= 0) {
      // EOF or error
      break;
    }

		curr_test->ProcessRecvbuf(recv_buf);
  }

END_OF_OTHER_CHILDREN:
  close(sock);

  return NULL;

}

void ThreadTest::ProcessRecvbuf(const char* recv_buf) {
	uint64_t timestamp;
  int tid;
  int action_type;
  int order;
	// the default value for LIB_FUNC is -1.
	int idx = -1;
  int ret_val = -1;
  int ret = -1;
	if (strncmp(recv_buf, "LIB_FUNC_BEGIN[]", strlen("LIB_FUNC_BEGIN[]")) == 0) {
      sscanf(recv_buf + strlen("LIB_FUNC_BEGIN[]"), "%"PRIu64":%d:%d:%d:%d",
              &timestamp, &tid, &action_type, &order, &ret_val);
      log_.InsertLog(timestamp, TestLog::LogItem(tid, 
											static_cast<ThreadActionNestedIn>(action_type),
                      static_cast<ThreadActionOrder>(order), 
											ret_val, ret, idx, TestLog::LogItem::LIB_FUNC_BEGIN));
    } else if (strncmp(recv_buf, "LIB_FUNC_END[]", strlen("LIB_FUNC_END[]")) == 0) {
			// currently, do not have to process this message.
		} else if (strncmp(recv_buf, "USER_FUNC[]", strlen("USER_FUNC[]")) == 0) {
			sscanf(recv_buf + strlen("USER_FUNC[]"), "%"PRIu64":%d:%d:%d:%d:%d",
							&timestamp, &tid, &order, &action_type, &idx, &ret);
			TestLogInsertRet log_ret = log_.InsertLog(timestamp, TestLog::LogItem(tid, 
											static_cast<ThreadActionNestedIn>(action_type),
                      static_cast<ThreadActionOrder>(order), 
											ret_val, ret, idx, TestLog::LogItem::USER_FUNC));

			// kill ASSERT_FAILED process:
			if (log_ret == RET_ASSERT_FAILED) {
				KillChildrenTest();
			}
		} else {
      // wrong message!
      std::cerr<<" wrong format of message. skip it"<<std::endl;
    }
}

// this thread is used to monitor if deadlock occurs.
void* ThreadTest::RunMonitor(void* thread_test) {
  ThreadTest* curr_test = static_cast<ThreadTest*>(thread_test);
 
  while (!curr_test->is_child_end_) {
    while (!curr_test->is_child_end_) {
      // check every 3 seconds.
      sleep(3);
			// check if all the threads are waiting for lock:
      if (curr_test->HasPotentialDeadLock()) {
        break;
      }  
    }

    if (!curr_test->is_child_end_) {
      // double check:
      // after another 3 seconds:
      sleep(3);
			// if suspecious deadlock happens, confirm it
			// checking if the time of the logs preceeds.
      if (curr_test->ConfirmDeadLock()) {
        curr_test->KillChildrenTest();
        break;
      }
    }
  }

  return NULL;
}

bool ThreadTest::HasPotentialDeadLock() {
  // check if all the threads are waiting for lock.

  return log_.IsAllInLockState();
}

bool ThreadTest::ConfirmDeadLock() {
  return (HasPotentialDeadLock() && log_.IsInSuspendState());
}

void ThreadTest::KillChildrenTest() {
  // send a signal to the child process to kill it.
	// TODO: currently, we try sending the signal of SIGTERM.
	// If it doesn't work for some situations, we will change to SIGKILL.
	s_pthread_kill(pid_, SIGTERM);

	log_.SetDeadLock();
}

// TODO:
int ThreadTest::JoinAllThreads() {
	// we modify the log that currently, do nothing special before the child quit.
#if 0
	pthread_t tid;
	void* ret_join = NULL;
	pthread_t (*GetRamThreadId)();
	GetRamThreadId = (pthread_t (*)())dlsym(RTLD_DEFAULT, "GetRamThreadId");
#endif
	// TODO: rewrite the join logic:
	// reasons: 
	// (1) pthread_join typically crashes, if we GDB the threads during their running.
	// (2) some future work also needs to attach (and possibly detach) the threads of the child process
	//     to set some breakpoints and re-write the logic.

	// new logic should be:
	// (1) get the number of running threads of the process: getpid()
	// (2) if (number >= 2) keep waiting
	//     else if (number == 1)
	//         if the thread number == current tid, return
 	//         else: keep waiting infinitely until the parent process WAIT(). 

#if 0
	while ((tid = GetRamThreadId()) != static_cast<pthread_t>(INVALID_TID)) {
		// ignore the return value.
fprintf(stderr, "child join: %llx\n", tid);
  	real_join(tid, &ret_join);	
fprintf(stderr, "child join: %llx end\n", tid);
	}
#endif
	return 0;
}

void ThreadTest::GenerateLogs() {
	thread_test_result_.GenerateLogs(this);
} 

void ThreadTest::DisplayResults(FILE* log_file) {
	// TODO: support different log levels

	// general result:
	thread_test_result_.DisplayGeneralResult(log_file);

	// successful parts:
	thread_test_result_.DisplaySuccessLogs(log_file);
	// failed parts:
	thread_test_result_.DisplayFailureLogs(log_file);
}

ThreadTestGroup::ThreadTestGroup() : is_cleared_(false) {
}

ThreadTestGroup::~ThreadTestGroup() {
	if (!is_cleared_) {
  	for (std::map<std::string, ThreadTest*>::iterator itr = elements_.begin();
    	    itr != elements_.end();
      	  ++itr) {
	    delete itr->second;
  	  itr->second = NULL;
  	}
		is_cleared_ = true;
	}
}

// TODO: judge according to filter
bool ThreadTestGroup::IsRunThisTest(const std::string& name) {
	return true;
}

int ThreadTestGroup::TearDownEnv() {
	// remove sync-points:
	ClearBreakpoints();

	return 0;
}

int ThreadTestGroup::Run() {
  int ret = 0;
  int local_result;

	//fprintf(stderr, "There are altogether %d tests in this part.\n\n", names_.size());

  // according to the order they are defined:
  for (size_t i = 0; i < names_.size(); ++i) {
    //fprintf(stderr, "The %d test is %s\n\n", i + 1, names_[i].c_str());

		if (!IsRunThisTest(names_[i])) {
      continue;
    }
	
		g_thread_action_info.clear();
    if ((local_result = this->SetUpEnv()) != 0) {
      // error with setup
      std::cerr<<"err occurs when setup for parent of "<<names_[i]<<std::endl;
      ret |= local_result;
      goto TEAR_DOWN_PARENT;
    }
/*
    if ((local_result = elements_[names_[i]]->SetUpEnv()) != 0) {
      // error with setup
      std::cerr<<"err occurs when setup for "<<names_[i]<<std::endl;
      ret |= local_result;
      goto TEAR_DOWN_CHILD;
    }
*/
    if ((local_result = elements_[names_[i]]->Run()) != 0) {
      // error with run
      std::cerr<<"err occurs when setup for "<<names_[i]<<std::endl;
      ret |= local_result;
    }
/*
TEAR_DOWN_CHILD: 
    if ((local_result = elements_[names_[i]]->TearDownEnv()) != 0) {
      // error with teardown
      std::cerr<<"err occurs when teardown for "<<names_[i]<<std::endl;
      ret |= local_result;
    }
*/
TEAR_DOWN_PARENT:
    if ((local_result = this->TearDownEnv()) != 0) {
      // error with teardown
      std::cerr<<"err occurs when teardown for parent of "<<names_[i]<<std::endl;
      ret |= local_result;
    }    
  }

  return ret;
}

void ThreadTestGroup::DisplayResults(FILE* log_file) {
	for (size_t i = 0; i < names_.size(); ++i) {
		if (!IsRunThisTest(names_[i])) {
      fprintf(log_file, "\nTest %s has been filtered; no output.\n", names_[i].c_str());
			continue;
    }

		fprintf(log_file, "\nTestCase %s\n", names_[i].c_str());
		
		elements_[names_[i]]->DisplayResults(log_file);
	}
}

ThreadTestContainer* ThreadTestContainer::container_ = NULL;

ThreadTestContainer::ThreadTestContainer() : 
											num_threadaction_items_(0) {
	threadaction_item_.clear();
	g_thread_action_info.clear();

	// assign values to the global function pointers:
	s_pthread_mutex_init = (int (*)(pthread_mutex_t*,
																	const pthread_mutexattr_t*))dlvsym(RTLD_NEXT, 
																																			"pthread_mutex_init", 
																																			"GLIBC_2.2.5");

	s_pthread_cond_init = (int (*)(pthread_cond_t*,
                                  const pthread_condattr_t*))dlvsym(RTLD_NEXT, 
                                                                      "pthread_cond_init", 
                                                                      "GLIBC_2.2.5");

	s_pthread_create = (int (*)(pthread_t*,
                            	const pthread_attr_t*,
                            	void*(*)(void*),
                            	void*))dlvsym(RTLD_NEXT, "pthread_create", "GLIBC_2.2.5");

	s_pthread_self = (pthread_t (*)())dlvsym(RTLD_NEXT, "pthread_self", "GLIBC_2.2.5");

	s_pthread_kill = (int (*)(pthread_t, int))dlvsym(RTLD_NEXT, "pthread_kill", "GLIBC_2.2.5");

	s_pthread_join = (int (*)(pthread_t, void**))dlvsym(RTLD_NEXT, "pthread_join", "GLIBC_2.2.5"); 

	s_pthread_mutex_lock = (int (*)(pthread_mutex_t*))dlvsym(RTLD_NEXT, "pthread_mutex_lock", "GLIBC_2.2.5");

	s_pthread_mutex_unlock = (int (*)(pthread_mutex_t*))dlvsym(RTLD_NEXT, "pthread_mutex_unlock", "GLIBC_2.2.5");

	s_pthread_cond_wait = (int (*)(pthread_cond_t*,
                              	 pthread_mutex_t*))dlvsym(RTLD_NEXT, "pthread_cond_wait", "GLIBC_2.2.5");

	s_pthread_cond_broadcast = (int (*)(pthread_cond_t*))dlvsym(RTLD_NEXT, "pthread_cond_broadcast", "GLIBC_2.2.5");

	s_pthread_cond_signal = (int (*)(pthread_cond_t*))dlvsym(RTLD_NEXT, "pthread_cond_signal", "GLIBC_2.2.5");
}

ThreadTestContainer::~ThreadTestContainer() {
	DeleteTest();
	container_ = NULL;
}

ThreadTestContainer* ThreadTestContainer::GetInstance() {
  if (!container_) {
    container_ = new ThreadTestContainer();
  }

  return container_;
}

void ThreadTestContainer::CollectResults(ThreadTest* test) {
	// currently, there is nothing that has to be done.	
}

// TODO: implement the logic:
void ThreadTestContainer::DisplayResults() {
	// io:
	FILE* log_file = NULL;
	if (!cmd_parser_.log_file_path_.empty()) {
		log_file = fopen(cmd_parser_.log_file_path_.c_str(), "w");
	}
	if (!log_file) {
		log_file = stdout;
	}

	ThreadTestGroup::DisplayResults(log_file);

	if (log_file) {
		fclose(log_file);
	}
}

void ThreadTestContainer::InitEnv(int argc, char** argv) {
	cmd_parser_.ParseCmdLine(argc, argv);
	if (InitGDB(argv[0]) != 0) {
		// afraid that we could not step further due to lack of support 
		// of GDB.
		fprintf(stderr, "GDB init failed. Quit.\n");
		exit(-1);
	}
}

int ThreadTestContainer::InitGDB(const char* prog_name) {
	// TODO: call our GDB API:
	return MakeGDBDoPreparation(prog_name);
}

int RegisterGroupAndCase(const char* group_name, 
                          ThreadTestGroup* test_group,
                          const char* case_name,
                          ThreadTest* test_case) {
	ThreadTestContainer* container = ThreadTestContainer::GetInstance();
	container->AddTest(group_name, test_group);
	test_group->AddTest(case_name, test_case);

	return 0;
}

Tid2Action g_thread_action_info;

// sync-point:
SyncPointHelper::SyncPointHelper() {
	tmp_file_name_.clear();
	tmp_line_num_.clear();
}

SyncPointHelper::~SyncPointHelper() {
}

SyncPointHelper& SyncPointHelper::SetFileName(const char* filename) {
	tmp_file_name_ = filename;	

	return *this;
}

SyncPointHelper& SyncPointHelper::SetLineNumber(const char* linenum) {
	tmp_line_num_ = linenum;

	return *this;
}

int SyncPointHelper::SetSyncPointName(const char* sync_point_name) {
	if (sync_name_2_gdb_bk_.count(sync_point_name) > 0) {
		std::cerr<<"Sync Name "<<sync_point_name<<" has been used. WRONG!"<<std::endl;
		return -1;
	}
	
	// call GDB API to set this breakpoint:

	// TODO: check if "filename" is legal.

	// TODO: check if "linenum" is legal.

	std::string break_point_statement = tmp_file_name_ + ":" + tmp_line_num_;
	tmp_file_name_.clear();
	tmp_line_num_.clear();
	
	if (SetBreakPoint(break_point_statement.c_str()) != 0) {
		std::cerr<<"Could not set breakpoint: "<<break_point_statement<<std::endl;
		return -1;
	}

	int gdb_num = GetNewlyAddedGdbBreakpoint();
	gdb_bk_2_sync_name_[gdb_num] = sync_point_name;
	sync_name_2_gdb_bk_[sync_point_name] = gdb_num;

	return 0;
}

SyncPointHelper& SyncPointHelper::CreateList(OrderListType type) {
	sync_point_order_list_.push_back(SyncPointOrderList(type));	
	is_skip_ = false;

	return *this;
}

SyncPointHelper& SyncPointHelper::AddListNode(const char* sync_point_name) {
	if (is_skip_) {
		goto ADD_LIST_NODE_DONE;
	}

	if (sync_name_2_gdb_bk_.count(sync_point_name) == 0) {
		std::cerr<<"Could not find the definition of "<<sync_point_name
							<<"; adding into sync list fails."<<std::endl;
		is_skip_ = true;
		goto ADD_LIST_NODE_DONE;
	}

	if (sync_point_order_list_.size() == 0) {
		is_skip_ = true;
		goto ADD_LIST_NODE_DONE;
	}
	curr_sync_point_order_list_ = sync_point_order_list_.size() - 1;
	
	sync_point_order_list_[curr_sync_point_order_list_].sync_point_v.push_back(SyncPoint());
	curr_sync_point_index_ = sync_point_order_list_[curr_sync_point_order_list_].
														sync_point_v.size() - 1;

	if (sync_point_index_list_[sync_name_2_gdb_bk_[sync_point_name]].size() == 0) {
		sync_point_index_list_[sync_name_2_gdb_bk_[sync_point_name]].
					push_back(SyncPointOrderIndex(curr_sync_point_order_list_, curr_sync_point_index_));
	} else if (sync_point_index_list_[sync_name_2_gdb_bk_[sync_point_name]]
							[sync_point_index_list_[sync_name_2_gdb_bk_[sync_point_name]].size() - 1].list_num 
							!= curr_sync_point_order_list_) {
		sync_point_index_list_[sync_name_2_gdb_bk_[sync_point_name]].
					push_back(SyncPointOrderIndex(curr_sync_point_order_list_, curr_sync_point_index_));
	} else {
		sync_point_index_list_[sync_name_2_gdb_bk_[sync_point_name]][sync_point_index_list_
					[sync_name_2_gdb_bk_[sync_point_name]].size() - 1].element_num.
					push_back(curr_sync_point_index_);
	}

ADD_LIST_NODE_DONE:
	return *this;
}

SyncPointHelper& SyncPointHelper::SetThreadId(int tid) {
	if (is_skip_) {
		goto SET_THREAD_ID_DONE;
	}

	sync_point_order_list_[curr_sync_point_order_list_].sync_point_v[curr_sync_point_index_].
				thread_num = tid;

SET_THREAD_ID_DONE:
	return *this;
}

SyncPointHelper& SyncPointHelper::SetTimes(int must_run_times) {
	if (is_skip_ || must_run_times < 0) {
		goto SET_TIMES_DONE;
	}

	sync_point_order_list_[curr_sync_point_order_list_].sync_point_v[curr_sync_point_index_].
				remained_check_num = must_run_times;

SET_TIMES_DONE:
	return *this;
}

void SyncPointHelper::PreprocessLists() {
	// TODO:
	// 1. remove all lists of size <= 1

	// 2. mark the last sync point in a circular list as "is_done" 
	for (size_t i = 0; i < sync_point_order_list_.size(); ++i) {
		if (sync_point_order_list_[i].type == CIRCULAR_LIST) {
			if (sync_point_order_list_[i].sync_point_v.size() <= 1) {
				continue;
			}

			sync_point_order_list_[i].sync_point_v[
					sync_point_order_list_[i].sync_point_v.size() - 1].is_done = true;
		}
	}
}

// update running status of the child process:
void SyncPointHelper::Update(int bk, int tid) {
	if (sync_point_index_list_.count(bk) == 0) {
		return;
	}
	SyncPointOrderListIndex& index = sync_point_index_list_[bk];
	for (size_t i_list = 0; i_list < index.size(); ++i_list) {
		int i = index[i_list].list_num;
		for (size_t j_ele = 0; j_ele < index[i_list].element_num.size(); ++j_ele) {
			int j = index[i_list].element_num[j_ele];
			SyncPoint& sync_point = sync_point_order_list_[i].sync_point_v[j];
			if (/*tid != ALL_THREADS && */sync_point.thread_num != ALL_THREADS 
						&& tid != sync_point.thread_num) {
				// thread not matching:
				continue;
			} else if (sync_point.is_done) {
				continue;
			}
			
			++sync_point.hit_num;
			if (sync_point.hit_num >= sync_point.remained_check_num) {
				sync_point.is_done = true;
				if (static_cast<size_t>(j) == sync_point_order_list_[i].sync_point_v.size() - 1
							 && sync_point_order_list_[i].type == SINGLE_LIST) {
					sync_point_order_list_[i].is_done = true;
				} else if (sync_point_order_list_[i].type == CIRCULAR_LIST) {
					// clear previous:
					int prev = (j == 0) ? (sync_point_order_list_[i].sync_point_v.size() - 1) : (j - 1);
					sync_point_order_list_[i].sync_point_v[prev].hit_num = 0;
					sync_point_order_list_[i].sync_point_v[prev].is_done = false;
				}
			}
			break;
		}
	}
}

// check if a sync point could run:
bool SyncPointHelper::Check(int bk, int tid) {
	if (sync_point_index_list_.count(bk) == 0) {
		return true;
	}

	// only when the previous sync points of all related lists are done, 
	// could this sync point run.
	SyncPointOrderListIndex& index = sync_point_index_list_[bk];
	for (size_t i_list = 0; i_list < index.size(); ++i_list) {
		int i = index[i_list].list_num;
		if (sync_point_order_list_[i].is_done) {
			continue;
		}
		for (size_t j_ele = 0; j_ele < index[i_list].element_num.size(); ++j_ele) {
			int j = index[i_list].element_num[j_ele];
			SyncPoint& sync_point = sync_point_order_list_[i].sync_point_v[j];
      if (/*tid != ALL_THREADS && */sync_point.thread_num != ALL_THREADS
            && tid != sync_point.thread_num) {
        continue;
      } else if (sync_point.is_done) {
				// TODO: WARNING: if there are continual same sync points in a certain CIRCULAR list, 
				// the following check could cause that sync point blocked.
				if (sync_point_order_list_[i].type == CIRCULAR_LIST || 
								j_ele == index[i_list].element_num.size() - 1) {
					return false;
				} else  {
					continue;
				}
			}

			if (j == 0) {
				if (sync_point_order_list_[i].type == SINGLE_LIST 
							&& sync_point.hit_num < sync_point.remained_check_num) {
					break;
				} else if (sync_point_order_list_[i].type == CIRCULAR_LIST &&
							sync_point_order_list_[i].sync_point_v[sync_point_order_list_[i].sync_point_v.size() - 1].is_done
							&& sync_point.hit_num < sync_point.remained_check_num) {
					continue;
				} else {
					return false;
				}
			} else if (sync_point_order_list_[i].sync_point_v[j - 1].is_done 
							&& sync_point.hit_num < sync_point.remained_check_num) {
				if (sync_point_order_list_[i].type == SINGLE_LIST) {
					break;
				} else if (sync_point_order_list_[i].type == CIRCULAR_LIST) {
					continue;
				}
			}
		
			return false;	
		}
	}

	return true;
}

extern void SetThreadAction(Tid2Action** tid_action) {
  *tid_action = &g_thread_action_info;
}

