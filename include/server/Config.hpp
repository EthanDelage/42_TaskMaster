#ifndef CONFIG_HPP
#define CONFIG_HPP

#include <string>
#include <vector>

typedef struct program_config_s {
  std::string               name;
  std::string               cmd;
  std::vector<std::string>  env;
  std::string               workingdir;
  std::string               stdout;
  std::string               stderr;
  std::string               stopsignal;
  std::vector<int>          exitcodes;
  unsigned long             numprocs;
  unsigned long             starttime;
  unsigned long             startretries;
  unsigned long             stoptime;
  unsigned long             umask;
  bool                      autostart;
  bool                      autorestart;
} program_config_t;

class Config {
public:
  explicit Config(const std::string &filename);
  void print();
private:
  std::vector<program_config_s> _programs_config;
};

#endif // CONFIG_HPP
