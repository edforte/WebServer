#include "Request.hpp"

Request::Request() : Message(), request_line(), uri() {}

Request::Request(const Request& other)
    : Message(other), request_line(other.request_line), uri(other.uri) {}

Request& Request::operator=(const Request& other) {
  if (this != &other) {
    Message::operator=(other);
    request_line = other.request_line;
    uri = other.uri;
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
    char ch = buffer[i];
    if (ch == '\r') {
      continue;
    }
    if (ch == '\n') {
      lines.push_back(temp);
      temp.clear();
    } else {
      temp.push_back(ch);
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

  // Parse the URI from the request line
  uri.parse(request_line.uri);

  parseHeaders(lines, 1);
  return true;
}
