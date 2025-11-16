#include "common/Logger.hpp"

#include <iomanip>
#include <sstream>
#include <stdexcept>
#include <sys/fcntl.h>
#include <unistd.h>

std::unique_ptr<Logger> Logger::_instance;
std::once_flag Logger::_init_flag;

Logger::Logger(const std::string &log_file_path) {
  _fd = open(log_file_path.c_str(), O_WRONLY | O_CREAT | O_APPEND, 0644);
  if (_fd == -1) {
    throw std::runtime_error("Logger(): Failed to open log file");
  }
}

Logger::~Logger() {
  info("Log file closed");
  close(_fd);
}

void Logger::init(const std::string &file_path) {
  std::call_once(_init_flag, [&]() {
    _instance = std::unique_ptr<Logger>(new Logger(file_path));
  });
  get_instance().info("Log file `" + file_path + "` created");
}

Logger &Logger::get_instance() {
  if (!_instance) {
    throw std::runtime_error(
        "Logger not initialized. Call Logger::init() first.");
  }
  return *_instance;
}

void Logger::log(Level level, const std::string &message) const {
  std::stringstream log_line_ss;

  const auto now = std::chrono::system_clock::now();
  const auto in_time_t = std::chrono::system_clock::to_time_t(now);
  const auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                      now.time_since_epoch()) %
                  1000;

  const std::string level_str = Logger::level_to_string(level);

  log_line_ss << '['
              << std::put_time(std::localtime(&in_time_t), "%Y-%m-%d %H:%M:%S")
              << '.' << std::setw(3) << std::setfill('0') << ms.count() << "] "
              << '[' << Logger::level_to_string(level) << "] "
              << "[pid=" << getpid() << "] " << message << std::endl;

  if (write(_fd, log_line_ss.str().c_str(), log_line_ss.str().size()) == -1) {
    throw std::runtime_error("Logger::log(): Failed to write to file");
  }
}

void Logger::debug(const std::string &message) const {
  log(Level::Debug, message);
}

void Logger::info(const std::string &message) const {
  log(Level::Info, message);
}

void Logger::warn(const std::string &message) const {
  log(Level::Warning, message);
}

void Logger::error(const std::string &message) const {
  log(Level::Error, message);
}

std::string Logger::level_to_string(Level level) {
  switch (level) {
  case Level::Debug:
    return "DEBUG";
  case Level::Info:
    return "INFO";
  case Level::Warning:
    return "WARNING";
  case Level::Error:
    return "ERROR";
  default:
    return "UNKNOWN";
  }
}
