#include "Server.hpp"

#include <arpa/inet.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>

#include <cerrno>
#include <cstring>
#include <iostream>

#include "Logger.hpp"
#include "constants.hpp"
#include "utils.hpp"

Server::Server(void)
    : fd(-1),
      port(-1),
      host(INADDR_ANY),
      autoindex(false),
      max_request_body(0) {
  LOG(DEBUG) << "Server() default constructor called";
  initDefaultHttpMethods(allow_methods);
  LOG(DEBUG) << "Server initialized with default allowed methods";
}

Server::Server(int port)
    : fd(-1),
      port(port),
      host(INADDR_ANY),
      autoindex(false),
      max_request_body(0) {
  LOG(DEBUG) << "Server(port) constructor called with port: " << port;
  initDefaultHttpMethods(allow_methods);
  LOG(DEBUG) << "Server on port " << port
             << " initialized with default allowed methods";
}

Server::Server(const Server& other)
    : fd(other.fd),
      port(other.port),
      host(other.host),
      allow_methods(other.allow_methods),
      index(other.index),
      autoindex(other.autoindex),
      root(other.root),
      error_page(other.error_page),
      max_request_body(other.max_request_body),
      locations(other.locations) {}

Server::~Server() {
  disconnect();
}

Server& Server::operator=(const Server& other) {
  if (this != &other) {
    fd = other.fd;
    port = other.port;
    host = other.host;
    allow_methods = other.allow_methods;
    index = other.index;
    autoindex = other.autoindex;
    root = other.root;
    error_page = other.error_page;
    max_request_body = other.max_request_body;
    locations = other.locations;
  }
  return *this;
}

void Server::init(void) {
  in_addr host_addr = {};
  host_addr.s_addr = host;
  LOG(INFO) << "Initializing server on " << inet_ntoa(host_addr) << ":" << port
            << "...";

  fd = socket(AF_INET, SOCK_STREAM, 0);
  if (fd < 0) {
    LOG_PERROR(ERROR, "socket");
    throw std::runtime_error("socket");
  }
  LOG(DEBUG) << "Socket created with fd: " << fd;

  /* avoid "address already in use" on quick restarts */
  int opt = 1;
  if (setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0) {
    disconnect();
    LOG_PERROR(ERROR, "setsockopt");
    throw std::runtime_error("setsockopt");
  }
  LOG(DEBUG) << "SO_REUSEADDR option set on socket";

  struct sockaddr_in addr = {};
  std::memset(&addr, 0, sizeof(addr));
  addr.sin_family = AF_INET;
  /* bind to configured host (INADDR_ANY if not set) */
  addr.sin_addr.s_addr = host;
  addr.sin_port = htons(port);

  // Required for POSIX socket API
  if (bind(fd, reinterpret_cast<struct sockaddr*>(&addr),  // NOLINT
           sizeof(addr)) < 0) {
    disconnect();
    LOG_PERROR(ERROR, "bind");
    throw std::runtime_error("bind");
  }
  LOG(DEBUG) << "Socket bound to " << inet_ntoa(host_addr) << ":" << port;

  if (listen(fd, MAX_CONNECTIONS_PER_SERVER) < 0) {
    disconnect();
    LOG_PERROR(ERROR, "listen");
    throw std::runtime_error("listen");
  }
  LOG(DEBUG) << "Socket listening with backlog: " << MAX_CONNECTIONS_PER_SERVER;

  if (set_nonblocking(fd) < 0) {
    disconnect();
    LOG_PERROR(ERROR, "set_nonblocking");
    throw std::runtime_error("set_nonblocking");
  }
  LOG(DEBUG) << "Socket set to non-blocking mode";

  LOG(INFO) << "Server successfully initialized on port " << port
            << " (fd: " << fd << ")";
}

void Server::disconnect(void) {
  if (fd != -1) {
    LOG(DEBUG) << "Closing server socket fd: " << fd;
    close(fd);
    fd = -1;
  }
}

Location Server::matchLocation(const std::string& path) const {
  LOG(DEBUG) << "Matching path '" << path << "' against " << locations.size()
             << " location(s)";

  // Find the longest matching prefix
  std::string best_match;
  std::map<std::string, Location>::const_iterator best_it = locations.end();

  for (std::map<std::string, Location>::const_iterator it = locations.begin();
       it != locations.end(); ++it) {
    const std::string& loc_path = it->first;

    // Check if this location path is a prefix of the request path, respecting
    // path segment boundaries
    if (path.find(loc_path) == 0) {
      // Ensure it's an exact match or followed by '/' (path boundary)
      if (path.length() == loc_path.length() ||
          (path.length() > loc_path.length() &&
           path[loc_path.length()] == '/')) {
        // This is a match. Keep the longest one.
        if (loc_path.length() > best_match.length()) {
          best_match = loc_path;
          best_it = it;
        }
      }
    }
  }

  if (best_it != locations.end()) {
    LOG(DEBUG) << "Matched location: '" << best_it->first << "'";
    return best_it->second;
  }

  // No location matched, return a default location with server defaults
  LOG(DEBUG) << "No location matched, using server defaults";
  Location default_loc;
  default_loc.path = "/";
  default_loc.allow_methods = allow_methods;
  default_loc.index = index;
  default_loc.autoindex = autoindex;
  default_loc.root = root;
  default_loc.error_page = error_page;
  return default_loc;
}
