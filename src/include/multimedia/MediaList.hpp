#pragma once

#include <algorithm>
#include <cstddef>
#include <random>
#include <vector>
#include "multimedia/MediaSource.hpp"

class MediaList
{
public:
  MediaList() = default;
  MediaList(const std::vector<MediaSource> &sources) 
    : sources_(sources) {
  }
  MediaList(std::vector<MediaSource> &&sources)
    : sources_(std::move(sources)) {
  }
  ~MediaList() = default;

  void add(const MediaSource &source) {
    sources_.push_back(source);
  }
  void add(const std::vector<MediaSource> &sources) {
    sources_.reserve(sources_.size() + sources.size()); 
    for (auto &source : sources) {
      sources_.push_back(source);
    }
  }
  void insert(size_t index, const MediaSource &sources) {
    sources_.insert(sources_.begin() + index, sources);
  }
  void remove(size_t index) { 
    sources_.erase(sources_.begin() + index); 
  }
  MediaSource get(size_t index) const { return sources_[index]; }

  void shuttle() {
    std::shuffle(sources_.begin(), sources_.end(), std::mt19937{});
  }

  void clear() {
    sources_.clear();
    index_ = 0;
  }
  void rewind() {
    index_ = 0;
  }

  size_t current() const { return index_; }
  void prev() { index_ = (index_ + sources_.size() - 1) % sources_.size(); }
  void next() { index_ = (index_ + 1) % sources_.size(); }

  size_t size() const { return sources_.size(); }
  bool isEmpty() const { return sources_.empty(); }

private:
  std::vector<MediaSource> sources_;
  size_t index_{0};
};
