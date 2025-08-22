#ifndef CONFIG_HPP
#define CONFIG_HPP

#include <server/config/ProgramConfig.hpp>
#include <string>
#include <vector>

class Config {
public:
  explicit Config(std::string config_path);
  std::vector<ProgramConfig> parse() const;

private:
  std::string _config_path;
};

std::ostream &operator<<(std::ostream &os, const Config &object);

#endif // CONFIG_HPP
