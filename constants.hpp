#pragma once

#define HTTP_VERSION "HTTP/1.1"
#define CRLF "\r\n"
#define DEFAULT_CONFIG_PATH "conf/default.conf"

// Using enum for integral constants as per C++ Core Guidelines
enum Constants {
  MAX_CONNECTIONS_PER_SERVER = 10,
  MAX_EVENTS = 64,
  WRITE_BUF_SIZE = 4096
};
