#include "common/utils.hpp"
#include <string>
#include <vector>

std::string join(const std::vector<std::string> &vec, const std::string &sep) {
  return join(vec.begin(), vec.end(), sep);
}
