#pragma once

#include <type_traits>
#include "multimedia/filter/Filter.hpp"
#include "multimedia/filter/Converter.hpp"
#include "multimedia/filter/Resampler.hpp"
#include "multimedia/filter/SpeedFilter.hpp"

class FilterFactory {
public:
  template <class F, typename... T>
  static auto create(T ...value) -> typename F::ptr {
    static_assert(std::is_base_of_v<Filter, F>, "F should inhert Filter");
    return F::create(value...);
  }

};
