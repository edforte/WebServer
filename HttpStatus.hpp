#pragma once

#include <string>

namespace http {

enum Status {
  S_0_UNKNOWN = 0,
  S_200_OK = 200,
  S_201_CREATED = 201,
  S_204_NO_CONTENT = 204,
  S_206_PARTIAL_CONTENT = 206,
  S_301_MOVED_PERMANENTLY = 301,
  S_302_FOUND = 302,
  S_303_SEE_OTHER = 303,
  S_307_TEMPORARY_REDIRECT = 307,
  S_308_PERMANENT_REDIRECT = 308,
  S_400_BAD_REQUEST = 400,
  S_401_UNAUTHORIZED = 401,
  S_403_FORBIDDEN = 403,
  S_404_NOT_FOUND = 404,
  S_405_METHOD_NOT_ALLOWED = 405,
  S_413_PAYLOAD_TOO_LARGE = 413,
  S_414_URI_TOO_LONG = 414,
  S_416_RANGE_NOT_SATISFIABLE = 416,
  S_500_INTERNAL_SERVER_ERROR = 500,
  S_501_NOT_IMPLEMENTED = 501,
  S_502_BAD_GATEWAY = 502,
  S_503_SERVICE_UNAVAILABLE = 503,
  S_504_GATEWAY_TIMEOUT = 504,
  S_505_HTTP_VERSION_NOT_SUPPORTED = 505
};

// Convert int to Status enum; throws std::invalid_argument on unknown code
Status intToStatus(int status);

std::string reasonPhrase(Status status);

// Return a single string containing the numeric status and reason phrase,
// e.g. "404 Not Found". Accept only the enum to avoid casts.
std::string statusWithReason(Status status);

// Classification helpers
bool isSuccess(Status status);
bool isRedirect(Status status);
bool isClientError(Status status);
bool isServerError(Status status);

// Check if status code (int) is within valid HTTP status code range
bool isValidStatusCode(int status);

}  // namespace http
