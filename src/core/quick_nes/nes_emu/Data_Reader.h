#ifndef DATA_READER_H
#define DATA_READER_H

#include <stdio.h>
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

class Std_File_Reader : public Data_Reader {
public:
  Std_File_Reader() : file_(nullptr) {}
  ~Std_File_Reader() override { close(); }

  error_t open(const char* path) {
    close();
    file_ = fopen(path, "rb");
    if (!file_) {
      return "Couldn't open file";
    }
    if (fseek(file_, 0, SEEK_END) != 0) {
      close();
      return "Couldn't seek in file";
    }
    const long size = ftell(file_);
    if (size < 0 || fseek(file_, 0, SEEK_SET) != 0) {
      close();
      return "Couldn't seek in file";
    }
    set_remain(size);
    return 0;
  }

  error_t read_avail(void* out, long n, long* out_read) override {
    if (!file_) {
      return "File is not open";
    }
    const long count = (long)fread(out, 1, (size_t)n, file_);
    *out_read = count;
    set_remain(remain() - count);
    if (ferror(file_)) {
      return "Couldn't read from file";
    }
    return 0;
  }

  void close() {
    if (file_) {
      fclose(file_);
      file_ = nullptr;
    }
    set_remain(0);
  }

private:
  FILE* file_;
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
