#pragma once

#include <cerrno>
#include <cstring>
#include <sstream>
#include <string>

class Logger {
 public:
  enum LogLevel { DEBUG, INFO, ERROR };

  // Logger can also be used as a temporary RAII stream object. This allows
  // usage like: LOG(INFO) << "message"; A temporary Logger is constructed
  // with the message location and level, its stream() is used to build the
  // message, and the destructor forwards the composed message to the static
  // logging backend.
  Logger(LogLevel level, const char* file, int line);
  ~Logger();

  // Disable copying since this is a temporary RAII object
  Logger(const Logger&);
  Logger& operator=(const Logger&);

  std::ostringstream& stream();

 private:
  // Instance fields used by the temporary RAII Logger
  LogLevel msgLevel_;
  const char* file_;
  int line_;
  std::ostringstream stream_;

  // Static logging configuration and helpers
  static LogLevel level_;

  static std::string getCurrentTime();
  static std::string levelToString(LogLevel level);

 public:
  static void setLevel(LogLevel level);
  static void log(LogLevel level, const std::string& message);
  static void debug(const std::string& message);
  static void info(const std::string& message);
  static void error(const std::string& message);
  static void printStartupLevel();
};

// Macro for convenient logging using the nested LogStream helper
#define LOG(level) Logger(Logger::level, __FILE__, __LINE__).stream()

// Convenience macro to log an errno-related error with strerror(errno).
// It uses the normal LOG(...) temporary Logger so file/line are included
// automatically by the Logger destructor.
#define LOG_PERROR(level, msg) \
  LOG(level) << (msg) << ": " << std::strerror(errno);
