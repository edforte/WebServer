#pragma once

#include <string>

class Header {
 public:
  Header();
  Header(const std::string& name_str, const std::string& value_str);
  Header(const Header& other);
  Header& operator=(const Header& other);
  ~Header();

  std::string name;
  std::string value;
};
