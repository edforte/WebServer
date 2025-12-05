#pragma once

#include <sys/types.h>

#include <string>

#include "Response.hpp"
#include "constants.hpp"

struct FileInfo {
  int fd;
  off_t size;
  std::string content_type;
};

namespace file_utils {
bool openFile(const std::string& path, FileInfo& out);
void closeFile(FileInfo& fi);
std::string guessMime(const std::string& path);
// stream file contents (uses sendfile). Returns:
//  0 = finished sending up to max_offset, 1 = would block (EAGAIN), -1 = error
int streamToSocket(int sock_fd, int file_fd, off_t& offset, off_t max_offset);

// parse a single-byte range header (only supports one range):
// input like "bytes=start-end" or "bytes=start-" or "bytes=-suffix"
// on success fills start/end (inclusive) and returns true.
bool parseRange(const std::string& rangeHeader, off_t file_size,
                off_t& out_start, off_t& out_end);

// Prepare a Response and FileInfo for serving a file (handles Range header).
// Parameters:
// - path: filesystem path
// - rangeHeader: pointer to Range header string or NULL
// - outResponse: Response to fill (status line + headers on success; no
// response prepared on error)
// - outFile: FileInfo to fill (fd and size)
// - out_start/out_end: byte range to serve (inclusive)
// Return: 0 = success (response prepared), -1 = file not found, -2 = invalid
// range
int prepareFileResponse(const std::string& path, const std::string* rangeHeader,
                        ::Response& outResponse, FileInfo& outFile,
                        off_t& out_start, off_t& out_end,
                        const std::string& httpVersion = HTTP_VERSION);
}  // namespace file_utils
