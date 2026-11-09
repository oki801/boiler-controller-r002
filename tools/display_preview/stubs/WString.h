#pragma once
#include <string>
class __FlashStringHelper;
class String {
 public:
  String(const char* s = "") : s_(s) {}
  const char* c_str() const { return s_.c_str(); }
  unsigned int length() const { return (unsigned)s_.size(); }
 private:
  std::string s_;
};
