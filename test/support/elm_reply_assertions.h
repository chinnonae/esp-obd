#pragma once

#include <string>
#include <unity.h>

// Fluent wrapper for readable ELM reply expectations:
//
//   expectReply(engine.execute("0100")).toEqual("41 00 BE 1F B8 10\r\r");
//
// Takes a plain string for now. Add an `ElmReply` overload once T03 defines
// that type, rather than replacing this one.
class ReplyExpectation {
 public:
  explicit ReplyExpectation(std::string actual) : actual_(std::move(actual)) {}

  void toEqual(const std::string& expected) const {
    TEST_ASSERT_EQUAL_STRING(expected.c_str(), actual_.c_str());
  }

 private:
  std::string actual_;
};

inline ReplyExpectation expectReply(std::string actual) {
  return ReplyExpectation(std::move(actual));
}
