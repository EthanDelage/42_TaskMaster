#include "common/Logger.hpp"

#include "common/socket/Socket.hpp"

#include <iomanip>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <sys/fcntl.h>
#include <unistd.h>

#define COLOR_RESET "\033[0m"
#define COLOR_RED "\033[31m"
#define COLOR_GREEN "\033[32m"
#define COLOR_YELLOW "\033[33m"
#define COLOR_BLUE "\033[34m"
#define COLOR_MAGENTA "\033[35m"
#define COLOR_CYAN "\033[36m"
#define COLOR_WHITE "\033[37m"

#define DEBUG_COLOR COLOR_GREEN
#define INFO_COLOR COLOR_BLUE
#define WARN_COLOR COLOR_YELLOW
#define ERROR_COLOR COLOR_RED

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

void Logger::log(Level level, const std::string &message) {
  std::stringstream log_line_ss;

  const auto now = std::chrono::system_clock::now();
  const auto in_time_t = std::chrono::system_clock::to_time_t(now);
  const auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                      now.time_since_epoch()) %
                  1000;

  log_line_ss << '['
              << std::put_time(std::localtime(&in_time_t), "%Y-%m-%d %H:%M:%S")
              << '.' << std::setw(3) << std::setfill('0') << ms.count() << "] "
              << '[' << level << "] "
              << "[pid=" << getpid() << "] " << message;
  if (message[message.length() - 1] != '\n') {
    log_line_ss << std::endl;
  }

  std::lock_guard lock(_mutex);
  if (Socket::write(_fd, log_line_ss.str()) == -1) {
    throw std::runtime_error("Logger::log(): Failed to write to file");
  }
  if (level == Level::Error) {
    std::cerr << log_level_to_color(level) << log_line_ss.str() << COLOR_RESET << std::flush;
  }
#ifdef LOG_TO_STDOUT
  else {
    std::cout << log_level_to_color(level) << log_line_ss.str() << COLOR_RESET << std::flush;
  }
#endif
}

void Logger::debug(const std::string &message) { log(Level::Debug, message); }

void Logger::info(const std::string &message) { log(Level::Info, message); }

void Logger::warn(const std::string &message) { log(Level::Warning, message); }

void Logger::error(const std::string &message) { log(Level::Error, message); }

std::string Logger::log_level_to_color(Level level) const {
  switch (level) {
  case Logger::Level::Debug:
    return DEBUG_COLOR;
  case Logger::Level::Info:
    return INFO_COLOR;
  case Logger::Level::Warning:
    return WARN_COLOR;
  case Logger::Level::Error:
    return ERROR_COLOR;
  default:
    return "";
  }
}

std::ostream &operator<<(std::ostream &os, const Logger::Level &level) {
  switch (level) {
  case Logger::Level::Debug:
    os << "DEBUG";
    break;
  case Logger::Level::Info:
    os << "INFO";
    break;
  case Logger::Level::Warning:
    os << "WARNING";
    break;
  case Logger::Level::Error:
    os << "ERROR";
    break;
  default:
    os << "UNKNOWN";
    break;
  }
  return os;
}
