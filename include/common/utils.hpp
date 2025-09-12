#ifndef UTILS_HPP
#define UTILS_HPP

#include <sstream>
#include <string>
#include <vector>

std::vector<std::string> split(const std::string &str, char delimiter);
std::string join(const std::vector<std::string> &vec, const std::string &sep);

template <typename Iter>
std::string join(Iter begin, Iter end, const std::string &separator) {
  std::ostringstream oss;
  if (begin != end) {
    oss << *begin;
    ++begin;
  }
  while (begin != end) {
    oss << separator << *begin;
    ++begin;
  }
  return oss.str();
}

#endif // UTILS_HPP
