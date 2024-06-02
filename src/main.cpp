#include <csignal>
#include <chrono>
#include <thread>
#include "multimedia/FFmpegPlayer.hpp"
#include "multimedia/common/Logger.hpp"
#include "multimedia/recorder/FFmpegRecoder.hpp"
#define SDL_MAIN_HANDLED
#include <SDL2/SDL.h>

void sig_handler(int sig) 
{
  SDL_Quit();
  exit(sig);
}

int main(int argc, char *argv[]) {
  signal(SIGINT, sig_handler);
  signal(SIGTERM, sig_handler);

  init();

  std::vector<std::string> list = {"E:/Data/video/1080p U149.mp4"
                                   , "E:/Data/video/bad_apple.mp4"};
  //std::string url = "video=USB2.0 HD UVC WebCam";
  //std::string shortName = "dshow";
  std::string url = "desktop";
  std::string shortName = "gdigrab";

  PlayerConfig config;
  FFmpegPlayer player(AudioDevice::SDL, VideoDevice::SDL);
  //player.init(config);
  //if (player.openDevice(url, shortName)) {
  //  LOG_INFO_FMT("Playing {} with {}", url, shortName);
  //  player.play();
  //  player.close();
  //  LOG_INFO_FMT("Closing {} with {}", url, shortName);
  //}
  while (config.common.loop) {
    for (auto media : list) {
      player.init(config);
      player.open(media);
      player.play();
      player.close();
    }
  }

  
  //bool success{false};
  //RecordConfig config;
  //FFmpegRecorder recorder;
  //success = recorder.init(config);
  //if (!success) {
  //  LOG_ERROR_FMT("Failed to initialize FFmpegRecorder");
  //  return -1;
  //}
  //success = recorder.open(url, shortName);
  //if (!success) {
  //  LOG_ERROR_FMT("Failed to open {} with {}", url, shortName);
  //  return -1;
  //}
  //recorder.record();

  //std::this_thread::sleep_for(std::chrono::seconds(10));
  //recorder.stop();
  return 0;
}
