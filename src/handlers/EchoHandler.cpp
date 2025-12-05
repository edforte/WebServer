#include "EchoHandler.hpp"

#include <sstream>

#include "Connection.hpp"
#include "Request.hpp"
#include "constants.hpp"

EchoHandler::EchoHandler() {}
EchoHandler::~EchoHandler() {}

HandlerResult EchoHandler::start(Connection& conn) {
  // Prepare a simple 200 OK response that echoes the request body
  conn.response.status_line.version = conn.getHttpVersion();
  conn.response.status_line.status_code = http::S_200_OK;
  conn.response.status_line.reason = "OK";

  // Copy request body into response body
  conn.response.setBody(conn.request.getBody());

  conn.response.addHeader("Content-Type", "text/plain; charset=utf-8");
  std::ostringstream oss;
  oss << conn.response.getBody().size();
  conn.response.addHeader("Content-Length", oss.str());

  // Serialize entire response into write_buffer
  conn.write_buffer = conn.response.serialize();
  conn.write_offset = 0;

  return HR_DONE;
}

HandlerResult EchoHandler::resume(Connection& /*conn*/) {
  // Echo is synchronous and completes in start()
  return HR_DONE;
}
