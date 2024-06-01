#include "multimedia/FFmpegPlayer.hpp"
#include "multimedia/common/Logger.hpp"
#include "multimedia/recorder/FFmpegRecoder.hpp"
#include <SDL2/SDL.h>
#include <chrono>
#include <csignal>
#include <thread>

void sig_handler(int sig) 
{
  SDL_Quit();
  exit(sig);
}

int main()
{
  signal(SIGINT, sig_handler);
  signal(SIGQUIT, sig_handler);

  std::vector<std::string> list = {"/home/youmu/Desktop/bad_apple.mp4", "/home/youmu/Desktop/227.mp4"};

  // PlayerConfig config;
  // FFmpegPlayer player(AudioDevice::SDL, VideoDevice::SDL);
  // while (config.common.loop) {
  //   for (auto media : list) {
  //     player.init(config);
  //     player.open(media);
  //     player.play();
  //     player.close();
  //   }
  // }

  bool success{false};
  RecordConfig config;
  FFmpegRecorder recorder;
  success = recorder.init(config);
  if (!success) {
    LOG_ERROR_FMT("Failed to initialize FFmpegRecorder");
    return -1;
  }
  std::string url = "USB2.0 HD UVC WebCam";
  std::string shortName = "dshow";
  success = recorder.open(url, shortName);
  if (!success) {
    LOG_ERROR_FMT("Failed to open {} with {}", url, shortName);
    return -1;
  }
  recorder.record();

  std::this_thread::sleep_for(std::chrono::seconds(10));
  recorder.stop();
}
