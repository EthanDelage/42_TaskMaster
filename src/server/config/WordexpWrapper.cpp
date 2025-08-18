#include "server/config/WordexpWrapper.hpp"
#include <stdexcept>
extern "C" {
  #include <wordexp.h>
}

void WordexpDestructor::operator()(wordexp_t* p) const {
  if (p) {
    wordfree(p);
  }
}

WordexpWrapper::WordexpWrapper()
  : ptr(new wordexp_t) {}

void WordexpWrapper::expand(const std::string& cmd) {
  if (wordexp(cmd.c_str(), ptr.get(), 0) != 0) {
    throw (std::runtime_error("WordexpWrapper: wordexp()"));
  }
}

