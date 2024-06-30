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

  DeviceConfig dconfig;
  dconfig.grabber.draw_mouse = 0;
  MediaSource desktopGarb = {"desktop", "gdigrab", dconfig};
  // MediaSource chromeGarb = {"video=chrome.exe", "gdigrab", dconfig};

  PlayerConfig config;
  FFmpegPlayer::is_native_mode = true;
  FFmpegPlayer player(AudioDevice::SDL, VideoDevice::SDL);
  //config.common.save_while_playing = true;
  config.common.track_mode = true;
  // config.common.save_file = "screen-capture.mp4";
  player.init(config);  
  player.play(desktopGarb);
  return 0;
}
