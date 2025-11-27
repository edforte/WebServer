#pragma once

#include <string>

struct Body {
  Body();
  explicit Body(const std::string& data_str);
  Body(const Body& other);
  Body& operator=(const Body& other);
  ~Body();

  void clear();
  bool empty() const;
  std::size_t size() const;

  std::string data;
};
