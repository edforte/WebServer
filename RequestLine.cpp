#include "RequestLine.hpp"

#include <sstream>

RequestLine::RequestLine() {}

RequestLine::RequestLine(const RequestLine& other)
    : method(other.method), uri(other.uri), version(other.version) {}

RequestLine& RequestLine::operator=(const RequestLine& other) {
  if (this != &other) {
    method = other.method;
    uri = other.uri;
    version = other.version;
  }
  return *this;
}

RequestLine::~RequestLine() {}

std::string RequestLine::toString() const {
  std::ostringstream output;
  output << method << " " << uri << " " << version;
  return output.str();
}

bool RequestLine::parse(const std::string& line) {
  std::istringstream input(line);
  return !!(input >> method >> uri >> version);
}
