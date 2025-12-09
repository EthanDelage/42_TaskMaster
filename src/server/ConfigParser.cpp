#include "server/ConfigParser.hpp"

#include "common/utils.hpp"

#include <csignal>
#include <filesystem>
#include <unistd.h>
#include <unordered_set>
#include <yaml-cpp/yaml.h>

#define PROCESS_NAME_MAX_LENGTH 64

static process_config_t parse_process_config(std::string &&name,
                                             const YAML::Node &config_node);
static void parse_cmd(const YAML::Node &config_node,
                      process_config_t &process_config);
static void parse_cmd_path(process_config_t &process_config);
static void parse_workingdir(const YAML::Node &config_node,
                             process_config_t &process_config);
static void parse_stdout(const YAML::Node &config_node,
                         process_config_t &process_config);
static void parse_stderr(const YAML::Node &config_node,
                         process_config_t &process_config);
static void parse_stopsignal(const YAML::Node &config_node,
                             process_config_t &process_config);
static void parse_numprocs(const YAML::Node &config_node,
                           process_config_t &process_config);
static void parse_starttime(const YAML::Node &config_node,
                            process_config_t &process_config);
static void parse_startretries(const YAML::Node &config_node,
                               process_config_t &process_config);
static void parse_stoptime(const YAML::Node &config_node,
                           process_config_t &process_config);
static void parse_umask(const YAML::Node &config_node,
                        process_config_t &process_config);
static void parse_autostart(const YAML::Node &config_node,
                            process_config_t &process_config);
static void parse_autorestart(const YAML::Node &config_node,
                              process_config_t &process_config);
static void parse_env(const YAML::Node &config_node,
                      process_config_t &process_config);
static void parse_exitcodes(const YAML::Node &config_node,
                            process_config_t &process_config);
static bool is_valid_process_name(const std::string &name);
static bool is_directory(std::string path);
static bool is_file_writeable(std::string path);

ConfigParser::ConfigParser(std::string config_path)
    : _config_path(std::move(config_path)) {}

std::unordered_map<std::string, process_config_t> ConfigParser::parse() const {
  std::unordered_map<std::string, process_config_t> process_configs;
  std::unordered_set<std::string> seen_names;

  YAML::Node root = YAML::LoadFile(_config_path);

  if (!root["process"]) {
    throw std::runtime_error("Config: Missing 'process' section in config");
  }
  for (const auto &node : root["process"]) {
    auto process_name = node.first.as<std::string>();
    if (!is_valid_process_name(process_name)) {
      throw std::runtime_error(
          "Config: process names must contain only letters, numbers and "
          "underscores and must be less than " +
          std::to_string(PROCESS_NAME_MAX_LENGTH) + "characters long");
    }
    if (!seen_names.insert(process_name).second) {
      throw std::runtime_error("Config: duplicate process name '" +
                               process_name + "'");
    }
    YAML::Node process_node = node.second;
    // TODO: try catch
    process_config_t process_config =
        parse_process_config(std::move(process_name), process_node);
    process_configs.insert({process_name, std::move(process_config)});
  }
  return process_configs;
}

static process_config_t parse_process_config(std::string &&name,
                                             const YAML::Node &config_node) {
  process_config_t process_config;

  process_config.name = name;
  process_config.cmd =
      std::unique_ptr<wordexp_t, WordexpDestructor>(new wordexp_t);
  parse_cmd(config_node, process_config);
  parse_cmd_path(process_config);
  parse_workingdir(config_node, process_config);
  parse_stdout(config_node, process_config);
  parse_stderr(config_node, process_config);
  parse_stopsignal(config_node, process_config);
  parse_numprocs(config_node, process_config);
  parse_starttime(config_node, process_config);
  parse_startretries(config_node, process_config);
  parse_stoptime(config_node, process_config);
  parse_umask(config_node, process_config);
  parse_autostart(config_node, process_config);
  parse_autorestart(config_node, process_config);
  parse_env(config_node, process_config);
  parse_exitcodes(config_node, process_config);
  return process_config;
}

static void parse_cmd(const YAML::Node &config_node,
                      process_config_t &process_config) {
  if (!config_node["cmd"]) {
    throw std::runtime_error("ProgramConfig: Missing required 'cmd' field");
  }
  if (wordexp(config_node["cmd"].as<std::string>().c_str(),
              process_config.cmd.get(), 0) != 0) {
    throw std::runtime_error("ProgramConfig: Failed to parse 'cmd' field");
  }
}

static void parse_cmd_path(process_config_t &process_config) {
  if (process_config.cmd->we_wordc < 1) {
    process_config.cmd_path = "";
    return;
  }
  std::string cmd(process_config.cmd->we_wordv[0]);
  if (cmd.find('/') != std::string::npos) {
    process_config.cmd_path = cmd;
    return;
  }

  char *env_path = std::getenv("PATH");
  if (env_path == nullptr) {
    throw std::runtime_error("Error: please define the PATH env variable");
  }
  std::vector<std::string> path_list = split(env_path, ':');

  for (const auto &path : path_list) {
    std::string cmd_path = path + '/' + cmd;
    if (access(cmd_path.c_str(), X_OK) == 0) {
      process_config.cmd_path = cmd_path;
      return;
    }
  }
  throw std::runtime_error("Error: command not found: " + cmd);
}

static void parse_workingdir(const YAML::Node &config_node,
                             process_config_t &process_config) {
  if (config_node["workingdir"]) {
    process_config.workingdir = config_node["workingdir"].as<std::string>();
    if (!is_directory(process_config.workingdir)) {
      throw std::runtime_error("ProgramConfig: workingdir: Not a directory (" +
                               process_config.workingdir + ")");
    }
  }
}

static void parse_stdout(const YAML::Node &config_node,
                         process_config_t &process_config) {
  if (!config_node["stdout"]) {
    return;
  }
  process_config.stdout = config_node["stdout"].as<std::string>();
  if (!is_file_writeable(process_config.stdout)) {
    throw std::runtime_error("ProgramConfig: stdout: Invalid permission (" +
                             process_config.stdout + ")");
  }
}

static void parse_stderr(const YAML::Node &config_node,
                         process_config_t &process_config) {
  if (!config_node["stderr"]) {
    return;
  }
  process_config.stderr = config_node["stderr"].as<std::string>();
  if (!is_file_writeable(process_config.stderr)) {
    throw std::runtime_error("ProgramConfig: stderr: Invalid permission (" +
                             process_config.stderr + ")");
  }
}

static void parse_stopsignal(const YAML::Node &config_node,
                             process_config_t &process_config) {
  if (!config_node["stopsignal"]) {
    process_config.stopsignal = SIGINT;
    return;
  }
  std::string signal_string = config_node["stopsignal"].as<std::string>();
  static const std::unordered_map<std::string, int> signal_table = {
      {"HUP", SIGHUP},   {"INT", SIGINT},   {"QUIT", SIGQUIT},
      {"ILL", SIGILL},   {"ABRT", SIGABRT}, {"FPE", SIGFPE},
      {"KILL", SIGKILL}, {"SEGV", SIGSEGV}, {"PIPE", SIGPIPE},
      {"ALRM", SIGALRM}, {"TERM", SIGTERM}, {"USR1", SIGUSR1},
      {"USR2", SIGUSR2}, {"CHLD", SIGCHLD}, {"CONT", SIGCONT},
      {"STOP", SIGSTOP}, {"TSTP", SIGTSTP}, {"TTIN", SIGTTIN},
      {"TTOU", SIGTTOU}};

  auto it = signal_table.find(signal_string);
  if (it == signal_table.end()) {
    throw std::runtime_error("ProgramConfig: Invalid stopsignal value (" +
                             signal_string + ")");
  }
  process_config.stopsignal = it->second;
}

static void parse_numprocs(const YAML::Node &config_node,
                           process_config_t &process_config) {
  process_config.numprocs =
      config_node["numprocs"] ? config_node["numprocs"].as<unsigned long>() : 1;
  if (process_config.numprocs == 0) {
    throw std::runtime_error("ProgramConfig: Invalid numprocs value (" +
                             std::to_string(process_config.numprocs) + ")");
  }
}

static void parse_starttime(const YAML::Node &config_node,
                            process_config_t &process_config) {
  process_config.starttime = config_node["starttime"]
                                 ? config_node["starttime"].as<unsigned long>()
                                 : 0;
}

static void parse_startretries(const YAML::Node &config_node,
                               process_config_t &process_config) {
  process_config.startretries =
      config_node["startretries"]
          ? config_node["startretries"].as<unsigned long>()
          : 0;
}

static void parse_stoptime(const YAML::Node &config_node,
                           process_config_t &process_config) {
  process_config.stoptime =
      config_node["stoptime"] ? config_node["stoptime"].as<unsigned long>() : 3;
  if (process_config.stoptime == 0) {
    throw std::runtime_error("ProgramConfig: Invalid stoptime value (0)");
  }
}

static void parse_umask(const YAML::Node &config_node,
                        process_config_t &process_config) {
  process_config.umask =
      config_node["umask"] ? config_node["umask"].as<mode_t>() : 022;
}

static void parse_autostart(const YAML::Node &config_node,
                            process_config_t &process_config) {
  process_config.autostart =
      config_node["autostart"] ? config_node["autostart"].as<bool>() : true;
}

static void parse_autorestart(const YAML::Node &config_node,
                              process_config_t &process_config) {
  if (!config_node["autorestart"]) {
    process_config.autorestart = AutoRestart::False;
    return;
  }
  std::string value = config_node["autorestart"].as<std::string>();
  std::transform(value.begin(), value.end(), value.begin(), ::tolower);
  if (value == "true") {
    process_config.autorestart = AutoRestart::True;
  } else if (value == "false") {
    process_config.autorestart = AutoRestart::False;
  } else if (value == "unexpected") {
    process_config.autorestart = AutoRestart::Unexpected;
  } else {
    throw std::runtime_error("ProgramConfig: Invalid autorestart value (" +
                             value + ")");
  }
}

static void parse_env(const YAML::Node &config_node,
                      process_config_t &process_config) {
  if (!config_node["env"]) {
    return;
  }
  try {
    for (const auto &entry : config_node["env"]) {
      process_config.env.emplace_back(entry.first.as<std::string>(),
                                      entry.second.as<std::string>());
    }
  } catch (std::exception &e) {
    throw std::runtime_error(std::string("ProgramConfig: Invalid env value: ") +
                             e.what());
  }
}

static void parse_exitcodes(const YAML::Node &config_node,
                            process_config_t &process_config) {
  if (!config_node["exitcodes"]) {
    process_config.exitcodes.push_back(0);
    return;
  }
  for (const auto &entry : config_node["exitcodes"]) {
    process_config.exitcodes.push_back(entry.as<uint8_t>());
  }
}

static bool is_valid_process_name(const std::string &name) {
  if (name.empty() && name.size() <= PROCESS_NAME_MAX_LENGTH) {
    return false;
  }
  return std::all_of(name.begin(), name.end(), [](unsigned char c) {
    return std::isalnum(c) || c == '_';
  });
}

static bool is_file_writeable(std::string path) {
  namespace fs = std::filesystem;

  if (!std::filesystem::exists(path)) {
    return true;
  }
  fs::perms permission = fs::status(path).permissions();
  return (permission & fs::perms::owner_write) != fs::perms::none;
}

static bool is_directory(std::string path) {
  return std::filesystem::is_directory(path);
}

void WordexpDestructor::operator()(wordexp_t *p) const {
  if (p) {
    wordfree(p);
    delete p;
  }
}
