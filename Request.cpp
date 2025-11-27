#include "Request.hpp"

Request::Request() {}

Request::Request(const Request& other)
    : Message(other), request_line(other.request_line) {}

Request& Request::operator=(const Request& other) {
  if (this != &other) {
    Message::operator=(other);
    request_line = other.request_line;
  }
  return *this;
}

Request::~Request() {}

std::string Request::startLine() const {
  return request_line.toString();
}

bool Request::parseStartAndHeaders(const std::string& buffer,
                                   std::size_t headers_pos) {
  if (headers_pos == std::string::npos) {
    return false;
  }

  std::vector<std::string> lines;
  std::string temp;
  for (std::size_t i = 0; i < headers_pos; ++i) {
    char curr_char = buffer[i];
    if (curr_char == '\r') {
      continue;
    }
    if (curr_char == '\n') {
      lines.push_back(temp);
      temp.clear();
    } else {
      temp.push_back(curr_char);
    }
  }

  if (!temp.empty()) {
    lines.push_back(temp);
  }

  if (lines.empty()) {
    return false;
  }
  if (!request_line.parse(lines[0])) {
    return false;
  }
  parseHeaders(lines, 1);
  return true;
}
