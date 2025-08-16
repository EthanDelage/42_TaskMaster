#ifndef CONFIG_HPP
#define CONFIG_HPP

#include <server/config/ProgramConfig.hpp>
#include <string>
#include <vector>

class Config {
public:
  explicit Config(std::string filename);
  void parse();
  void clear();
  std::vector<ProgramConfig> get_programs_config() const;

private:
  std::string _config_path;
  std::vector<ProgramConfig> _programs_config;
};

std::ostream &operator<<(std::ostream &os, const Config &object);

#endif // CONFIG_HPP
