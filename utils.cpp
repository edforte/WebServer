#include "utils.hpp"

#include <fcntl.h>

#include <cerrno>
#include <cstdlib>
#include <cstring>
#include <stdexcept>
#include <string>

#include "Logger.hpp"
#include "constants.hpp"

int set_nonblocking(int file_descriptor) {
  int flags = fcntl(file_descriptor, F_GETFL, 0);
  if (flags < 0) {
    return -1;
  }
  return fcntl(file_descriptor, F_SETFL, flags | O_NONBLOCK);
}

// Trim whitespace from both ends of a string and return the trimmed copy.
std::string trim_copy(const std::string& str) {
  std::string res = str;
  // left trim
  std::string::size_type idx = 0;
  while (idx < res.size() &&
         (std::isspace(static_cast<unsigned char>(res[idx])) != 0)) {
    ++idx;
  }
  res.erase(0, idx);
  // right trim
  if (!res.empty()) {
    std::string::size_type jdx = res.size() - 1;
    while (jdx > 0 &&
           (std::isspace(static_cast<unsigned char>(res[jdx])) != 0)) {
      --jdx;
    }
    res.erase(jdx + 1);
  }
  return res;
}

void initDefaultHttpMethods(std::set<http::Method>& methods) {
  methods.insert(http::GET);
  methods.insert(http::POST);
  methods.insert(http::PUT);
  methods.insert(http::DELETE);
  methods.insert(http::HEAD);
}

int parseLogLevelFlag(const std::string& arg) {
  // Guard 1: Wrong length
  if (arg.length() != 4) {
    return -1;
  }

  // Guard 2: Wrong prefix
  if (arg.compare(0, 3, "-l:") != 0) {
    return -1;
  }

  // Guard 3: Invalid value
  char level = arg[3];
  if (level < '0' || level > '2') {
    return -1;
  }

  // All good
  return level - '0';
}

// Parse program arguments and fill `path` and `logLevel`.
// This was moved out of main to keep main shorter and clearer.
void processArgs(int argc, char** argv, std::string& path, int& logLevel) {
  logLevel = -1;

  // Parse arguments
  for (int i = 1; i < argc; ++i) {
    std::string arg = argv[i];
    int level = parseLogLevelFlag(arg);

    if (level >= 0) {
      if (logLevel < 0) {
        logLevel = level;
      } else {
        throw std::runtime_error("Error: multiple log level flags provided");
      }
    } else {
      if (path.empty()) {
        path = arg;
      } else {
        throw std::runtime_error("Error: multiple config file paths provided");
      }
    }
  }
  if (logLevel < 0) {
    logLevel = Logger::INFO;  // Default: INFO
  }
  if (path.empty()) {
    path = DEFAULT_CONFIG_PATH;
  }
}

bool safeStrtoll(const std::string& s, long long& out) {
  if (s.empty()) {
    return false;
  }
  errno = 0;
  char* endptr = NULL;
  long long num = std::strtoll(s.c_str(), &endptr, 10);
  // Check for conversion errors: range error, no conversion, or trailing chars
  if (errno == ERANGE || endptr == s.c_str() ||
      (endptr != NULL && *endptr != '\0')) {
    return false;
  }
  out = num;
  return true;
}
