#ifndef CONFIG_HPP
#define CONFIG_HPP

#include <string>
#include <unordered_map>
#include <vector>
#include <yaml-cpp/yaml.h>
extern "C" {
#include <wordexp.h>
}

enum class AutoRestart { True, False, Unexpected };

typedef struct {
  std::string name;
  std::vector<std::string> cmd;
  std::string cmd_path;
  std::string workingdir;
  std::string stdout;
  std::string stderr;
  int stopsignal;
  unsigned long numprocs;
  unsigned long starttime;
  unsigned long startretries;
  unsigned long stoptime;
  mode_t umask;
  bool autostart;
  AutoRestart autorestart;
  std::vector<std::pair<std::string, std::string>> env;
  std::vector<uint8_t> exitcodes;
} process_config_t;

class ConfigParser {
public:
  explicit ConfigParser(std::string config_path);
  std::unordered_map<std::string, process_config_t> parse() const;

private:
  std::string _config_path;
};

#endif // CONFIG_HPP
