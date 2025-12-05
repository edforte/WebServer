#include "Uri.hpp"

#include <cctype>
#include <sstream>
#include <vector>

#include "utils/utils.hpp"

namespace http {

Uri::Uri() : port_(-1), valid_(false) {}

Uri::Uri(const std::string& uri) : port_(-1), valid_(false) {
  parse(uri);
}

Uri::Uri(const Uri& other)
    : scheme_(other.scheme_),
      host_(other.host_),
      port_(other.port_),
      path_(other.path_),
      query_(other.query_),
      fragment_(other.fragment_),
      valid_(other.valid_) {}

Uri& Uri::operator=(const Uri& other) {
  if (this != &other) {
    scheme_ = other.scheme_;
    host_ = other.host_;
    port_ = other.port_;
    path_ = other.path_;
    query_ = other.query_;
    fragment_ = other.fragment_;
    valid_ = other.valid_;
  }
  return *this;
}

Uri::~Uri() {}

bool Uri::parse(const std::string& url) {
  // Reset state
  scheme_.clear();
  host_.clear();
  port_ = -1;
  path_.clear();
  query_.clear();
  fragment_.clear();
  valid_ = false;

  if (url.empty()) {
    return false;
  }

  std::string remaining = url;
  std::size_t pos = 0;

  // Check for scheme (e.g., "http://")
  pos = remaining.find("://");
  if (pos != std::string::npos) {
    scheme_ = remaining.substr(0, pos);
    remaining = remaining.substr(pos + 3);

    // Parse host and optional port
    std::size_t path_start = remaining.find('/');
    std::string authority;
    if (path_start != std::string::npos) {
      authority = remaining.substr(0, path_start);
      remaining = remaining.substr(path_start);
    } else {
      authority = remaining;
      remaining = "/";
    }

    // Check for port in authority
    std::size_t port_pos = authority.rfind(':');
    if (port_pos != std::string::npos) {
      std::string port_str = authority.substr(port_pos + 1);
      // Validate port string is not empty
      if (port_str.empty()) {
        return false;  // Invalid URI: empty port
      }
      host_ = authority.substr(0, port_pos);
      // Parse port number using safeStrtoll
      long long port_val;
      if (!safeStrtoll(port_str, port_val)) {
        return false;  // Invalid port format or overflow
      }
      // Validate port range (1-65535)
      if (port_val < 1 || port_val > 65535) {
        return false;  // Port out of valid range
      }
      port_ = static_cast<int>(port_val);
    } else {
      host_ = authority;
    }
  }

  // Parse path, query, and fragment from remaining string
  // Extract fragment first (after #)
  pos = remaining.find('#');
  if (pos != std::string::npos) {
    fragment_ = remaining.substr(pos + 1);
    remaining = remaining.substr(0, pos);
  }

  // Extract query string (after ?)
  pos = remaining.find('?');
  if (pos != std::string::npos) {
    query_ = remaining.substr(pos + 1);
    remaining = remaining.substr(0, pos);
  }

  // What remains is the path
  path_ = remaining;

  // A URI with at least a path is valid
  valid_ = !path_.empty();
  return valid_;
}

std::string Uri::serialize() const {
  std::ostringstream oss;

  if (!scheme_.empty()) {
    oss << scheme_ << "://";
    if (!host_.empty()) {
      oss << host_;
      if (port_ > 0) {
        oss << ":" << port_;
      }
    }
  }

  oss << path_;

  if (!query_.empty()) {
    oss << "?" << query_;
  }

  if (!fragment_.empty()) {
    oss << "#" << fragment_;
  }

  return oss.str();
}

std::string Uri::getPath() const {
  return path_;
}

std::string Uri::getQuery() const {
  return query_;
}

std::string Uri::getFragment() const {
  return fragment_;
}

std::string Uri::getDecodedPath() const {
  return decodePath(path_);
}

bool Uri::hasPathTraversal() const {
  // Decode the path first, then check for ".." sequences
  std::string decoded = getDecodedPath();

  // Check for ".." at various positions
  // 1. Exactly ".."
  if (decoded == "..") {
    return true;
  }

  // 2. Starts with "../"
  if (decoded.size() >= 3 && decoded.substr(0, 3) == "../") {
    return true;
  }

  // 3. Ends with "/.."
  if (decoded.size() >= 3 && decoded.substr(decoded.size() - 3) == "/..") {
    return true;
  }

  // 4. Contains "/../"
  if (decoded.find("/../") != std::string::npos) {
    return true;
  }

  return false;
}

bool Uri::isValid() const {
  return valid_;
}

std::string Uri::getScheme() const {
  return scheme_;
}

std::string Uri::getHost() const {
  return host_;
}

int Uri::getPort() const {
  return port_;
}

int Uri::hexToInt(char c) {
  if (c >= '0' && c <= '9') {
    return c - '0';
  }
  if (c >= 'A' && c <= 'F') {
    return c - 'A' + 10;
  }
  if (c >= 'a' && c <= 'f') {
    return c - 'a' + 10;
  }
  return -1;
}

char Uri::intToHex(int n) {
  if (n >= 0 && n <= 9) {
    return static_cast<char>('0' + n);
  }
  if (n >= 10 && n <= 15) {
    return static_cast<char>('A' + n - 10);
  }
  return '0';
}

std::string Uri::decodeInternal(const std::string& str, bool plusAsSpace) {
  std::string result;
  result.reserve(str.size());

  for (std::size_t i = 0; i < str.size(); ++i) {
    if (str[i] == '%' && i + 2 < str.size()) {
      int high = hexToInt(str[i + 1]);
      int low = hexToInt(str[i + 2]);
      if (high >= 0 && low >= 0) {
        result += static_cast<char>((high << 4) | low);
        i += 2;
        continue;
      }
    }
    if (str[i] == '+' && plusAsSpace) {
      result += ' ';
    } else {
      result += str[i];
    }
  }

  return result;
}

std::string Uri::decode(const std::string& str) {
  // Convenience function - defaults to query string decoding (treats '+' as
  // space). Use decodePath() or decodeQuery() for explicit behavior.
  return decodeQuery(str);
}

std::string Uri::decodePath(const std::string& str) {
  return decodeInternal(str, false);
}

std::string Uri::decodeQuery(const std::string& str) {
  return decodeInternal(str, true);
}

std::string Uri::encode(const std::string& str) {
  std::string result;
  result.reserve(str.size() * 3);  // Worst case: all characters need encoding

  for (std::size_t i = 0; i < str.size(); ++i) {
    unsigned char c = static_cast<unsigned char>(str[i]);

    // Unreserved characters (RFC 3986)
    if (std::isalnum(c) || c == '-' || c == '_' || c == '.' || c == '~') {
      result += static_cast<char>(c);
    } else {
      result += '%';
      result += intToHex((c >> 4) & 0x0F);
      result += intToHex(c & 0x0F);
    }
  }

  return result;
}

std::string Uri::normalizePath(const std::string& path) {
  if (path.empty()) {
    return "/";
  }

  // Decode the path first
  std::string decoded = decodePath(path);

  // Split path into segments
  std::vector<std::string> segments;
  std::string segment;
  bool absolute = (!decoded.empty() && decoded[0] == '/');

  for (std::size_t i = 0; i < decoded.size(); ++i) {
    if (decoded[i] == '/') {
      if (!segment.empty()) {
        if (segment == "..") {
          // Go up one directory if possible
          if (!segments.empty()) {
            segments.pop_back();
          }
        } else if (segment != ".") {
          segments.push_back(segment);
        }
        segment.clear();
      }
    } else {
      segment += decoded[i];
    }
  }

  // Handle trailing segment
  if (!segment.empty()) {
    if (segment == "..") {
      if (!segments.empty()) {
        segments.pop_back();
      }
    } else if (segment != ".") {
      segments.push_back(segment);
    }
  }

  // Rebuild path
  std::string result;
  for (std::size_t i = 0; i < segments.size(); ++i) {
    if (absolute || i > 0) {
      result += "/";
    }
    result += segments[i];
  }

  // Handle empty result
  if (result.empty()) {
    result = "/";
  }

  // Preserve trailing slash if original had it and result isn't just "/"
  // Check the original path, not decoded, since %2F is data, not a delimiter
  if (result.size() > 1 && !path.empty() && path[path.size() - 1] == '/') {
    result += "/";
  }

  return result;
}

}  // namespace http
