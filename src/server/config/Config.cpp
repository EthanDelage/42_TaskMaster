#include "server/config/Config.hpp"

#include <yaml-cpp/yaml.h>
#include <iostream>

Config::Config() {}

void Config::parse(const std::string &filename) {
  std::string program_name;
  YAML::Node root = YAML::LoadFile(filename);

  if (!root["programs"]) {
    throw std::runtime_error("Config: Missing 'programs' section in config");
  }
  for (const auto &program : root["programs"]) {
    program_name = program.first.as<std::string>();
    YAML::Node config_node = program.second;
    // TODO: try catch
    ProgramConfig program_config(program_name, config_node);

    _programs_config.push_back(program_config);
  }
}

void Config::clear(const std::string &filename) {
  _programs_config.clear();
}

std::ostream& operator<<(std::ostream& os, const Config& object) {
  for (const auto &config : object.get_programs_config()) {
    os << config;
  }
  return os;
}

std::vector<ProgramConfig> Config::get_programs_config() const {
  return _programs_config;
}

