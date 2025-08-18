#include "server/config/Config.hpp"

#include <iostream>
#include <yaml-cpp/yaml.h>

static bool is_valid_program_name(const std::string &name);

Config::Config(std::string config_path)
    : _config_path(std::move(config_path)) {}

std::vector<ProgramConfig> Config::parse() {
  std::string program_name;
  std::vector<ProgramConfig> program_configs;
  YAML::Node root = YAML::LoadFile(_config_path);

  if (!root["programs"]) {
    throw std::runtime_error("Config: Missing 'programs' section in config");
  }
  for (const auto &program : root["programs"]) {
    program_name = program.first.as<std::string>();
    if (is_valid_program_name(program_name) == false) {
      throw std::runtime_error("Config: programs names must contain only "
                               "letters, numbers and underscores");
    }
    YAML::Node config_node = program.second;
    // TODO: try catch
    ProgramConfig program_config(program_name, config_node);

    program_configs.push_back(std::move(program_config));
  }
  return program_configs;
}

static bool is_valid_program_name(const std::string &name) {
  if (name.empty()) {
    return false;
  }
  return std::all_of(name.begin(), name.end(), [](unsigned char c) {
    return std::isalnum(c) || c == '_';
  });
}
