#ifndef PROGRAM_CONFIG_HPP
#define PROGRAM_CONFIG_HPP

#include <cstdint>
#include <vector>
#include <yaml-cpp/yaml.h>
extern "C" {
#include <wordexp.h>
}

enum class AutoRestart { True, False, Unexpected };

struct WordexpDestructor {
  void operator()(wordexp_t *p) const;
};

class ProgramConfig {
public:
  explicit ProgramConfig(std::string name, const YAML::Node &config_node);

  bool operator==(const ProgramConfig& other) const;

  void parse_cmd(YAML::Node config_node);
  void parse_workingdir(YAML::Node config_node);
  void parse_stdout(YAML::Node config_node);
  void parse_stderr(YAML::Node config_node);
  void parse_stopsignal(YAML::Node config_node);
  void parse_numprocs(YAML::Node config_node);
  void parse_starttime(YAML::Node config_node);
  void parse_startretries(YAML::Node config_node);
  void parse_stoptime(YAML::Node config_node);
  void parse_umask(YAML::Node config_node);
  void parse_autostart(YAML::Node config_node);
  void parse_autorestart(YAML::Node config_node);
  void parse_env(YAML::Node config_node);
  void parse_exitcodes(YAML::Node config_node);

  std::string get_name() const;
  char **get_cmd() const;
  std::string get_workingdir() const;
  std::string get_stdout() const;
  std::string get_stderr() const;
  int get_stopsignal() const;
  unsigned long get_numprocs() const;
  unsigned long get_starttime() const;
  unsigned long get_startretries() const;
  unsigned long get_stoptime() const;
  unsigned long get_umask() const;
  bool get_autostart() const;
  AutoRestart get_autorestart() const;
  std::vector<std::pair<std::string, std::string>> get_env() const;
  std::vector<uint8_t> get_exitcodes() const;

private:
  std::string _name;
  std::unique_ptr<wordexp_t, WordexpDestructor> _cmd;
  std::string _workingdir;
  std::string _stdout;
  std::string _stderr;
  int _stopsignal;
  unsigned long _numprocs;
  unsigned long _starttime;
  unsigned long _startretries;
  unsigned long _stoptime;
  unsigned long _umask;
  bool _autostart;
  AutoRestart _autorestart;
  std::vector<std::pair<std::string, std::string>> _env;
  std::vector<uint8_t> _exitcodes;
};

std::ostream &operator<<(std::ostream &os, const ProgramConfig &object);

#endif // PROGRAM_CONFIG_HPP
