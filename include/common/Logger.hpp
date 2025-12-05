#ifndef LOGGER_HPP
#define LOGGER_HPP
#include <memory>
#include <mutex>

#ifdef DEBUG
#define LOG_TO_STDOUT
#endif

class Logger {
public:
  enum class Level { Debug, Info, Warning, Error };

  Logger(const Logger &) = delete;
  Logger &operator=(const Logger &) = delete;
  ~Logger();

  static void init(const std::string &log_file_path);
  static Logger &get_instance();

  void log(Level level, const std::string &message);
  void debug(const std::string &message);
  void info(const std::string &message);
  void warn(const std::string &message);
  void error(const std::string &message);

private:
  explicit Logger(const std::string &log_file_path);

  std::string log_level_to_color(Level level) const;
  static std::unique_ptr<Logger> _instance;
  static std::once_flag _init_flag;
  int _fd{};
  std::mutex _mutex;
};

std::ostream &operator<<(std::ostream &os, const Logger::Level &level);

#endif // LOGGER_HPP
