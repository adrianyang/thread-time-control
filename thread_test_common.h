#ifndef _THREAD_TEST_COMMON_H__
#define _THREAD_TEST_COMMON_H__

#include "thread_test_wrapper.h"


#define TT_ATTRIBUTE_UNUSED_ __attribute__ ((unused))

#define TT_DISALLOW_ASSIGN_(type)\
  void operator=(type const &)

#define TT_DISALLOW_COPY_AND_ASSIGN_(type)\
  type(type const &);\
  TT_DISALLOW_ASSIGN_(type)


// all the internal classes are defined here:

class TestInfo {
 public:
  ~TestInfo();

  const char* test_case_name() const { return test_case_name_.c_str(); }
  const char* test_name() const { return test_name_.c_str(); }
  const char* type_param() const { 
    if (type_param_.get() != NULL) {
      return type_param_->c_str();
    }

    return NULL;
  }
  const char* value_param() const {
    if (value_param_.get() != NULL) {
      return value_param_->c_str();
    }

    return NULL;
  }
  bool should_run() const { return should_run_; }
  const TestResult* result() const { return &result_; }

 private:
  friend class Test;
  friend class TestCase;
  friend class ThreadTestImpl;
  friend TestInfo* MakeAndRegisterTestInfo(
                    const char* test_case_name,
                    const char* test_name,
                    const char* type_param,
                    const char* value_param,
                    TypeId fixture_class_id,
                    SetUpTestCaseFunc set_up_tc,
                    TearDownTestCaseFunc tear_down_tc,
                    TestFactoryBase* factory);

  TestInfo(const std::string& test_case_name,
            const std::string& test_name,
            const char* a_type_param,
            const char* a_value-param,
            TypeId fixture_class_id,
            TestFactoryBase* factory);

  void Run();

  const std::string test_case_name_;
  const std::string test_name_;
  const scoped_ptr<const std::string> type_param_;
  const scoped_ptr<const std::string> value_param_;
  const TypeId fixture_class_id_;
  bool should_run_;


  TestFactoryBase* const factory_;

  TT_DISALLOW_COPY_AND_ASSIGN(TestInfo);
};











#endif
