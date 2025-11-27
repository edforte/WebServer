#pragma once

#include <string>

namespace http {
enum Method { GET, POST, PUT, DELETE, HEAD };

std::string methodToString(Method method);
Method stringToMethod(const std::string& method_str);

}  // namespace http
