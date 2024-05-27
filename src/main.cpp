#include "multimedia/FFmpegPlayer.hpp"

int main()
{
  PlayerConfig config;
  FFmpegPlayer player(AudioDevice::SDL, VideoDevice::SDL);
  player.init(config);
  player.open("/home/youmu/Desktop/bad_apple.mp4");
  player.play();
  player.stop();
}
