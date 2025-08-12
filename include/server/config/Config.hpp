#ifndef CONFIG_HPP
#define CONFIG_HPP

#include <string>
#include <vector>
#include <server/config/ProgramConfig.hpp>

class Config {
public:
  explicit Config();
  void parse(const std::string& filename);
  void clear(const std::string &filename);
  std::vector<ProgramConfig> get_programs_config() const;
private:
  std::vector<ProgramConfig> _programs_config;
};

std::ostream& operator<<(std::ostream& os, const Config& object);

#endif // CONFIG_HPP

