#include "Response.hpp"

Response::Response() {}

Response::Response(const Response& other)
    : Message(other), status_line(other.status_line) {}

Response& Response::operator=(const Response& other) {
  if (this != &other) {
    Message::operator=(other);
    status_line = other.status_line;
  }

  return *this;
}

Response::~Response() {}

std::string Response::startLine() const {
  return status_line.toString();
}

bool Response::parseStartAndHeaders(const std::vector<std::string>& lines) {
  if (lines.empty()) {
    return false;
  }
  if (!status_line.parse(lines[0])) {
    return false;
  }
  parseHeaders(lines, 1);
  return true;
}
