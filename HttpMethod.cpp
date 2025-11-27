#include "HttpMethod.hpp"

#include <stdexcept>

namespace http {

std::string methodToString(Method method) {
  switch (method) {
    case GET:
      return "GET";
    case POST:
      return "POST";
    case PUT:
      return "PUT";
    case DELETE:
      return "DELETE";
    case HEAD:
      return "HEAD";
    default:
      return "UNKNOWN";
  }
}

Method stringToMethod(const std::string& method_str) {
  if (method_str == "GET") {
    return GET;
  }
  if (method_str == "POST") {
    return POST;
  }
  if (method_str == "PUT") {
    return PUT;
  }
  if (method_str == "DELETE") {
    return DELETE;
  }
  if (method_str == "HEAD") {
    return HEAD;
  }
  throw std::invalid_argument(std::string("Unknown HTTP method: ") +
                              method_str);
}

}  // namespace http
