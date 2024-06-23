#include <string>
#include <vector>
#include "multimedia/filter/SpeedFilter.hpp"

int SpeedFilter::run(AVFramePtr in, AVFramePtr out) {
  size_t twiceSpeedN = 0;
  while (speed_ > 2.0f) {
    speed_ /= 2.0f;
    twiceSpeedN++;
  }
  size_t halfSpeedN = 0;
  while (speed_ < 0.5f) {
    speed_ /= 0.5f;
    halfSpeedN++;
  }

  std::vector<AVFilter*> filters{std::max(twiceSpeedN, halfSpeedN)};
  Filter *pRemainFilter;
  auto graph = avfilter_graph_alloc();
  if (speed_ != 1.0f) {
    std::string name = "setpts=" + std::to_string(1.0f / speed_) + "*PTS";
  }

  // twice = "setpts=0.5*PTS";
  // half = "setpts=2.0*PTS";


  return 0;
}
