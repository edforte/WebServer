#include "Connection.hpp"

#include <sys/socket.h>
#include <sys/stat.h>
#include <unistd.h>

#include <cerrno>
#include <cstdio>
#include <iostream>
#include <sstream>

#include "AutoindexHandler.hpp"
#include "Body.hpp"
#include "CgiHandler.hpp"
#include "FileHandler.hpp"
#include "HttpMethod.hpp"
#include "HttpStatus.hpp"
#include "Location.hpp"
#include "Logger.hpp"
#include "RedirectHandler.hpp"
#include "Server.hpp"
#include "constants.hpp"

Connection::Connection()
    : fd(-1),
      server_fd(-1),
      write_offset(0),
      headers_end_pos(std::string::npos),
      write_ready(false),
      request(),
      response(),
      active_handler(NULL) {}

Connection::Connection(int fd)
    : fd(fd),
      server_fd(-1),
      write_offset(0),
      headers_end_pos(std::string::npos),
      write_ready(false),
      request(),
      response(),
      active_handler(NULL) {}

Connection::Connection(const Connection& other)
    : fd(other.fd),
      server_fd(other.server_fd),
      read_buffer(other.read_buffer),
      write_buffer(other.write_buffer),
      write_offset(other.write_offset),
      headers_end_pos(other.headers_end_pos),
      write_ready(other.write_ready),
      request(other.request),
      response(other.response),
      active_handler(NULL) {}

Connection::~Connection() {
  clearHandler();
}

Connection& Connection::operator=(const Connection& other) {
  if (this != &other) {
    fd = other.fd;
    server_fd = other.server_fd;
    read_buffer = other.read_buffer;
    write_buffer = other.write_buffer;
    write_offset = other.write_offset;
    headers_end_pos = other.headers_end_pos;
    write_ready = other.write_ready;
    request = other.request;
    response = other.response;
    clearHandler();
  }
  return *this;
}

int Connection::handleRead() {
  while (1) {
    char buf[WRITE_BUF_SIZE] = {0};

    ssize_t r = recv(fd, buf, sizeof(buf), 0);

    if (r < 0) {
      if (errno == EAGAIN || errno == EWOULDBLOCK) {
        return 1;
      }
      LOG_PERROR(ERROR, "read");
      return -1;
    }

    if (r == 0) {
      LOG(INFO) << "Client disconnected (fd: " << fd << ")";
      return -1;
    }

    // Add new data to persistent buffer
    read_buffer.append(buf, r);

    // Check if the HTTP request headers are complete
    std::size_t pos = read_buffer.find(CRLF CRLF);
    if (pos != std::string::npos) {
      headers_end_pos = pos;
      break;
    }
  }
  return 0;
}

int Connection::handleWrite() {
  while (write_offset < write_buffer.size()) {
    ssize_t w =
        send(fd, write_buffer.c_str() + write_offset,
             static_cast<size_t>(write_buffer.size()) - write_offset, 0);

    LOG(DEBUG) << "Sent " << w << " bytes to fd=" << fd;

    if (w < 0) {
      if (errno == EAGAIN || errno == EWOULDBLOCK) {
        return 1;
      }
      // Error occurred
      LOG_PERROR(ERROR, "write");
      return -1;
    }

    write_offset += static_cast<size_t>(w);
  }

  // If there's an active handler, ask it to resume (streaming, CGI, etc.)
  if (active_handler) {
    HandlerResult hr = active_handler->resume(*this);
    if (hr == HR_WOULD_BLOCK) {
      return 1;
    } else if (hr == HR_ERROR) {
      clearHandler();
      return -1;
    } else {
      // HR_DONE
      clearHandler();
    }
  }

  // All data sent successfully
  return 0;
}

std::string Connection::getHttpVersion() const {
  if (request.request_line.version == "HTTP/1.0" ||
      request.request_line.version == "HTTP/1.1") {
    return request.request_line.version;
  }
  return HTTP_VERSION;
}

void Connection::prepareErrorResponse(http::Status status) {
  response.status_line.version = getHttpVersion();
  response.status_line.status_code = status;
  response.status_line.reason = http::reasonPhrase(status);

  std::string title = http::statusWithReason(status);
  std::ostringstream body;
  body << "<html>" << CRLF << "<head><title>" << title << "</title></head>"
       << CRLF << "<body>" << CRLF << "<center><h1>" << title
       << "</h1></center>" << CRLF << "</body>" << CRLF << "</html>" << CRLF;

  response.getBody().data = body.str();
  response.addHeader("Content-Type", "text/html; charset=utf-8");
  std::ostringstream oss;
  oss << response.getBody().size();
  response.addHeader("Content-Length", oss.str());
  write_buffer = response.serialize();
}

void Connection::setHandler(IHandler* h) {
  clearHandler();
  active_handler = h;
}

void Connection::clearHandler() {
  if (active_handler) {
    delete active_handler;
    active_handler = NULL;
  }
}

HandlerResult Connection::executeHandler(IHandler* handler) {
  // setHandler takes ownership of handler and clears any previous handler.
  setHandler(handler);
  HandlerResult hr = active_handler->start(*this);
  if (hr == HR_WOULD_BLOCK) {
    // Handler will continue later; keep it installed.
    return HR_WOULD_BLOCK;
  } else if (hr == HR_ERROR) {
    // Handler failed; clear and prepare a 500 error response.
    clearHandler();
    prepareErrorResponse(http::S_500_INTERNAL_SERVER_ERROR);
    return HR_ERROR;
  } else {
    // HR_DONE: handler completed synchronously and response is prepared.
    clearHandler();
    return HR_DONE;
  }
}

void Connection::processRequest(const Server& server) {
  LOG(DEBUG) << "Processing request for fd: " << fd;

  // 1. Parse request headers (already done in ServerManager)
  // 2. Get pathname from URI
  std::string path = request.request_line.uri;

  // Extract path from URI (remove query string if present)
  std::size_t query_pos = path.find('?');
  if (query_pos != std::string::npos) {
    path = path.substr(0, query_pos);
  }

  LOG(DEBUG) << "Request path: " << path;

  // 3. Match URI with Server.Location
  Location location = server.matchLocation(path);

  // 4. Process response based on location
  processResponse(location);
}

void Connection::processResponse(const Location& location) {
  LOG(DEBUG) << "Processing response for fd: " << fd;

  // Reset response state at the beginning to ensure all handlers start clean
  response = Response();

  // Validate protocol version and allowed method for this location.
  http::Status vstat = validateRequestForLocation(location);
  if (vstat != http::S_0_UNKNOWN) {
    prepareErrorResponse(vstat);
    return;
  }

  // Resource-based handler selection:
  // 1. Redirect handler (if configured) - TODO: implement in future PR
  // 2. CGI handler (if configured and matching extension) - TODO: implement in
  // future PR
  // 3. Directory handler (if path is directory) - TODO: implement in future PR
  // 4. File handler (default for static files)

  if (location.redirect_code != http::S_0_UNKNOWN) {
    // Delegate redirect response preparation to a RedirectHandler instance
    RedirectHandler* rh = new RedirectHandler(location);
    HandlerResult hr = executeHandler(rh);
    if (hr == HR_WOULD_BLOCK) {
      return;  // handler will continue later
    }
    // For HR_ERROR and HR_DONE, executeHandler already handled cleanup/error
    // and we should return to finish processing this request.
    return;
  }

  if (location.cgi) {
    // CGI handling
    std::string resolved_path;
    bool is_directory = false;
    if (!resolvePathForLocation(location, resolved_path, is_directory)) {
      return;  // resolvePathForLocation prepared an error response
    }

    if (is_directory) {
      prepareErrorResponse(http::S_403_FORBIDDEN);
      return;
    }

    IHandler* handler = new CgiHandler(resolved_path);
    setHandler(handler);

    HandlerResult hr = active_handler->start(*this);
    if (hr == HR_WOULD_BLOCK) {
      return;  // handler will continue later
    } else if (hr == HR_ERROR) {
      clearHandler();
      prepareErrorResponse(http::S_500_INTERNAL_SERVER_ERROR);
      return;
    } else {
      // HR_DONE: response prepared in write_buffer
      clearHandler();
      return;
    }
  }

  // Resolve the filesystem path for the request
  std::string resolved_path;
  bool is_directory = false;
  if (!resolvePathForLocation(location, resolved_path, is_directory)) {
    return;  // resolvePathForLocation prepared an error response
  }

  // Directory handling
  if (is_directory) {
    if (location.autoindex) {
      // Delegate to AutoindexHandler (produces directory listing)
      // Pass a user-facing URI path for display in the listing instead of the
      // filesystem path to avoid leaking internal structure.
      std::string display_path = request.request_line.uri;
      std::size_t qpos = display_path.find('?');
      if (qpos != std::string::npos) {
        display_path = display_path.substr(0, qpos);
      }
      if (display_path.empty()) {
        display_path = "/";
      }
      if (display_path[display_path.size() - 1] != '/') {
        display_path += '/';
      }

      AutoindexHandler* ah = new AutoindexHandler(resolved_path, display_path);
      HandlerResult hr = executeHandler(ah);
      if (hr == HR_WOULD_BLOCK) {
        return;  // handler will continue later
      }
      // HR_DONE or HR_ERROR: executeHandler already handled everything
      return;
    } else {
      // Directory listing not allowed
      prepareErrorResponse(http::S_403_FORBIDDEN);
      return;
    }
  }

  // Static file handling - FileHandler handles GET, HEAD, PUT, DELETE
  IHandler* handler = new FileHandler(resolved_path);
  HandlerResult hr = executeHandler(handler);
  if (hr == HR_WOULD_BLOCK) {
    return;  // handler will continue later
  }
}

http::Status Connection::validateRequestForLocation(const Location& location) {
  // 1. Check HTTP protocol version (accept both HTTP/1.0 and HTTP/1.1)
  if (request.request_line.version != "HTTP/1.0" &&
      request.request_line.version != "HTTP/1.1") {
    LOG(INFO) << "Unsupported HTTP version: " << request.request_line.version;
    return http::S_505_HTTP_VERSION_NOT_SUPPORTED;
  }

  // 2. Check HTTP method
  http::Method method;
  try {
    method = http::stringToMethod(request.request_line.method);
  } catch (const std::invalid_argument&) {
    LOG(INFO) << "Not implemented method: " << request.request_line.method;
    return http::S_501_NOT_IMPLEMENTED;
  }

  // Check if method is allowed in this location
  if (location.allow_methods.find(method) == location.allow_methods.end()) {
    LOG(INFO) << "Method not allowed: " << request.request_line.method
              << " for location: " << location.path;
    // Build Allow header with allowed methods
    std::string allow_header;
    for (std::set<http::Method>::const_iterator it =
             location.allow_methods.begin();
         it != location.allow_methods.end(); ++it) {
      if (!allow_header.empty()) {
        allow_header += ", ";
      }
      allow_header += http::methodToString(*it);
    }
    response.addHeader("Allow", allow_header);
    return http::S_405_METHOD_NOT_ALLOWED;
  }

  return http::S_0_UNKNOWN;  // OK
}

bool Connection::resolvePathForLocation(const Location& location,
                                        std::string& out_path,
                                        bool& out_is_directory) {
  out_is_directory = false;

  // Use the request URI (strip query string)
  std::string uri = request.request_line.uri;
  std::size_t q = uri.find('?');
  if (q != std::string::npos) {
    uri = uri.substr(0, q);
  }

  // Path traversal protection: check for ".." sequences
  // This handles both raw ".." and URL-encoded variants (%2e%2e, %2E%2E)
  // by checking the raw URI and rejecting suspicious patterns.
  // Note: A proper implementation would URL-decode first, then validate.
  if (uri.find("..") != std::string::npos ||
      uri.find("%2e%2e") != std::string::npos ||
      uri.find("%2E%2E") != std::string::npos ||
      uri.find("%2e%2E") != std::string::npos ||
      uri.find("%2E%2e") != std::string::npos) {
    LOG(INFO) << "Path traversal attempt blocked: " << uri;
    prepareErrorResponse(http::S_403_FORBIDDEN);
    return false;
  }

  // Relative path inside the location
  std::string rel = uri;
  if (!location.path.empty() && location.path != "/") {
    if (rel.find(location.path) == 0) {
      rel = rel.substr(location.path.size());
      if (rel.empty()) {
        rel = "/";
      }
    }
  }

  if (location.root.empty()) {
    prepareErrorResponse(http::S_500_INTERNAL_SERVER_ERROR);
    return false;
  }
  std::string root = location.root;

  std::string path;
  if (!root.empty() && root[root.size() - 1] == '/' && !rel.empty() &&
      rel[0] == '/') {
    path = root + rel.substr(1);
  } else if (!root.empty() && root[root.size() - 1] != '/' && !rel.empty() &&
             rel[0] != '/') {
    path = root + "/" + rel;
  } else {
    path = root + rel;
  }

  struct stat st;
  bool path_is_dir = false;
  if (stat(path.c_str(), &st) == 0 && S_ISDIR(st.st_mode)) {
    path_is_dir = true;
    if (!path.empty() && path[path.size() - 1] != '/') {
      path += '/';
    }
  }

  // Try to resolve directory to index file
  if (path_is_dir || (!path.empty() && path[path.size() - 1] == '/')) {
    bool found_index = false;
    for (std::set<std::string>::const_iterator it = location.index.begin();
         it != location.index.end(); ++it) {
      std::string cand = path + *it;
      if (stat(cand.c_str(), &st) == 0 && S_ISREG(st.st_mode)) {
        path = cand;
        found_index = true;
        break;
      }
    }
    if (!found_index) {
      // No index file found - this is a directory request
      out_path = path;
      out_is_directory = true;
      return true;  // Let caller decide what to do with directory
    }
  }

  out_path = path;
  return true;
}
