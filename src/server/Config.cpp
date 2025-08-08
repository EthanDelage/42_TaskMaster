#include "server/Config.hpp"

#include <yaml-cpp/yaml.h>
#include <iostream>

Config::Config(const std::string &filename) {
  YAML::Node root = YAML::LoadFile(filename);
  if (!root["programs"]) {
    throw std::runtime_error("Missing 'programs' section in config");
  }

  for (const auto &configram : root["programs"]) {
    program_config_t cfg;
    cfg.name = configram.first.as<std::string>();
    auto data = configram.second;

    if (data["cmd"]) cfg.cmd = data["cmd"].as<std::string>();
    if (data["numprocs"]) cfg.numprocs = data["numprocs"].as<unsigned long>();
    if (data["umask"]) cfg.umask = std::stoul(data["umask"].as<std::string>(), nullptr, 8);
    if (data["workingdir"]) cfg.workingdir = data["workingdir"].as<std::string>();
    if (data["autostart"]) cfg.autostart = data["autostart"].as<bool>();
    if (data["autorestart"]) cfg.autorestart = (data["autorestart"].as<std::string>() == "unexpected");
    if (data["exitcodes"]) {
      for (const auto &code : data["exitcodes"]) {
        cfg.exitcodes.push_back(code.as<int>());
      }
    }
    if (data["env"]) {
      for (const auto &e : data["env"]) {
        cfg.env.push_back(e.first.as<std::string>() + "=" + e.second.as<std::string>());
      }
    }
    if (data["startretries"]) cfg.startretries = data["startretries"].as<unsigned long>();
    if (data["starttime"]) cfg.starttime = data["starttime"].as<unsigned long>();
    if (data["stopsignal"]) cfg.stopsignal = data["stopsignal"].as<std::string>();
    if (data["stoptime"]) cfg.stoptime = data["stoptime"].as<unsigned long>();
    if (data["stdout"]) cfg.stdout = data["stdout"].as<std::string>();
    if (data["stderr"]) cfg.stderr = data["stderr"].as<std::string>();

    _programs_config.push_back(std::move(cfg));
  }
}

void Config::print() {
  for (const auto &config : _programs_config) {
    std::cout << "  Program: " << config.name << "\n";
    std::cout << "  Cmd: " << config.cmd << "\n";
    std::cout << "  NumProcs: " << config.numprocs << "\n";
    std::cout << "  Umask: " << std::oct << config.umask << std::dec << "\n";
    std::cout << "  Working Dir: " << config.workingdir << "\n";
    std::cout << "  Autostart: " << (config.autostart ? "true" : "false") << "\n";
    std::cout << "  Autorestart: " << (config.autorestart ? "true" : "false") << "\n";
    std::cout << "  Start Retries: " << config.startretries << "\n";
    std::cout << "  Start Time: " << config.starttime << "\n";
    std::cout << "  Stop Signal: " << config.stopsignal << "\n";
    std::cout << "  Stop Time: " << config.stoptime << "\n";
    std::cout << "  Stdout File: " << config.stdout << "\n";
    std::cout << "  Stderr File: " << config.stderr << "\n";

    std::cout << "  Exit Codes: ";
    for (auto code : config.exitcodes) {
      std::cout << code << " ";
    }
    std::cout << "\n";

    std::cout << "  Env:\n";
    for (const auto &env_var : config.env) {
      std::cout << "    " << env_var << "\n";
    }

    std::cout << "---------------------------\n";
  }
}
