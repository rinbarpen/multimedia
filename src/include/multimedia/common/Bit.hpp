#pragma once

class Bit 
{
public:
  Bit() = default;
  Bit(bool value) : value_(value) {}
  ~Bit() = default;

  void set() { value_ = true; }
  void unset() { value_ = false; }
  bool get() const { return value_; } 

  operator bool() { return value_; }
  operator bool() const { return value_; }

private:
  bool value_{false};
};
