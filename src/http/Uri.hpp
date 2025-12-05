#pragma once

#include <string>

namespace http {

/**
 * A class to parse and manipulate HTTP URIs.
 *
 * Supports parsing URIs with the following components:
 * - scheme (e.g., "http", "https")
 * - host (e.g., "example.com")
 * - port (e.g., 8080)
 * - path (e.g., "/path/to/resource")
 * - query (e.g., "key=value&foo=bar")
 * - fragment (e.g., "section1")
 *
 * Also handles URI encoding/decoding and path traversal detection.
 */
class Uri {
 public:
  Uri();
  explicit Uri(const std::string& uri);
  Uri(const Uri& other);
  Uri& operator=(const Uri& other);
  ~Uri();

  /**
   * Parse a URI string into components.
   * @param uri The URI string to parse
   * @return true if parsing succeeded, false otherwise
   */
  bool parse(const std::string& uri);

  /**
   * Serialize the URI back to a string.
   * @return The serialized URI string
   */
  std::string serialize() const;

  /**
   * Get the path component without the query string.
   * @return The path component
   */
  std::string getPath() const;

  /**
   * Get the query string (without the leading '?').
   * @return The query string
   */
  std::string getQuery() const;

  /**
   * Get the fragment (without the leading '#').
   * @return The fragment
   */
  std::string getFragment() const;

  /**
   * Get the decoded path (URI-decoded).
   * @return The URI-decoded path
   */
  std::string getDecodedPath() const;

  /**
   * Check if the path contains path traversal sequences.
   * This checks the decoded path for ".." sequences.
   * @return true if path traversal is detected, false otherwise
   */
  bool hasPathTraversal() const;

  /**
   * Check if the URI is valid (was successfully parsed).
   * @return true if valid, false otherwise
   */
  bool isValid() const;

  /**
   * URI-decode a string (percent-decoding).
   * @param str The string to decode
   * @return The decoded string
   * @deprecated Use decodePath() or decodeQuery() instead
   */
  static std::string decode(const std::string& str);

  /**
   * URI-decode a path string (percent-decoding).
   * '+' characters are treated as literal '+', not as spaces.
   * @param str The path string to decode
   * @return The decoded path string
   */
  static std::string decodePath(const std::string& str);

  /**
   * URI-decode a query string (percent-decoding).
   * '+' characters are converted to spaces (application/x-www-form-urlencoded).
   * @param str The query string to decode
   * @return The decoded query string
   */
  static std::string decodeQuery(const std::string& str);

  /**
   * URI-encode a string (percent-encoding).
   * @param str The string to encode
   * @return The encoded string
   */
  static std::string encode(const std::string& str);

  /**
   * Normalize a path by resolving "." and ".." components.
   * @param path The path to normalize
   * @return The normalized path
   */
  static std::string normalizePath(const std::string& path);

  // Accessors for URI components
  std::string getScheme() const;
  std::string getHost() const;
  int getPort() const;

 private:
  std::string scheme_;
  std::string host_;
  int port_;
  std::string path_;
  std::string query_;
  std::string fragment_;
  bool valid_;

  /**
   * Convert a hex character to its integer value.
   * @param c The hex character ('0'-'9', 'A'-'F', 'a'-'f')
   * @return The integer value (0-15), or -1 if invalid
   */
  static int hexToInt(char c);

  /**
   * Convert an integer to a hex character.
   * @param n The integer value (0-15)
   * @return The hex character ('0'-'9', 'A'-'F')
   */
  static char intToHex(int n);

  /**
   * Internal URI decoding helper with configurable plus handling.
   * @param str The string to decode
   * @param plusAsSpace Whether to treat '+' as space (true for query strings,
   * false for paths)
   * @return The decoded string
   */
  static std::string decodeInternal(const std::string& str, bool plusAsSpace);
};

}  // namespace http
