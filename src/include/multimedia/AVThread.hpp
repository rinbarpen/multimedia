#pragma once

#include "multimedia/common/Thread.hpp"

class AVThread : public Thread
{
public:
  AVThread(std::string_view name) : Thread(name) {}
  virtual ~AVThread() { this->stop(); }
};
