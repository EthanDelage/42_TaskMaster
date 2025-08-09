#ifndef CONFIG_HPP
#define CONFIG_HPP

#include <string>
#include <vector>
#include <server/config/ProgramConfig.hpp>

class Config {
public:
  explicit Config(const std::string &filename);
  void print();
private:
  std::vector<ProgramConfig> _programs_config;
};

#endif // CONFIG_HPP

