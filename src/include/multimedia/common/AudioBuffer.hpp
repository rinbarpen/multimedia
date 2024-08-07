#pragma once

#include <cstdint>
#include <cstring>
#include <cassert>
#include <memory>

class AudioBuffer
{
public:
  AudioBuffer(uint32_t capacity)
    : capacity_(capacity) {
    buf_.reset(new uint8_t[capacity_], std::default_delete<uint8_t[]>());
    read_offset_ = 0;
    write_offset_ = 0;
  }
  ~AudioBuffer() = default;

  void fill(const uint8_t *data, uint32_t size) {
    assert(data);
    uint32_t freeSpace = writableBytes();
    if (size > freeSpace) {
      size = freeSpace;
    }
    memcpy(buf_.get() + write_offset_, data, size);
    write_offset_ += size;
  }
  void extract(uint8_t* data, uint32_t size) {
    uint32_t nReadBytes = readableBytes();
    if (size > nReadBytes) {
      size = nReadBytes;
    }
    if (data)
      memcpy(data, buf_.get() + read_offset_, size);
    read_offset_ += size;
    if (read_offset_ >= write_offset_) clear();
  }

  uint8_t *peek() const {
    return buf_.get() + read_offset_;
  }
  uint8_t *data(uint32_t offset = 0) {
    return buf_.get() + offset;
  }
  void clear() {
    read_offset_ = 0;
    write_offset_ = 0;
  }
  void reset() {
    read_offset_ = 0;
    write_offset_ = 0;
    buf_.reset(new uint8_t[capacity_], std::default_delete<uint8_t[]>());
  }

  uint32_t readableBytes() const { return write_offset_ - read_offset_; }
  uint32_t writableBytes() const { return capacity_ - write_offset_; }
  uint32_t size() const { return write_offset_ - read_offset_; }
  uint32_t capacity() const { return capacity_; }

private:
  std::shared_ptr<uint8_t> buf_;
  uint32_t read_offset_;
  uint32_t write_offset_;
  uint32_t capacity_;
};
