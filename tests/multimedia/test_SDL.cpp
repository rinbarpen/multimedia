#pragma once

#include <gtest/gtest.h>
#include "multimedia/FFmpegPlayer.hpp"

class TestFFmpegPlayer : FFmpegPlayer{
public:
    void openAudioSDL() {
        this->openSDL(true);
    }
private:
};

TEST(TestSDL, TestSDLOpen) 
{
    TestFFmpegPlayer player;
    player.openAudioSDL();
}