#pragma once

#include <sys/types.h>

#include <cstddef>
#include <string>

#include "HttpStatus.hpp"
#include "IHandler.hpp"
#include "Request.hpp"
#include "Response.hpp"

class Connection {
 public:
  Connection();
  Connection(int file_descriptor);
  Connection(const Connection& other);
  ~Connection();

  Connection& operator=(const Connection& other);

  int fd;
  int server_fd;
  std::string read_buffer;
  std::string write_buffer;
  std::size_t write_offset;
  std::size_t headers_end_pos;
  bool write_ready;
  Request request;
  Response response;
  IHandler* active_handler;

  int handleRead();
  int handleWrite();
  void processRequest(const class Server& server);
  void processResponse(const class Location& location);
  void prepareErrorResponse(http::Status status);
  // Validate request version and method for a given location.
  // Returns http::S_0_UNKNOWN on success, or an http::Status code to send.
  http::Status validateRequestForLocation(const class Location& location);
  // Resolve the request URI to a filesystem path according to `location`.
  // On success returns true and fills `out_path` and `out_is_directory`.
  // On failure it prepares an error response and returns false.
  bool resolvePathForLocation(const class Location& location,
                              std::string& out_path, bool& out_is_directory);
  void setHandler(IHandler* h);
  void clearHandler();
};
