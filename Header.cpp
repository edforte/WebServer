#include "Header.hpp"

Header::Header() {}

Header::Header(const std::string& name_str, const std::string& value_str)
    : name(name_str), value(value_str) {}

Header::Header(const Header& other) : name(other.name), value(other.value) {}

Header& Header::operator=(const Header& other) {
  if (this != &other) {
    name = other.name;
    value = other.value;
  }
  return *this;
}

Header::~Header() {}
