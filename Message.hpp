#pragma once

#include <string>
#include <vector>

#include "Body.hpp"
#include "Header.hpp"

class Message {
 public:
  Message();
  Message(const Message& other);
  Message& operator=(const Message& other);
  virtual ~Message();

  void addHeader(const std::string& name, const std::string& value);
  bool getHeader(const std::string& name, std::string& out) const;
  std::vector<std::string> getHeaders(const std::string& name) const;

  void setBody(const Body& body_obj);
  Body& getBody();
  const Body& getBody() const;

  std::string serializeHeaders() const;
  static bool parseHeaderLine(const std::string& line, Header& out);

  virtual std::string startLine() const = 0;
  virtual std::string serialize() const;

 protected:
  std::vector<Header> headers;
  Body body;

  std::size_t parseHeaders(const std::vector<std::string>& lines,
                           std::size_t start);
};
