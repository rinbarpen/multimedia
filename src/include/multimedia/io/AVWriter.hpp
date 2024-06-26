#pragma once

#include <string>
#include "multimedia/AVQueue.hpp"
#include "multimedia/AVThread.hpp"

class AVWriter
{
public:
  AVWriter();
  ~AVWriter();

  void open(AVFormatContext *pFormatContext);
  void close();

  void write(AVFramePtr pFrame);

private:
  void onWrite();

  bool openInputStream();
  bool openOutputStream();

private:
  std::string output_filename_;
};
