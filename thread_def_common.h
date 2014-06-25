#ifndef __THREAD_DEF_COMMON_H__
#define __THREAD_DEF_COMMON_H__

#include <pthread.h>

#include <iostream>
#include <string>
#include <vector>
#include <list>
#include <map>

#define __STDC_FORMAT_MACROS
#include <inttypes.h>

enum ThreadActionFreq {
  ONLY_ONCE,
  ALWAYS,

	FINISHED,
};

typedef bool (*Func)(void*);


// TODO: 
// (1) need to match resource pointers
// (2) 'NULL' could match resource
struct ThreadResource {
	ThreadResource() : mutex(NULL),
											cond(NULL) { }

	ThreadResource(pthread_mutex_t* m, pthread_cond_t* c) :
											mutex(m), cond(c) { } 

	bool operator==(const ThreadResource& resource) const {
		return ((mutex == resource.mutex || !mutex || !resource.mutex)
				 && (cond == resource.cond || !cond || !resource.cond)); 
	}

  pthread_mutex_t* mutex;
  pthread_cond_t* cond;

  // possibly also others
};

enum ThreadActionType {
	DEFAULT_TYPE,
	EXPECT_FUNC_TYPE,
	ASSERT_FUNC_TYPE,

	// TODO: also others...
};

// a struct for actions to be done during mutex_lock/unlock
// and cond_wait/signal
// as the value of a map (map thread_id to thread_action)
struct ThreadAction {
	ThreadAction(ThreadActionFreq frequence,
								Func func_action,
								void* argument,
								ThreadResource thread_resource,
								ThreadActionType action_type,
								std::string file,
								std::string line) :
									freq(frequence),
									action(func_action),
									para(argument),
									resource(thread_resource),
									type(action_type),
									file_name(file),
									line_num(line) {
	}

  ThreadActionFreq freq;
  Func action;
  void* para;
	ThreadResource resource;
	ThreadActionType type;

	// debug info:
	std::string file_name;
	std::string line_num; 

	// int id; // implicit variable: kept in the index info
						 // of vector<ThreadAction>
};

enum ThreadActionOrder {
  BEFORE_CALLED = 0,
  AFTER_CALLED,
	BOTH_BEFORE_AND_AFTER_CALLED,

	// for allocating memory:
	THREAD_ACTION_ORDER_SIZE,
};

enum ThreadActionNestedIn {
	INVALID_THREAD_ACTION = -1,

  THREAD_CREATE = 0,

  MUTEX_INIT,
  MUTEX_DESTROY,
  MUTEX_LOCK,
  MUTEX_UNLOCK,

  COND_INIT,
  COND_DESTROY,
  COND_WAIT,
  COND_TIMEDWAIT,
  COND_BROADCAST,
  COND_SIGNAL,


  // also a lot of others...
};

// defined in "thread_test.cpp"
extern const char* kThreadActionNestedIn[];

// the index of this vector should be within the range of 
// "enum ThreadActionOrder"
// random access.
typedef std::vector< std::map<ThreadActionNestedIn, std::vector<ThreadAction> > >
          ThreadActionInfo;

#define INVALID_TID -1
#define ALL_THREADS -2

typedef std::map<int, ThreadActionInfo>
          Tid2Action;

//int QueryThreadId(pthread_t thread_id);

extern "C" {
extern void SetThreadAction(Tid2Action** tid_action);
}


#endif

