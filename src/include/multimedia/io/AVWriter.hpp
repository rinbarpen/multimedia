#pragma once

#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
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
  AVThread write_thread_;
  AVFrameQueue frames_;
};
