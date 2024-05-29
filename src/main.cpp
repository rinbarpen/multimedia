#include "multimedia/FFmpegPlayer.hpp"
#include <SDL2/SDL.h>
#include <csignal>

void sig_handler(int sig) 
{
  SDL_Quit();
  exit(sig);
}

int main()
{
  signal(SIGINT, sig_handler);
  signal(SIGQUIT, sig_handler);

  PlayerConfig config;
  FFmpegPlayer player(AudioDevice::SDL, VideoDevice::SDL);
  player.init(config);
  player.open("/home/youmu/Desktop/bad_apple.mp4");
  // player.open("/home/youmu/Desktop/227.mp4");
  player.play();
  player.stop();
}
