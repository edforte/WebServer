#include "RedirectHandler.hpp"

#include "Connection.hpp"
#include "HttpStatus.hpp"
#include "Response.hpp"
#include "constants.hpp"

RedirectHandler::RedirectHandler(const Location& location)
    : location_(location) {}

RedirectHandler::~RedirectHandler() {}

HandlerResult RedirectHandler::start(Connection& conn) {
  // Prepare redirect response and serialize into connection write buffer
  conn.response.status_line.version = conn.getHttpVersion();
  conn.response.status_line.status_code = location_.redirect_code;
  conn.response.status_line.reason =
      http::reasonPhrase(location_.redirect_code);

  conn.response.addHeader("Location", location_.redirect_location);

  conn.response.getBody().data = "";
  conn.response.addHeader("Content-Length", "0");

  conn.write_buffer = conn.response.serialize();
  conn.write_offset = 0;
  return HR_DONE;
}

HandlerResult RedirectHandler::resume(Connection& conn) {
  (void)conn;
  return HR_DONE;  // no resume functionality for redirects
}
