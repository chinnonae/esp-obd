#pragma once

#include <cstddef>
#include <cstring>

// Fixed-capacity, no-heap string used for ELM reply/line text -- see
// docs/ARCHITECTURE.md's `ElmReply`. Overflow is silently truncated rather
// than allocating or throwing; callers that might overflow it are
// responsible for keeping content within Capacity.

template <size_t Capacity>
class FixedString {
 public:
  FixedString() { buffer_[0] = '\0'; }
  FixedString(const char* text) { assign(text); }

  void assign(const char* text) {
    size_t len = std::strlen(text);
    if (len > Capacity) {
      len = Capacity;
    }
    std::memcpy(buffer_, text, len);
    buffer_[len] = '\0';
    length_ = len;
  }

  FixedString& operator+=(const char* text) {
    size_t addLen = std::strlen(text);
    size_t room = Capacity - length_;
    if (addLen > room) {
      addLen = room;
    }
    std::memcpy(buffer_ + length_, text, addLen);
    length_ += addLen;
    buffer_[length_] = '\0';
    return *this;
  }

  FixedString& operator+=(char c) {
    if (length_ < Capacity) {
      buffer_[length_++] = c;
      buffer_[length_] = '\0';
    }
    return *this;
  }

  const char* c_str() const { return buffer_; }
  size_t size() const { return length_; }
  bool empty() const { return length_ == 0; }
  void clear() {
    length_ = 0;
    buffer_[0] = '\0';
  }

  bool operator==(const FixedString& other) const {
    return length_ == other.length_ && std::memcmp(buffer_, other.buffer_, length_) == 0;
  }
  bool operator==(const char* other) const { return std::strcmp(buffer_, other) == 0; }

 private:
  char buffer_[Capacity + 1];
  size_t length_ = 0;
};
