#include "Body.hpp"

Body::Body() {}

Body::Body(const std::string& data_str) : data(data_str) {}

Body::Body(const Body& other) : data(other.data) {}

Body& Body::operator=(const Body& other) {
  if (this != &other) {
    data = other.data;
  }
  return *this;
}

Body::~Body() {}

void Body::clear() {
  data.clear();
}

bool Body::empty() const {
  return data.empty();
}

std::size_t Body::size() const {
  return data.size();
}
