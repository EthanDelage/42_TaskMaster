#include "server/config/ProgramConfig.hpp"
#include "server/config/WordexpWrapper.hpp"

#include <cctype>
#include <csignal>
#include <filesystem>
#include <iostream>
#include <stdexcept>
extern "C" {
  #include <wordexp.h>
}

static bool is_file_writeable(std::string path);
static bool is_directory(std::string path);

ProgramConfig::ProgramConfig(std::string name, const YAML::Node &config_node)
    : _name(std::move(name)), _cmd{} {
  parse_cmd(config_node);
  parse_workingdir(config_node);
  parse_stdout(config_node);
  parse_stderr(config_node);
  parse_stopsignal(config_node);
  parse_numprocs(config_node);
  parse_starttime(config_node);
  parse_startretries(config_node);
  parse_stoptime(config_node);
  parse_umask(config_node);
  parse_autostart(config_node);
  parse_autorestart(config_node);
  parse_env(config_node);
  parse_exitcodes(config_node);
}

// ProgramConfig::ProgramConfig(ProgramConfig&& other) noexcept {
//   _name = other._name;
//   _cmd = other._cmd;
//   _workingdir = other._workingdir;
//   _stdout = other._stdout;
//   _stderr = other._stderr;
//   _stopsignal = other._stopsignal;
//   _starttime = other._starttime;
//   _startretries = other._startretries;
//   _stoptime = other._stoptime;
//   _umask = other._umask;
//   _autostart = other._autostart;
//   _autorestart = other._autorestart;
//   _env = other._env;
//   _exitcodes = other._exitcodes;
//   other._cmd = {};
// }
//
// ProgramConfig::ProgramConfig& operator=(ProgramConfig&& other) noexcept {
//   if (this != &other) {
//     wordfree(&_cmd);
//   }
//   return *this;
// }

void ProgramConfig::parse_cmd(YAML::Node config_node) {
  if (!config_node["cmd"]) {
    throw std::runtime_error("ProgramConfig: Missing required 'cmd' field");
  }
  _cmd.expand(config_node["cmd"].as<std::string>());
}

void ProgramConfig::parse_workingdir(YAML::Node config_node) {
  if (config_node["workingdir"]) {
    _workingdir = config_node["workingdir"].as<std::string>();
    if (!is_directory(_workingdir)) {
      throw std::runtime_error("ProgramConfig: workingdir: Not a directory (" +
                               _workingdir + ")");
    }
  }
}

void ProgramConfig::parse_stdout(YAML::Node config_node) {
  if (!config_node["stdout"]) {
    return;
  }
  _stdout = config_node["stdout"].as<std::string>();
  if (!is_file_writeable(_stdout)) {
    throw std::runtime_error("ProgramConfig: stdout: Invalid permission (" +
                             _stdout + ")");
  }
}

void ProgramConfig::parse_stderr(YAML::Node config_node) {
  if (!config_node["stderr"]) {
    return;
  }
  _stderr = config_node["stderr"].as<std::string>();
  if (!is_file_writeable(_stderr)) {
    throw std::runtime_error("ProgramConfig: stderr: Invalid permission (" +
                             _stderr + ")");
  }
}

void ProgramConfig::parse_stopsignal(YAML::Node config_node) {
  if (!config_node["stopsignal"]) {
    _stopsignal = SIGSTOP;
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
  _stopsignal = it->second;
}

void ProgramConfig::parse_numprocs(YAML::Node config_node) {
  _numprocs =
      config_node["numprocs"] ? config_node["numprocs"].as<unsigned long>() : 1;
}

void ProgramConfig::parse_starttime(YAML::Node config_node) {
  _starttime = config_node["starttime"]
                   ? config_node["starttime"].as<unsigned long>()
                   : 0;
}

void ProgramConfig::parse_startretries(YAML::Node config_node) {
  _startretries = config_node["startretries"]
                      ? config_node["startretries"].as<unsigned long>()
                      : 0;
}

void ProgramConfig::parse_stoptime(YAML::Node config_node) {
  _stoptime =
      config_node["stoptime"] ? config_node["stoptime"].as<unsigned long>() : 0;
}

void ProgramConfig::parse_umask(YAML::Node config_node) {
  _umask = config_node["umask"] ? config_node["umask"].as<unsigned long>() : 0;
}

void ProgramConfig::parse_autostart(YAML::Node config_node) {
  _autostart =
      config_node["autostart"] ? config_node["autostart"].as<bool>() : false;
}

void ProgramConfig::parse_autorestart(YAML::Node config_node) {
  if (!config_node["autorestart"]) {
    _autorestart = AutoRestart::False;
    return;
  }
  std::string value = config_node["autorestart"].as<std::string>();
  std::transform(value.begin(), value.end(), value.begin(), ::tolower);
  if (value == "true") {
    _autorestart = AutoRestart::True;
  } else if (value == "false") {
    _autorestart = AutoRestart::False;
  } else if (value == "unexpected") {
    _autorestart = AutoRestart::Unexpected;
  } else {
    throw std::runtime_error("ProgramConfig: Invalid autorestart value (" +
                             value + ")");
  }
}

void ProgramConfig::parse_env(YAML::Node config_node) {
  if (!config_node["env"]) {
    return;
  }
  for (const auto &entry : config_node["env"]) {
    _env.push_back(entry.first.as<std::string>() + "=" +
                   entry.second.as<std::string>());
  } // TODO: test this
}

void ProgramConfig::parse_exitcodes(YAML::Node config_node) {
  if (!config_node["exitcodes"]) {
    return;
  }
  for (const auto &entry : config_node["exitcodes"]) {
    _exitcodes.push_back(entry.as<uint8_t>());
  }
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

std::ostream &operator<<(std::ostream &os, const ProgramConfig &object) {
  os << "  Program: " << object.get_name() << "\n";
  // os << "  Cmd: " << object.get_cmd() << "\n";
  os << "  NumProcs: " << object.get_numprocs() << "\n";
  os << "  Umask: " << std::oct << object.get_umask() << std::dec << "\n";
  os << "  Working Dir: " << object.get_workingdir() << "\n";
  os << "  Autostart: " << (object.get_autostart() ? "true" : "false") << "\n";
  if (object.get_autorestart() == AutoRestart::True) {
    os << "  Autorestart: true" << "\n";
  } else if (object.get_autorestart() == AutoRestart::False) {
    os << "  Autorestart: false" << "\n";
  } else if (object.get_autorestart() == AutoRestart::Unexpected) {
    os << "  Autorestart: unexpected" << "\n";
  }
  os << "  Start Retries: " << object.get_startretries() << "\n";
  os << "  Start Time: " << object.get_starttime() << "\n";
  os << "  Stop Signal: " << object.get_stopsignal() << "\n";
  os << "  Stop Time: " << object.get_stoptime() << "\n";
  os << "  Stdout File: " << object.get_stdout() << "\n";
  os << "  Stderr File: " << object.get_stderr() << "\n";
  os << "  Exit Codes: ";
  for (auto code : object.get_exitcodes()) {
    os << static_cast<int>(code) << " ";
  }
  os << "\n";
  os << "  Env:\n";
  for (const auto &env_var : object.get_env()) {
    os << "    " << env_var << "\n";
  }
  os << "---------------------------\n";
  return os;
}

std::string ProgramConfig::get_name() const { return _name; }

char** ProgramConfig::get_cmd() const { return _cmd.ptr->we_wordv; }

std::string ProgramConfig::get_workingdir() const { return _workingdir; }

std::string ProgramConfig::get_stdout() const { return _stdout; }

std::string ProgramConfig::get_stderr() const { return _stderr; }

int ProgramConfig::get_stopsignal() const { return _stopsignal; }

unsigned long ProgramConfig::get_numprocs() const { return _numprocs; }

unsigned long ProgramConfig::get_starttime() const { return _starttime; }

unsigned long ProgramConfig::get_startretries() const { return _startretries; }

unsigned long ProgramConfig::get_stoptime() const { return _stoptime; }

unsigned long ProgramConfig::get_umask() const { return _umask; }

bool ProgramConfig::get_autostart() const { return _autostart; }

AutoRestart ProgramConfig::get_autorestart() const { return _autorestart; }

std::vector<std::string> ProgramConfig::get_env() const { return _env; }

std::vector<uint8_t> ProgramConfig::get_exitcodes() const { return _exitcodes; }
