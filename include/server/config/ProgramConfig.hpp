#ifndef PROGRAM_CONFIG_HPP
#define PROGRAM_CONFIG_HPP

#include <string>
#include <vector>
#include <yaml-cpp/yaml.h>

enum class AutoRestart {
  True,
  False,
  Unexpected
};

class ProgramConfig {
public:
  explicit ProgramConfig(const std::string name, const YAML::Node config_node);

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
  void print() const;

  std::string               get_name() const;
  std::string               get_cmd() const;
  std::string               get_workingdir() const;
  std::string               get_stdout() const;
  std::string               get_stderr() const;
  int                       get_stopsignal() const;
  unsigned long             get_numprocs() const;
  unsigned long             get_starttime() const;
  unsigned long             get_startretries() const;
  unsigned long             get_stoptime() const;
  unsigned long             get_umask() const;
  bool                      get_autostart() const;
  AutoRestart               get_autorestart() const;
  std::vector<std::string>  get_env() const;
  std::vector<int>          get_exitcodes() const;

private:
  std::string               _name;
  std::string               _cmd;
  std::string               _workingdir;
  std::string               _stdout;
  std::string               _stderr;
  int                       _stopsignal;
  unsigned long             _numprocs;
  unsigned long             _starttime;
  unsigned long             _startretries;
  unsigned long             _stoptime;
  unsigned long             _umask;
  bool                      _autostart;
  AutoRestart               _autorestart;
  std::vector<std::string>  _env;
  std::vector<int>          _exitcodes;
};

#endif // PROGRAM_CONFIG_HPP

