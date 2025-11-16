#ifndef LOGGER_HPP
#define LOGGER_HPP
#include <memory>
#include <mutex>

class Logger {
public:
  enum class Level { Debug, Info, Warning, Error };

  Logger(const Logger &) = delete;
  Logger &operator=(const Logger &) = delete;
  ~Logger();

  static void init(const std::string &log_file_path);
  static Logger &get_instance();

  void log(Level level, const std::string &message) const;
  void debug(const std::string &message) const;
  void info(const std::string &message) const;
  void warn(const std::string &message) const;
  void error(const std::string &message) const;

private:
  explicit Logger(const std::string &log_file_path);

  static std::string level_to_string(Level level);

  static std::unique_ptr<Logger> _instance;
  static std::once_flag _init_flag;
  int _fd{};
};

#endif // LOGGER_HPP
