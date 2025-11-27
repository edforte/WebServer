#include "Message.hpp"

#include <cctype>
#include <sstream>

#include "constants.hpp"
#include "utils.hpp"

namespace {
bool ci_equal_copy(const std::string& str_a, const std::string& str_b) {
  if (str_a.size() != str_b.size()) {
    return false;
  }
  for (std::string::size_type i = 0; i < str_a.size(); ++i) {
    char char_a = str_a[i];
    char char_b = str_b[i];
    char_a =
        static_cast<char>(std::tolower(static_cast<unsigned char>(char_a)));
    char_b =
        static_cast<char>(std::tolower(static_cast<unsigned char>(char_b)));
    if (char_a != char_b) {
      return false;
    }
  }
  return true;
}
}  // namespace

/* Message */
Message::Message() {}

Message::Message(const Message& other)
    : headers(other.headers), body(other.body) {}

Message& Message::operator=(const Message& other) {
  if (this != &other) {
    headers = other.headers;
    body = other.body;
  }
  return *this;
}

Message::~Message() {}

void Message::addHeader(const std::string& name, const std::string& value) {
  headers.push_back(Header(name, value));
}

bool Message::getHeader(const std::string& name, std::string& out) const {
  for (std::vector<Header>::const_iterator it = headers.begin();
       it != headers.end(); ++it) {
    if (ci_equal_copy(it->name, name)) {
      out = it->value;
      return true;
    }
  }
  return false;
}

std::vector<std::string> Message::getHeaders(const std::string& name) const {
  std::vector<std::string> res;
  for (std::vector<Header>::const_iterator it = headers.begin();
       it != headers.end(); ++it) {
    if (ci_equal_copy(it->name, name)) {
      res.push_back(it->value);
    }
  }
  return res;
}

void Message::setBody(const Body& body_obj) {
  body = body_obj;
}

Body& Message::getBody() {
  return body;
}

const Body& Message::getBody() const {
  return body;
}

std::string Message::serializeHeaders() const {
  std::ostringstream output;
  for (std::vector<Header>::const_iterator it = headers.begin();
       it != headers.end(); ++it) {
    output << it->name << ": " << it->value << CRLF;
  }
  return output.str();
}

bool Message::parseHeaderLine(const std::string& line, Header& out) {
  std::string::size_type pos = line.find(':');
  if (pos == std::string::npos) {
    return false;
  }
  out.name = trim_copy(line.substr(0, pos));
  out.value = trim_copy(line.substr(pos + 1));
  return true;
}

std::string Message::serialize() const {
  std::ostringstream output;
  output << startLine() << CRLF;
  output << serializeHeaders();
  output << CRLF;
  output << body.data;
  return output.str();
}

std::size_t Message::parseHeaders(const std::vector<std::string>& lines,
                                  std::size_t start) {
  std::size_t count = 0;
  for (std::vector<std::string>::size_type i = start; i < lines.size(); ++i) {
    const std::string& line = lines[i];
    if (line.empty()) {
      continue;
    }
    Header header;
    if (parseHeaderLine(line, header)) {
      headers.push_back(header);
      ++count;
    }
  }
  return count;
}
