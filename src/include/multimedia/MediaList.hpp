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
  void pushBack(const MediaSource &source) { 
    sources_.push_back(source);
  }
  void popBack() { 
    sources_.pop_back();
  }
  MediaSource get(size_t index) const { return sources_[index]; }

  void skipTo(const MediaSource& source) { 
    for (auto i = 0; i < sources_.size(); ++i) {
      if (source.getUrl() == sources_[i].getUrl()) {
        index_ = i;
        return;
      }
    }
    throw std::invalid_argument(fmt::format("No media source: {}, {}", source.getUrl(), source.getDeviceName()));
  }

  void shuttle() {
    std::shuffle(sources_.begin(), sources_.end(), std::mt19937{});
  }

  void setListLoop(bool isLoop) { 
    is_list_loop_ = isLoop;
  }
  void setSingleLoop(bool isLoop) { 
    is_single_loop_ = isLoop;
  }

  void clear() {
    sources_.clear();
    index_ = 0;
  }
  void rewind() {
    index_ = 0;
  }

  MediaSource current() const { return sources_[index_]; }
  size_t currentIndex() const { return index_; }
  void prev() { 
    if (is_single_loop_) return;
    if (is_list_loop_) {
      index_ = (index_ + sources_.size() - 1) % sources_.size();
    }
    else {
      index_--;
    }
  }
  void next() { 
    if (is_single_loop_) return;
    if (is_list_loop_) {
      index_ = (index_ + 1) % sources_.size();
    }
    else {
      index_++; 
    }
  }

  size_t size() const { return sources_.size(); }
  bool isEmpty() const { return sources_.empty(); }

private:
  std::vector<MediaSource> sources_;
  size_t index_{0};
  bool is_list_loop_{false};
  bool is_single_loop_{false};
};
