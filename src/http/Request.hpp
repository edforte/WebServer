#pragma once

#include "Message.hpp"
#include "RequestLine.hpp"
#include "Uri.hpp"

class Request : public Message {
 public:
  Request();
  Request(const Request& other);
  Request& operator=(const Request& other);
  virtual ~Request();

  RequestLine request_line;
  http::Uri uri;  // Parsed URI from request_line.uri

  virtual std::string startLine() const;
  bool parseStartAndHeaders(const std::string& buffer, std::size_t headers_pos);
};
