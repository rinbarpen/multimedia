#pragma once

#include "Filter.hpp"

class SpeedFilter : public Filter
{
public:
  SpeedFilter() = default;
  ~SpeedFilter() = default;

  void run() override;
  
private:

};