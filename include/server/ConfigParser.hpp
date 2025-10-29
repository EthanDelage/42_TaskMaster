#ifndef CONFIG_HPP
#define CONFIG_HPP

#include <string>
#include <vector>
#include <yaml-cpp/yaml.h>
extern "C" {
#include <wordexp.h>
}

enum class AutoRestart { True, False, Unexpected };

struct WordexpDestructor {
  void operator()(wordexp_t *p) const;
};

typedef struct process_config_s {
  std::string name;
  std::unique_ptr<wordexp_t, WordexpDestructor> cmd;
  std::string cmd_path;
  std::string workingdir;
  std::string stdout;
  std::string stderr;
  int stopsignal;
  unsigned long numprocs;
  unsigned long starttime;
  unsigned long startretries;
  unsigned long stoptime;
  unsigned long umask;
  bool autostart;
  AutoRestart autorestart;
  std::vector<std::pair<std::string, std::string>> env;
  std::vector<uint8_t> exitcodes;
}process_config_t;

class ConfigParser {
public:
  explicit ConfigParser(std::string config_path);
  std::vector<process_config_t> parse() const;

private:
  std::string _config_path;

};

#endif // CONFIG_HPP
