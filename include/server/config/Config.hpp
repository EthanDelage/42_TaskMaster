#ifndef CONFIG_HPP
#define CONFIG_HPP

#include <string>
#include <vector>
#include <server/config/ProgramConfig.hpp>

class Config {
public:
  explicit Config(const std::string& filename);
  void parse();
  void clear();
  std::vector<ProgramConfig> get_programs_config() const;
private:
  static bool is_valid_program_name(const std::string& name);

  std::string _config_path;
  std::vector<ProgramConfig> _programs_config;
};

std::ostream& operator<<(std::ostream& os, const Config& object);

#endif // CONFIG_HPP

