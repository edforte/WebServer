#include "Config.hpp"

#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/stat.h>

#include <cctype>
#include <cerrno>
#include <climits>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <string>

#include "HttpMethod.hpp"
#include "HttpStatus.hpp"
#include "Logger.hpp"
#include "utils.hpp"

// ==================== PUBLIC METHODS ====================

Config::Config()
    : global_max_request_body_(0),
      idx_(0),
      current_server_index_(kGlobalContext) {}

Config::~Config() {}

Config::Config(const Config& other)
    : tokens_(other.tokens_),
      root_(other.root_),
      servers_(other.servers_),
      global_error_pages_(other.global_error_pages_),
      global_max_request_body_(other.global_max_request_body_),
      idx_(other.idx_),
      current_server_index_(other.current_server_index_),
      current_location_path_(other.current_location_path_) {}

Config& Config::operator=(const Config& other) {
  if (this != &other) {
    tokens_ = other.tokens_;
    idx_ = other.idx_;
    root_ = other.root_;
    servers_ = other.servers_;
    global_error_pages_ = other.global_error_pages_;
    global_max_request_body_ = other.global_max_request_body_;
    current_server_index_ = other.current_server_index_;
    current_location_path_ = other.current_location_path_;
  }
  return *this;
}

void Config::parseFile(const std::string& path) {
  LOG(INFO) << "Starting to parse config file: " << path;

  // read file
  std::ifstream file(path.c_str());
  if (!file.is_open()) {
    LOG(ERROR) << "Unable to open config file: " << path;
    throw std::runtime_error(std::string("Unable to open config file: ") +
                             path);
  }
  std::stringstream buffer;
  buffer << file.rdbuf();
  std::string content = buffer.str();

  LOG(DEBUG) << "File content size: " << content.size() << " bytes";

  removeComments(content);
  LOG(DEBUG) << "Comments removed, tokenizing...";

  tokenize(content);
  LOG(INFO) << "Tokenization complete. Total tokens: " << tokens_.size();

  root_.type = "root";
  while (!eof()) {
    if (isBlock()) {
      LOG(DEBUG) << "Found block '" << peek() << "', parsing...";
      root_.sub_blocks.push_back(parseBlock());
    } else {
      LOG(DEBUG) << "Found global directive: " << peek();
      root_.directives.push_back(parseDirective());
    }
  }
  LOG(INFO) << "Config file parsed successfully. Server blocks found: "
            << root_.sub_blocks.size();
}

std::vector<Server> Config::getServers(void) {
  LOG(INFO) << "Validating configuration before building servers";

  // Ensure there is at least one server block
  if (root_.sub_blocks.empty()) {
    std::ostringstream oss;
    oss << configErrorPrefix() << "No server blocks defined";
    std::string msg = oss.str();
    LOG(ERROR) << msg;
    throw std::runtime_error(msg);
  }

  // Ensure that all top-level sub-blocks are `server` blocks
  for (size_t i = 0; i < root_.sub_blocks.size(); ++i) {
    const BlockNode& block = root_.sub_blocks[i];
    if (block.type != "server") {
      std::ostringstream oss;
      oss << configErrorPrefix() << "unexpected top-level block '" << block.type
          << "' at index " << i << " (expected 'server')";
      std::string msg = oss.str();
      LOG(ERROR) << msg;
      throw std::runtime_error(msg);
    }
  }

  // Parse and validate global directives
  global_max_request_body_ = 0;
  global_error_pages_.clear();

  LOG(DEBUG) << "Processing " << root_.directives.size()
             << " global directive(s)";
  for (size_t i = 0; i < root_.directives.size(); ++i) {
    const DirectiveNode& directive = root_.directives[i];

    if (directive.name == "error_page") {
      requireArgsAtLeast_(directive, 2);
      global_error_pages_ = parseErrorPages(directive.args);
      for (std::map<http::Status, std::string>::const_iterator it =
               global_error_pages_.begin();
           it != global_error_pages_.end(); ++it) {
        LOG(DEBUG) << "Global error_page: " << it->first << " -> "
                   << it->second;
      }

    } else if (directive.name == "max_request_body") {
      requireArgsEqual_(directive, 1);
      global_max_request_body_ = parsePositiveNumber_(directive.args[0]);
      LOG(DEBUG) << "Global max_request_body set to: "
                 << global_max_request_body_;
    } else {
      throwUnrecognizedDirective_(directive, "as global directive");
    }
  }

  LOG(DEBUG) << "Building server objects from configuration...";
  servers_.clear();

  for (size_t i = 0; i < root_.sub_blocks.size(); ++i) {
    const BlockNode& block = root_.sub_blocks[i];
    if (block.type == "server") {
      LOG(DEBUG) << "Translating server block #" << i;
      Server srv;
      translateServerBlock_(block, srv, i);
      servers_.push_back(srv);
      LOG(DEBUG) << "Server #" << i << " created - Port: " << srv.port
                 << ", Locations: " << srv.locations.size();
    }
  }
  LOG(DEBUG) << "Built " << servers_.size() << " server(s)";

  return servers_;
}

// ==================== ERROR HELPER ====================

// Return the appropriate configuration error prefix depending on context.
std::string Config::configErrorPrefix() const {
  std::ostringstream oss;
  if (current_server_index_ != kGlobalContext) {
    oss << "Configuration error in server #" << current_server_index_;
    if (!current_location_path_.empty()) {
      oss << " location '" << current_location_path_ << "'";
    }
  } else {
    oss << "Configuration error";
  }
  oss << ": ";
  return oss.str();
}

// Centralised helper to throw standardized unrecognized-directive errors.
// `context` will be appended after the directive message (for example:
// "in server block", "in location block", or "global directive").
void Config::throwUnrecognizedDirective_(const DirectiveNode& directive,
                                         const std::string& context) const {
  std::ostringstream oss;
  oss << configErrorPrefix() << "Unrecognized directive '" << directive.name
      << "'";
  if (!context.empty()) {
    oss << " " << context;
  }
  std::string msg = oss.str();
  LOG(ERROR) << msg;
  throw std::runtime_error(msg);
}

// ==================== DEBUG OUTPUT ====================

static void printBlockRec(const BlockNode& block, int indent) {
  std::string pad(indent, ' ');
  {
    std::ostringstream stream;
    stream << pad << "Block: type='" << block.type << "'";
    if (!block.param.empty()) {
      stream << " param='" << block.param << "'";
    }
    LOG(DEBUG) << stream.str();
  }
  for (size_t i = 0; i < block.directives.size(); ++i) {
    const DirectiveNode& directive = block.directives[i];
    std::ostringstream stream;
    stream << pad << "  Directive: name='" << directive.name << "' args=[";
    for (size_t j = 0; j < directive.args.size(); ++j) {
      if (j != 0U) {
        stream << ", ";
      }
      stream << "'" << directive.args[j] << "'";
    }
    stream << "]";
    LOG(DEBUG) << stream.str();
  }
  for (size_t i = 0; i < block.sub_blocks.size(); ++i) {
    printBlockRec(block.sub_blocks[i], indent + 2);
  }
}

void Config::debug(void) const {
  printBlockRec(root_, 0);
}

// ==================== PARSING HELPERS ====================

void Config::removeComments(std::string& str) {
  size_t pos = 0;
  while ((pos = str.find('#', pos)) != std::string::npos) {
    size_t end = str.find('\n', pos);
    if (end == std::string::npos) {
      str.erase(pos);
      break;
    }
    str.erase(pos, end - pos);
  }
}

void Config::tokenize(const std::string& content) {
  tokens_.clear();
  std::string cur;
  for (size_t i = 0; i < content.size(); ++i) {
    char chr = content[i];
    if (chr == '{' || chr == '}' || chr == ';') {
      if (!cur.empty()) {
        tokens_.push_back(cur);
        cur.clear();
      }
      tokens_.push_back(std::string(1, chr));
    } else if (std::isspace(static_cast<unsigned char>(chr)) != 0) {
      if (!cur.empty()) {
        tokens_.push_back(cur);
        cur.clear();
      }
    } else {
      cur.push_back(chr);
    }
  }
  if (!cur.empty()) {
    tokens_.push_back(cur);
  }
  idx_ = 0;
}

bool Config::eof() const {
  return idx_ >= tokens_.size();
}

const std::string& Config::peek() const {
  static std::string empty;
  return idx_ < tokens_.size() ? tokens_[idx_] : empty;
}

std::string Config::get() {
  if (idx_ >= tokens_.size()) {
    throw std::runtime_error("Unexpected end of tokens");
  }
  return tokens_[idx_++];
}

bool Config::isBlock() const {
  // A block is identified by a '{' following the current token.
  // This can be:
  //   - Immediately after (e.g., "server {")
  //   - After one parameter (e.g., "location /path {")
  // We look ahead at most 2 positions to accommodate blocks with parameters.
  return (idx_ + 1 < tokens_.size() && tokens_[idx_ + 1] == "{") ||
         (idx_ + 2 < tokens_.size() && tokens_[idx_ + 2] == "{");
}

DirectiveNode Config::parseDirective() {
  DirectiveNode directive;
  directive.name = get();
  while (peek() != ";") {
    if (eof()) {
      throw std::runtime_error(std::string("Directive '") + directive.name +
                               "' missing ';'");
    }
    directive.args.push_back(get());
  }
  get();  // consume ;
  return directive;
}

BlockNode Config::parseBlock() {
  BlockNode block;
  block.type = get();  // server or location
  if (block.type == "location") {
    if (peek().empty()) {
      throw std::runtime_error("location missing parameter");
    }
    block.param = get();
  }
  if (get() != "{") {
    throw std::runtime_error("Expected '{' after block type");
  }
  while (peek() != "}") {
    if (eof()) {
      throw std::runtime_error(std::string("Missing '}' for block ") +
                               block.type);
    }
    if (isBlock()) {
      block.sub_blocks.push_back(parseBlock());
    } else {
      block.directives.push_back(parseDirective());
    }
  }
  get();  // consume }
  return block;
}

// ==================== VALIDATION METHODS ====================

int Config::parsePortValue_(const std::string& portstr) {
  static const std::size_t kMaxPort = 65535;
  std::size_t num = parsePositiveNumber_(portstr);
  if (num < 1 || num > kMaxPort) {
    std::ostringstream oss;
    oss << configErrorPrefix() << "Invalid port number " << num
        << " (must be 1-65535)";
    LOG(ERROR) << oss.str();
    throw std::runtime_error(oss.str());
  }
  return static_cast<int>(num);
}

bool Config::parseBooleanValue_(const std::string& value) {
  if (value == "on") {
    return true;
  }
  if (value == "off") {
    return false;
  }
  std::ostringstream oss;
  oss << configErrorPrefix() << "Invalid boolean value '" << value
      << "' (expected: on/off)";
  throw std::runtime_error(oss.str());
}

http::Method Config::parseHttpMethod_(const std::string& method) {
  try {
    return http::stringToMethod(method);
  } catch (const std::invalid_argument& e) {
    std::ostringstream oss;
    oss << configErrorPrefix() << e.what();
    throw std::runtime_error(oss.str());
  }
}

http::Status Config::parseRedirectCode_(const std::string& value) {
  std::size_t code_sz = parsePositiveNumber_(value);
  int code = static_cast<int>(code_sz);
  if (code_sz > static_cast<std::size_t>(INT_MAX)) {
    std::ostringstream oss;
    oss << configErrorPrefix() << "Invalid redirect status code " << code
        << " (valid: 301, 302, 303, 307, 308)";
    throw std::runtime_error(oss.str());
  }

  try {
    http::Status status = http::intToStatus(code);
    if (!http::isRedirect(status)) {
      std::ostringstream oss;
      oss << configErrorPrefix() << "Invalid redirect status code " << code
          << " (valid: 301, 302, 303, 307, 308)";
      throw std::runtime_error(oss.str());
    }
    return status;
  } catch (const std::invalid_argument&) {
    std::ostringstream oss;
    oss << configErrorPrefix() << "Invalid redirect status code " << code
        << " (valid: 301, 302, 303, 307, 308)";
    throw std::runtime_error(oss.str());
  }
}

std::size_t Config::parsePositiveNumber_(const std::string& value) {
  static const int kDecimalBase = 10;
  for (size_t i = 0; i < value.size(); ++i) {
    if (value[i] < '0' || value[i] > '9') {
      std::ostringstream oss;
      oss << configErrorPrefix() << "Invalid positive number '" << value << "'";
      throw std::runtime_error(oss.str());
    }
  }

  errno = 0;
  char* endptr = NULL;
  long num = std::strtol(value.c_str(), &endptr, kDecimalBase);

  if (errno == ERANGE) {
    std::ostringstream oss;
    oss << configErrorPrefix() << "Numeric value out of range: '" << value
        << "'";
    throw std::runtime_error(oss.str());
  }
  if (endptr == NULL || *endptr != '\0') {
    std::ostringstream oss;
    oss << configErrorPrefix() << "Invalid numeric value '" << value << "'";
    throw std::runtime_error(oss.str());
  }
  if (num <= 0) {
    std::ostringstream oss;
    oss << configErrorPrefix() << "Invalid positive number '" << value << "'";
    throw std::runtime_error(oss.str());
  }

  return static_cast<std::size_t>(num);
}

void Config::requireArgsAtLeast_(const DirectiveNode& directive,
                                 size_t num) const {
  if (directive.args.size() < num) {
    std::ostringstream oss;
    oss << configErrorPrefix() << "Directive '" << directive.name
        << "' requires at least " << num << " argument(s)";
    std::string msg = oss.str();
    LOG(ERROR) << msg;
    throw std::runtime_error(msg);
  }
}

void Config::requireArgsEqual_(const DirectiveNode& directive,
                               size_t num) const {
  if (directive.args.size() != num) {
    std::ostringstream oss;
    oss << configErrorPrefix() << "Directive '" << directive.name
        << "' requires exactly " << num << " argument(s)";
    std::string msg = oss.str();
    LOG(ERROR) << msg;
    throw std::runtime_error(msg);
  }
}

// Validate a list of HTTP methods and populate the destination set with
// http::Method entries.
std::set<http::Method> Config::parseMethods(
    const std::vector<std::string>& args) {
  std::set<http::Method> dest;
  for (size_t i = 0; i < args.size(); ++i) {
    const std::string& method_str = args[i];
    http::Method method = parseHttpMethod_(method_str);
    dest.insert(method);
  }
  return dest;
}

// Validate and populate error_page mappings. The args vector is expected
// to have one or more status codes followed by a final path. For example:
// ["500","502","/50x.html"]
std::map<http::Status, std::string> Config::parseErrorPages(
    const std::vector<std::string>& args) {
  if (args.size() < 2) {
    std::ostringstream oss;
    oss << configErrorPrefix() << "Directive requires at least two args";
    throw std::runtime_error(oss.str());
  }
  const std::string& path = args[args.size() - 1];
  std::map<http::Status, std::string> dest;
  for (size_t i = 0; i + 1 < args.size(); ++i) {
    http::Status code = parseStatusCode_(args[i]);
    // centralised validation (now accepts enum)
    validateErrorPageCode_(code);
    dest[code] = path;
  }
  return dest;
}

void Config::validateErrorPageCode_(http::Status code) const {
  if (!(http::isClientError(code) || http::isServerError(code))) {
    throwInvalidErrorPageCode_(code);
  }
}

void Config::throwInvalidErrorPageCode_(http::Status code) const {
  std::ostringstream oss;
  oss << configErrorPrefix() << "Invalid error_page status code " << code
      << " (must be 4xx or 5xx)";
  std::string msg = oss.str();
  LOG(ERROR) << msg;
  throw std::runtime_error(msg);
}

// Parse a redirect (return) directive. Expects at least two args:
// [code, location]. Returns pair<code, location>.
std::pair<http::Status, std::string> Config::parseRedirect(
    const std::vector<std::string>& args) {
  if (args.size() < 2) {
    std::ostringstream oss;
    oss << configErrorPrefix() << "Directive requires at least two args";
    throw std::runtime_error(oss.str());
  }
  http::Status code = parseRedirectCode_(args[0]);
  const std::string& location = args[1];
  return std::make_pair(code, location);
}

http::Status Config::parseStatusCode_(const std::string& value) {
  std::size_t code_sz = parsePositiveNumber_(value);
  int code = static_cast<int>(code_sz);

  if (code_sz > static_cast<std::size_t>(INT_MAX) ||
      !http::isValidStatusCode(code)) {
    std::ostringstream oss;
    oss << configErrorPrefix() << "Invalid status code " << code_sz;
    std::string msg = oss.str();
    LOG(ERROR) << msg;
    throw std::runtime_error(msg);
  }

  return http::intToStatus(code);
}

// ==================== TRANSLATION/BUILDING METHODS ====================

void Config::translateServerBlock_(const BlockNode& server_block, Server& srv,
                                   size_t server_index) {
  LOG(DEBUG) << "Translating server block #" << server_index << "...";

  // set current server context for helpers
  current_server_index_ = server_index;
  current_location_path_.clear();

  // Process server directives (handle listen + others in one pass)
  LOG(DEBUG) << "Processing " << server_block.directives.size()
             << " server directive(s)";
  for (size_t i = 0; i < server_block.directives.size(); ++i) {
    const DirectiveNode& directive = server_block.directives[i];

    if (directive.name == "listen") {
      requireArgsEqual_(directive, 1);
      Config::ListenInfo listen_info = parseListen(directive.args[0]);
      srv.port = listen_info.port;
      srv.host = listen_info.host;
      in_addr addr = {};
      addr.s_addr = srv.host;
      LOG(DEBUG) << "Server listen: " << inet_ntoa(addr) << ":" << srv.port;

    } else if (directive.name == "root") {
      requireArgsEqual_(directive, 1);
      srv.root = directive.args[0];
      LOG(DEBUG) << "Server root: " << srv.root;

    } else if (directive.name == "index") {
      requireArgsEqual_(directive, 1);
      std::set<std::string> idx;
      for (size_t j = 0; j < directive.args.size(); ++j) {
        idx.insert(trim_copy(directive.args[j]));
      }
      srv.index = idx;
      LOG(DEBUG) << "Server index files: " << directive.args.size()
                 << " file(s)";

    } else if (directive.name == "autoindex") {
      requireArgsAtLeast_(directive, 1);
      srv.autoindex = parseBooleanValue_(directive.args[0]);
      LOG(DEBUG) << "Server autoindex: " << (srv.autoindex ? "on" : "off");

    } else if (directive.name == "allow_methods") {
      requireArgsAtLeast_(directive, 1);
      srv.allow_methods = parseMethods(directive.args);
      LOG(DEBUG) << "Server allowed methods: " << directive.args.size()
                 << " method(s)";

    } else if (directive.name == "error_page") {
      requireArgsAtLeast_(directive, 2);
      std::map<http::Status, std::string> parsed =
          parseErrorPages(directive.args);
      for (std::map<http::Status, std::string>::const_iterator it =
               parsed.begin();
           it != parsed.end(); ++it) {
        srv.error_page[it->first] = it->second;
        LOG(DEBUG) << "Server error_page: " << it->first << " -> "
                   << it->second;
      }

    } else if (directive.name == "max_request_body") {
      requireArgsEqual_(directive, 1);
      srv.max_request_body = parsePositiveNumber_(directive.args[0]);
      LOG(DEBUG) << "Server max_request_body: " << srv.max_request_body;
    } else {
      throwUnrecognizedDirective_(directive, "in server block");
    }
  }

  // NOTE: restoration of context will happen at function end so subsequent
  // servers are processed with the global context.

  // Apply global error pages if not overridden
  if (srv.error_page.empty()) {
    srv.error_page = global_error_pages_;
    LOG(DEBUG) << "Applied global error pages to server";
  }

  // Minimum requirements: ensure listen was specified and root is set
  if (srv.port <= 0) {
    std::ostringstream oss;
    oss << configErrorPrefix() << "server #" << server_index
        << " missing 'listen' directive or invalid port";
    std::string msg = oss.str();
    LOG(ERROR) << msg;
    throw std::runtime_error(msg);
  }
  if (srv.root.empty()) {
    std::ostringstream oss;
    oss << configErrorPrefix() << "server #" << server_index
        << " missing 'root' directive";
    std::string msg = oss.str();
    LOG(ERROR) << msg;
    throw std::runtime_error(msg);
  }

  if (srv.max_request_body == 0 && global_max_request_body_ > 0) {
    srv.max_request_body = global_max_request_body_;
    LOG(DEBUG) << "Applied global max_request_body to server: "
               << srv.max_request_body;
  }

  LOG(DEBUG) << "Processing " << server_block.sub_blocks.size()
             << " location block(s)";
  for (size_t i = 0; i < server_block.sub_blocks.size(); ++i) {
    const BlockNode& block = server_block.sub_blocks[i];
    if (block.type == "location") {
      LOG(DEBUG) << "Translating location: " << block.param;
      Location loc(block.param);
      translateLocationBlock_(block, loc);
      srv.locations[loc.path] = loc;
    }
  }
  LOG(DEBUG) << "Server block translation completed";
  // restore to global context
  current_server_index_ = kGlobalContext;
  current_location_path_.clear();
}

void Config::translateLocationBlock_(const BlockNode& location_block,
                                     Location& loc) {
  // Set path from constructor parameter (block.param)
  loc.path = location_block.param;
  LOG(DEBUG) << "Translating location block: " << loc.path;
  // set current location context (server_index already set by caller)
  current_location_path_ = loc.path;

  // Parse directives
  LOG(DEBUG) << "Processing " << location_block.directives.size()
             << " location directive(s)";
  for (size_t i = 0; i < location_block.directives.size(); ++i) {
    const DirectiveNode& directive = location_block.directives[i];

    if (directive.name == "root") {
      requireArgsEqual_(directive, 1);
      loc.root = directive.args[0];
      LOG(DEBUG) << "  Location root: " << loc.root;
    } else if (directive.name == "index") {
      requireArgsAtLeast_(directive, 1);
      std::set<std::string> idx;
      for (size_t j = 0; j < directive.args.size(); ++j) {
        idx.insert(trim_copy(directive.args[j]));
      }
      loc.index = idx;
      LOG(DEBUG) << "  Location index files: " << directive.args.size()
                 << " file(s)";
    } else if (directive.name == "autoindex") {
      requireArgsEqual_(directive, 1);
      loc.autoindex = parseBooleanValue_(directive.args[0]);
      LOG(DEBUG) << "  Location autoindex: " << (loc.autoindex ? "on" : "off");
    } else if (directive.name == "allow_methods") {
      requireArgsAtLeast_(directive, 1);
      loc.allow_methods = parseMethods(directive.args);
      LOG(DEBUG) << "  Location allowed methods: " << directive.args.size()
                 << " method(s)";
    } else if (directive.name == "redirect") {
      requireArgsEqual_(directive, 2);
      std::pair<http::Status, std::string> ret = parseRedirect(directive.args);
      loc.redirect_code = ret.first;
      loc.redirect_location = ret.second;
      LOG(DEBUG) << "  Location redirect: " << loc.redirect_code << " -> "
                 << loc.redirect_location;
    } else if (directive.name == "error_page") {
      requireArgsAtLeast_(directive, 2);
      std::map<http::Status, std::string> parsed =
          parseErrorPages(directive.args);
      for (std::map<http::Status, std::string>::const_iterator it =
               parsed.begin();
           it != parsed.end(); ++it) {
        loc.error_page[it->first] = it->second;
        LOG(DEBUG) << "  Location error_page: " << it->first << " -> "
                   << it->second;
      }
    } else if (directive.name == "cgi") {
      requireArgsEqual_(directive, 1);
      loc.cgi = parseBooleanValue_(directive.args[0]);
      LOG(DEBUG) << "  Location CGI: " << (loc.cgi ? "on" : "off");
    } else {
      throwUnrecognizedDirective_(directive, "in location block");
    }
  }
  // clear location context (server context remains active in caller)
  current_location_path_.clear();
  LOG(DEBUG) << "Location block translation completed: " << loc.path;
}

// ==================== DIRECTIVE PARSERS ====================

Config::ListenInfo Config::parseListen(const std::string& listen_arg) {
  Config::ListenInfo listen_info = {};
  size_t colon_pos = listen_arg.find(':');

  // Extract port string
  std::string portstr;
  if (colon_pos != std::string::npos) {
    portstr = listen_arg.substr(colon_pos + 1);
  } else {
    portstr = listen_arg;
  }
  listen_info.port = parsePortValue_(portstr);

  // no host
  if (colon_pos == std::string::npos) {
    listen_info.host = INADDR_ANY;
    return listen_info;
  }

  listen_info.host = inet_addr(listen_arg.substr(0, colon_pos).c_str());

  // invalid host
  if (listen_info.host == INADDR_NONE) {
    std::ostringstream oss;
    oss << configErrorPrefix()
        << "Invalid IP address in listen directive: " << listen_arg;
    LOG(ERROR) << oss.str();
    throw std::runtime_error(oss.str());
  }

  return listen_info;
}
