#ifndef STD_FILE_READER_H
#define STD_FILE_READER_H

#include <fstream>

#include "Data_Reader.h"

class Std_File_Reader : public Data_Reader {
public:
  Std_File_Reader() = default;
  ~Std_File_Reader() override { close(); }

  error_t open(const char* path) {
    close();
    file_.open(path, std::ios::binary);
    if (!file_.is_open()) {
      return "Couldn't open file";
    }

    file_.seekg(0, std::ios::end);
    if (!file_) {
      close();
      return "Couldn't seek in file";
    }

    const std::streampos size_pos = file_.tellg();
    if (size_pos < 0) {
      close();
      return "Couldn't seek in file";
    }

    file_.seekg(0, std::ios::beg);
    if (!file_) {
      close();
      return "Couldn't seek in file";
    }

    set_remain(static_cast<long>(size_pos));
    return 0;
  }

  error_t read_avail(void* out, long n, long* out_read) override {
    if (!file_.is_open()) {
      return "File is not open";
    }
    if (n < 0) {
      return "Invalid read size";
    }

    file_.read(static_cast<char*>(out), static_cast<std::streamsize>(n));
    const long count = static_cast<long>(file_.gcount());
    *out_read = count;
    set_remain(remain() - count);

    if (!file_.eof() && file_.fail()) {
      return "Couldn't read from file";
    }
    return 0;
  }

  void close() {
    if (file_.is_open()) {
      file_.close();
    }
    file_.clear();
    set_remain(0);
  }

private:
  std::ifstream file_;
};

#endif
