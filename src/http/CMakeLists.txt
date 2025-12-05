set(HTTP_SOURCES
  Body.cpp
  Header.cpp
  http_utils.cpp
  HttpMethod.cpp
  HttpStatus.cpp
  Message.cpp
  Request.cpp
  RequestLine.cpp
  Response.cpp
  StatusLine.cpp
  Uri.cpp
)

foreach(f IN LISTS HTTP_SOURCES)
  set(_rel "src/http/${f}")
  set_property(GLOBAL APPEND PROPERTY ALL_SOURCES "${_rel}")
endforeach()

add_library(webserv_http STATIC ${HTTP_SOURCES})
target_include_directories(webserv_http PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_compile_options(webserv_http PRIVATE -Wall -Wextra -Werror)
target_compile_features(webserv_http PUBLIC cxx_std_98)
