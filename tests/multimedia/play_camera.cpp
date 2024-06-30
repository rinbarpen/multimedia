#include <csignal>

#include "multimedia/device/Device.hpp"
#include "multimedia/common/FFmpegUtil.hpp"
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
  dconfig.is_camera = true;
  MediaSource cameraGarb = {"video=USB2.0 HD UVC WebCam", "dshow", dconfig};

  PlayerConfig config;
  FFmpegPlayer::is_native_mode = true;
  FFmpegPlayer player(AudioDevice::SDL, VideoDevice::SDL);
  config.common.track_mode = true;
  player.init(config);
  player.play(cameraGarb);
  return 0;
}
