#pragma once

#include <cstdint>
#include <vector>

#ifdef __WIN__
# include <D3D9.h>
# include <D3DX9.h>

struct Monitor
{
  uint64_t low_part, high_part;
  int width;
  int height;
  int xleft;
  int ytop;
};

class D3D9
{
public:
  static std::vector<Monitor> monitors() {
    std::vector<Monitor> monitors;
    HRESULT hr{D3D_OK};
    IDirect3D9Ex *d3d9ex = nullptr;
    hr = Direct3DCreate9Ex(D3D_SDK_VERSION, &d3d9ex);
    if (FAILED(hr)) {
      return monitors;
    }

    int adapterCount = d3d9ex->GetAdapterCount();
    monitors.reserve(adapterCount);
    for (int i = 0; i < adapterCount; ++i) {
      Monitor monitor;
      memset(&monitor, 0, sizeof(Monitor));

      LUID luid{0,0};
      hr = d3d9ex->GetAdapterLUID(i, &luid);

      HMONITOR hMonitor = d3d9ex->GetAdapterMonitor(i);
      if (hMonitor == nullptr) {
        continue;
      }
      MONITORINFO monitorinfo;
      monitorinfo.cbSize = sizeof(MONITORINFO);
      if (!GetMonitorInfoA(hMonitor, &monitorinfo)) {
        continue;
      }
      monitor.width = monitorinfo.rcMonitor.right - monitorinfo.rcMonitor.left;
      monitor.height = monitorinfo.rcMonitor.bottom - monitorinfo.rcMonitor.top;
      monitor.xleft = monitorinfo.rcMonitor.left;
      monitor.ytop = monitorinfo.rcMonitor.top;
      monitor.low_part = luid.LowPart;
      monitor.high_part = luid.HighPart;
      monitors.push_back(monitor);
    }

    d3d9ex->Release();
    return monitors;
  }
};

#endif
