#ifndef WORDEXP_WRAPPER_HPP
#define WORDEXP_WRAPPER_HPP

#include <memory>
#include <string>
extern "C" {
  #include <wordexp.h>
}

struct WordexpDestructor {
  void operator()(wordexp_t* p) const;
};

struct WordexpWrapper{
  std::unique_ptr<wordexp_t, WordexpDestructor> ptr;

  WordexpWrapper();
  WordexpWrapper(const WordexpWrapper&) = delete;
  WordexpWrapper& operator=(const WordexpWrapper&) = delete;

  WordexpWrapper(WordexpWrapper&&) noexcept = default;
  WordexpWrapper& operator=(WordexpWrapper&&) noexcept = default;

  void expand(const std::string& cmd);

  char** get_argv() const;
};

#endif // WORDEXP_WRAPPER_HPP
