#include "HttpStatus.hpp"

#include <sstream>
#include <stdexcept>

namespace http {

std::string reasonPhrase(Status status) {
  switch (status) {
    case S_200_OK:
      return "OK";
    case S_206_PARTIAL_CONTENT:
      return "Partial Content";
    case S_400_BAD_REQUEST:
      return "Bad Request";
    case S_404_NOT_FOUND:
      return "Not Found";
    case S_500_INTERNAL_SERVER_ERROR:
      return "Internal Server Error";
    case S_201_CREATED:
      return "Created";
    case S_204_NO_CONTENT:
      return "No Content";
    case S_301_MOVED_PERMANENTLY:
      return "Moved Permanently";
    case S_302_FOUND:
      return "Found";
    case S_303_SEE_OTHER:
      return "See Other";
    case S_307_TEMPORARY_REDIRECT:
      return "Temporary Redirect";
    case S_308_PERMANENT_REDIRECT:
      return "Permanent Redirect";
    case S_401_UNAUTHORIZED:
      return "Unauthorized";
    case S_403_FORBIDDEN:
      return "Forbidden";
    case S_405_METHOD_NOT_ALLOWED:
      return "Method Not Allowed";
    case S_413_PAYLOAD_TOO_LARGE:
      return "Payload Too Large";
    case S_414_URI_TOO_LONG:
      return "URI Too Long";
    case S_416_RANGE_NOT_SATISFIABLE:
      return "Range Not Satisfiable";
    case S_501_NOT_IMPLEMENTED:
      return "Not Implemented";
    case S_502_BAD_GATEWAY:
      return "Bad Gateway";
    case S_503_SERVICE_UNAVAILABLE:
      return "Service Unavailable";
    case S_504_GATEWAY_TIMEOUT:
      return "Gateway Timeout";
    case S_505_HTTP_VERSION_NOT_SUPPORTED:
      return "HTTP Version Not Supported";
    default:
      return "";
  }
}

Status intToStatus(int status) {
  switch (status) {
    case 200:
      return S_200_OK;
    case 206:
      return S_206_PARTIAL_CONTENT;
    case 201:
      return S_201_CREATED;
    case 204:
      return S_204_NO_CONTENT;
    case 301:
      return S_301_MOVED_PERMANENTLY;
    case 302:
      return S_302_FOUND;
    case 303:
      return S_303_SEE_OTHER;
    case 307:
      return S_307_TEMPORARY_REDIRECT;
    case 308:
      return S_308_PERMANENT_REDIRECT;
    case 400:
      return S_400_BAD_REQUEST;
    case 401:
      return S_401_UNAUTHORIZED;
    case 403:
      return S_403_FORBIDDEN;
    case 404:
      return S_404_NOT_FOUND;
    case 405:
      return S_405_METHOD_NOT_ALLOWED;
    case 413:
      return S_413_PAYLOAD_TOO_LARGE;
    case 414:
      return S_414_URI_TOO_LONG;
    case 416:
      return S_416_RANGE_NOT_SATISFIABLE;
    case 500:
      return S_500_INTERNAL_SERVER_ERROR;
    case 501:
      return S_501_NOT_IMPLEMENTED;
    case 502:
      return S_502_BAD_GATEWAY;
    case 503:
      return S_503_SERVICE_UNAVAILABLE;
    case 504:
      return S_504_GATEWAY_TIMEOUT;
    case 505:
      return S_505_HTTP_VERSION_NOT_SUPPORTED;
    default:
      throw std::invalid_argument("Unknown HTTP status code");
  }
}

std::string statusWithReason(Status status) {
  std::ostringstream oss;
  oss << status;
  std::string reason = reasonPhrase(status);
  if (!reason.empty()) {
    oss << " " << reason;
  }
  return oss.str();
}

bool isSuccess(Status status) {
  static const int kMinSuccess = 200;
  static const int kMaxSuccess = 299;
  return status >= kMinSuccess && status <= kMaxSuccess;
}

bool isRedirect(Status status) {
  static const int kMinRedirect = 300;
  static const int kMaxRedirect = 399;
  return status >= kMinRedirect && status <= kMaxRedirect;
}

bool isClientError(Status status) {
  static const int kMinClientError = 400;
  static const int kMaxClientError = 499;
  return status >= kMinClientError && status <= kMaxClientError;
}

bool isServerError(Status status) {
  static const int kMinServerError = 500;
  static const int kMaxServerError = 505;
  return status >= kMinServerError && status <= kMaxServerError;
}

bool isValidStatusCode(int status) {
  try {
    (void)intToStatus(status);
    return true;
  } catch (const std::invalid_argument&) {
    return false;
  }
}

}  // namespace http
