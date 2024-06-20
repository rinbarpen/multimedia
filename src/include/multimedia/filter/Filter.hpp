#pragma once

class Filter
{
public:
  Filter() = default;
  virtual ~Filter() = default;
  virtual void run() = 0;

protected:
};
