#pragma once

#include <cstddef>
#include <fstream>
#include <string>
#include <vector>

namespace cmrc {

class file {
  public:
	file() = default;
	explicit file(std::vector<char>&& data) : data_(std::move(data)) {}

	[[nodiscard]] const char* begin() const { return data_.empty() ? nullptr : data_.data(); }
	[[nodiscard]] const char* end() const { return data_.empty() ? nullptr : data_.data() + data_.size(); }
	[[nodiscard]] std::size_t size() const { return data_.size(); }

  private:
	std::vector<char> data_;
};

class embedded_filesystem {
  public:
	[[nodiscard]] file open(const std::string& path) const {
		std::ifstream in(path, std::ios::binary);
		if (!in.good()) {
			return {};
		}

		std::vector<char> data((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
		return file(std::move(data));
	}
};

}  // namespace cmrc

#define CMRC_DECLARE(name)                \
	namespace cmrc::name {                 \
	inline cmrc::embedded_filesystem get_filesystem() { \
		return {};                         \
	}                                     \
	}
