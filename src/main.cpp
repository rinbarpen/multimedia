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
  //FFmpegRecorder recorder;
  //RecorderConfig recorder_config;
  //recorder_config.video.width = config.video.width;
  //recorder_config.video.height = config.video.height;
  //recorder_config.video.max_width = config.video.max_width;
  //recorder_config.video.max_height = config.video.max_height;
  //recorder_config.video.sample_aspect_ratio = config.video.sample_aspect_ratio;
  //recorder_config.device = dconfig;
  //recorder_config.common.enable_video = config.common.enable_video;
  //recorder_config.common.enable_audio = config.common.enable_audio;

  //recorder.init(recorder_config);
  //recorder.open(cameraGarb);
  //recorder.record();

  //{
  //  std::this_thread::sleep_for(std::chrono::seconds(5));
  //}

  FFmpegPlayer::is_native_mode = true;
  FFmpegPlayer player(AudioDevice::SDL, VideoDevice::SDL);
  list.setListLoop(true);
  config.common.save_while_playing = true;
  player.init(config);
  player.play(list);
  //player.play(cameraGarb);
  return 0;
}
