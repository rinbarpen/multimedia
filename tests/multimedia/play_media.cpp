#include <csignal>

#include "multimedia/player/FFmpegPlayer.hpp"

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

  av_log_set_level(AV_LOG_QUIET);

  MediaList list;
#if defined(__WIN__)
  list.add(MediaSource{"E:/Data/video/bad_apple.mp4"});
  list.add(MediaSource{"E:/Data/video/1080p U149.mp4"});
#elif defined(__LINUX__)
  list.add(MediaSource{"/home/youmu/Desktop/bad_apple.mp4"});
  list.add(MediaSource{"/home/youmu/Desktop/227.mp4"});
#endif

  PlayerConfig config;
  FFmpegPlayer::is_native_mode = true;
  FFmpegPlayer player(AudioDevice::SDL, VideoDevice::SDL);
  list.setListLoop(true);
  player.init(config);
  player.play(list);  
  return 0;
}
