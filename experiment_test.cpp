#include "experiment.h"
#include "thread_test.h"

#include <vector>
#include <string>

extern std::vector<std::string> g_input_buffers;
extern std::string g_output_buffer;
extern int g_output_buffer_size;

class ExperimentTest : public ThreadTestGroup {
 protected:
 	virtual int SetUpEnv() { return 0; }

	virtual int TearDownEnv() { return 0; }

 public:
};

THREAD_TEST(ExperimentTest, TestCase) {
  SET_SYNCPOINT_AT_FILE(experiment.cpp).AT_LINENO(13).WITH_NAME(sync1); // 108
  SET_SYNCPOINT_AT_FILE(experiment.cpp).AT_LINENO(21).WITH_NAME(sync2); // 152
	IN_SINGLE_LIST.ORDER_SYNCPOINT(sync2).AT_THREAD_ID(2).TIMES(5).ORDER_SYNCPOINT(sync1).AT_THREAD_ID(1).TIMES(6);
//	IN_SINGLE_LIST.ORDER_SYNCPOINT(sync2).AT_THREAD_ID(2).TIMES(3).ORDER_SYNCPOINT(sync1).AT_THREAD_ID(1).TIMES(5).ORDER_SYNCPOINT(sync2).AT_THREAD_ID(2).TIMES(2).ORDER_SYNCPOINT(sync1).AT_THREAD_ID(1);
//  IN_CIRCULAR_LIST.ORDER_SYNCPOINT(sync2).AT_THREAD_ID(2).TIMES(1).ORDER_SYNCPOINT(sync1).AT_THREAD_ID(1).TIMES(1);
//  IN_CIRCULAR_LIST.ORDER_SYNCPOINT(sync2).AT_THREAD_ID(2).TIMES(4).ORDER_SYNCPOINT(sync1).AT_THREAD_ID(1).TIMES(4);
  BEGIN_THREAD_TEST();
  CREATE_THREAD(CreateThread());
  END_THREAD_TEST();
}


int main(int argc, char** argv) {
	RUN_THREAD_TESTS(argc, argv);

	return 0;
}
