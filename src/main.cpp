#include <csignal>
#include <chrono>
#include <thread>

#include "multimedia/FFmpegPlayer.hpp"
#include "multimedia/MediaList.hpp"
#include "multimedia/MediaSource.hpp"
#include "multimedia/Player.hpp"
#include "multimedia/common/Logger.hpp"
#include "multimedia/recorder/FFmpegRecoder.hpp"

#define SDL_MAIN_HANDLED
#include <SDL2/SDL.h>

void sig_handler(int sig) {
  SDL_Quit();
  exit(sig);
}

int main(int argc, char *argv[]) {
  signal(SIGINT, sig_handler);
  signal(SIGTERM, sig_handler);

  ffinit();

  DeviceConfig dconfig;
  dconfig.grabber.draw_mouse = 1;
  dconfig.is_camera = true;
  MediaSource cameraGarb = {"video=USB2.0 HD UVC WebCam", "dshow", dconfig};
  MediaSource desktopGarb = {"desktop", "gdigrab", dconfig};

  MediaList list;
//  list.add(MediaSource{"/home/youmu/Desktop/227.mp4"});
//  list.add(MediaSource{"/home/youmu/Desktop/bad_apple.mp4"});
  list.add(MediaSource{"E:/Data/video/bad_apple.mp4"});
  list.add(MediaSource{"E:/Data/video/1080p U149.mp4"});

  PlayerConfig config;
  FFmpegPlayer::is_native_mode = true;
  FFmpegPlayer player(AudioDevice::SDL, VideoDevice::SDL);
  config.common.loop = LoopType::LOOP_LIST;
  config.common.auto_read_next_media = true;
  config.common.save_while_playing = true;
  player.init(config);
  //player.play(list);
  player.play(cameraGarb);
  return 0;
}
