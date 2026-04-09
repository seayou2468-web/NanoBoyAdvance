#ifndef DATA_READER_H
#define DATA_READER_H

#include <cstring>

#include "blargg_common.h"

class Data_Reader {
public:
  typedef blargg_err_t error_t;

  Data_Reader() : remain_(0) {}
  virtual ~Data_Reader() = default;

  virtual error_t read_avail(void* out, long n, long* out_read) = 0;

  virtual error_t read(void* out, long n) {
    long count = 0;
    const error_t err = read_avail(out, n, &count);
    if (err) {
      return err;
    }
    if (count != n) {
      return "Unexpected end of file";
    }
    return 0;
  }

  virtual error_t skip(long n) {
    char buffer[1024];
    while (n > 0) {
      const long step = (n > (long)sizeof(buffer)) ? (long)sizeof(buffer) : n;
      long count = 0;
      const error_t err = read_avail(buffer, step, &count);
      if (err) {
        return err;
      }
      n -= count;
      if (count <= 0) {
        return "Unexpected end of file";
      }
    }
    return 0;
  }

  long remain() const { return remain_; }
  void set_remain(long remain) { remain_ = remain; }

private:
  long remain_;
};

class Mem_File_Reader : public Data_Reader {
public:
  Mem_File_Reader(const void* data, long size) : data_(static_cast<const unsigned char*>(data)), size_(size), pos_(0) {
    set_remain(size_);
  }

  error_t read_avail(void* out, long n, long* out_read) override {
    if (n < 0) {
      return "Invalid read size";
    }
    const long available = size_ - pos_;
    const long count = (n < available) ? n : available;
    if (count > 0) {
      memcpy(out, data_ + pos_, static_cast<size_t>(count));
      pos_ += count;
      set_remain(size_ - pos_);
    }
    *out_read = count;
    return 0;
  }

private:
  const unsigned char* data_;
  long size_;
  long pos_;
};

#endif
