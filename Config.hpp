#pragma once

#include <string>
#include <vector>

#include "BlockNode.hpp"
#include "DirectiveNode.hpp"
#include "HttpStatus.hpp"
#include "Server.hpp"

class Config {
 public:
  Config();
  ~Config();
  Config(const Config& other);
  Config& operator=(const Config& other);

  void parseFile(const std::string& path);
  std::vector<Server> getServers(void);
  void debug(void) const;

 private:
  std::vector<std::string> tokens_;
  BlockNode root_;
  std::vector<Server> servers_;
  std::map<http::Status, std::string> global_error_pages_;
  std::size_t global_max_request_body_;
  size_t idx_;
  static const size_t kGlobalContext = static_cast<size_t>(-1);
  size_t current_server_index_;
  std::string current_location_path_;

  // Parsing helpers
  static void removeComments(std::string& str);
  void tokenize(const std::string& content);
  bool eof() const;
  const std::string& peek() const;
  std::string get();
  bool isBlock() const;
  BlockNode parseBlock();
  DirectiveNode parseDirective();

  // Argument parsers
  int parsePortValue_(const std::string& portstr);
  bool parseBooleanValue_(const std::string& value);
  http::Method parseHttpMethod_(const std::string& method);
  http::Status parseRedirectCode_(const std::string& value);
  std::size_t parsePositiveNumber_(const std::string& value);
  // Return-style parse helpers (convert+validate and return the value)
  std::set<http::Method> parseMethods(const std::vector<std::string>& args);
  std::map<http::Status, std::string> parseErrorPages(
      const std::vector<std::string>& args);
  std::pair<http::Status, std::string> parseRedirect(
      const std::vector<std::string>& args);
  http::Status parseStatusCode_(const std::string& value);
  struct ListenInfo {
    in_addr_t host;
    int port;
  };
  ListenInfo parseListen(const std::string& listen_arg);

  // Argument count validators
  // Throw if directive does not have at least n arguments
  void requireArgsAtLeast_(const DirectiveNode& directive, size_t num) const;
  // Throw if directive does not have exactly n arguments
  void requireArgsEqual_(const DirectiveNode& directive, size_t num) const;

  // Validate that an integer status code is a 4xx or 5xx error; throws on
  // invalid
  // Construct, log and throw the standardized invalid error_page status code
  // message. Kept separate to avoid duplicate message construction.
  // Validate that a status enum is a 4xx or 5xx error; throws on invalid
  void validateErrorPageCode_(http::Status code) const;
  // Construct, log and throw the standardized invalid error_page status code
  // message. Kept separate to avoid duplicate message construction.
  void throwInvalidErrorPageCode_(http::Status code) const;

  // Construct, log and throw a standardized message for unrecognized
  // directives. The `context` string is appended after the message
  // (e.g. "in server block" or "global directive").
  void throwUnrecognizedDirective_(const DirectiveNode& directive,
                                   const std::string& context) const;

  std::string configErrorPrefix() const;

  // Translation/building methods
  void translateServerBlock_(const BlockNode& server_block, Server& srv,
                             size_t server_index);
  void translateLocationBlock_(const BlockNode& location_block, Location& loc);
};
