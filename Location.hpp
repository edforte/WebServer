#pragma once

#include <map>
#include <set>
#include <string>

#include "HttpMethod.hpp"
#include "HttpStatus.hpp"

class Location {
 public:
  Location();
  Location(const std::string& path_str);
  Location(const Location& other);
  Location& operator=(const Location& other);
  ~Location();

  // Location path identifier
  std::string path;

  // Location-specific configuration
  std::set<http::Method> allow_methods;
  http::Status redirect_code;
  std::string redirect_location;
  bool cgi;
  std::set<std::string> index;
  bool autoindex;
  std::string root;
  std::map<http::Status, std::string> error_page;
};
