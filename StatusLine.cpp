#include "StatusLine.hpp"

#include <sstream>

#include "HttpStatus.hpp"
#include "constants.hpp"

StatusLine::StatusLine()
    : version(HTTP_VERSION),
      status_code(http::S_200_OK),
      reason(http::reasonPhrase(http::S_200_OK)) {}

StatusLine::StatusLine(const StatusLine& other)
    : version(other.version),
      status_code(other.status_code),
      reason(other.reason) {}

StatusLine& StatusLine::operator=(const StatusLine& other) {
  if (this != &other) {
    version = other.version;
    status_code = other.status_code;
    reason = other.reason;
  }
  return *this;
}

StatusLine::~StatusLine() {}

std::string StatusLine::toString() const {
  std::ostringstream output;
  output << version << " " << status_code << " " << reason;
  return output.str();
}

bool StatusLine::parse(const std::string& line) {
  std::istringstream input(line);
  int code = 0;
  if (!(input >> version >> code)) {
    return false;
  }
  try {
    status_code = http::intToStatus(code);
  } catch (const std::invalid_argument&) {
    return false;
  }
  std::getline(input, reason);
  if (!reason.empty() && reason[0] == ' ') {
    reason.erase(0, 1);
  }
  return true;
}
