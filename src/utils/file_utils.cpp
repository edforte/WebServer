#include "file_utils.hpp"

#include <fcntl.h>
#include <sys/sendfile.h>
#include <sys/stat.h>
#include <unistd.h>

#include <cerrno>
#include <cstdlib>
#include <cstring>
#include <sstream>

#include "HttpStatus.hpp"
#include "Logger.hpp"
#include "Response.hpp"
#include "constants.hpp"
#include "utils.hpp"

namespace file_utils {

std::string guessMime(const std::string& path) {
  std::string def = "application/octet-stream";
  std::size_t pos = path.rfind('.');
  if (pos == std::string::npos) {
    return def;
  }
  std::string ext = path.substr(pos + 1);
  if (ext == "html" || ext == "htm") {
    return "text/html; charset=utf-8";
  }
  if (ext == "txt") {
    return "text/plain; charset=utf-8";
  }
  if (ext == "css") {
    return "text/css";
  }
  if (ext == "js") {
    return "application/javascript";
  }
  if (ext == "jpg" || ext == "jpeg") {
    return "image/jpeg";
  }
  if (ext == "png") {
    return "image/png";
  }
  if (ext == "gif") {
    return "image/gif";
  }
  return def;
}

bool openFile(const std::string& path, FileInfo& out) {
  out.fd = -1;
  out.size = 0;
  out.content_type.clear();

  int fd = open(path.c_str(), O_RDONLY);
  if (fd < 0) {
    LOG_PERROR(ERROR, "file_utils: openFile failed for '" << path << "'");
    return false;
  }

  struct stat st;
  if (fstat(fd, &st) < 0) {
    close(fd);
    LOG_PERROR(ERROR, "file_utils: fstat failed for '" << path << "'");
    return false;
  }

  out.fd = fd;
  out.size = st.st_size;
  out.content_type = guessMime(path);
  LOG(DEBUG) << "file_utils: opened '" << path << "' fd=" << out.fd
             << " size=" << out.size << " type=" << out.content_type;
  return true;
}

void closeFile(FileInfo& fi) {
  if (fi.fd >= 0) {
    LOG(DEBUG) << "file_utils: closing fd=" << fi.fd;
    close(fi.fd);
    fi.fd = -1;
  }
  fi.size = 0;
  fi.content_type.clear();
}

int streamToSocket(int sock_fd, int file_fd, off_t& offset, off_t max_offset) {
  if (offset >= max_offset) {
    return 0;  // nothing to send
  }

  LOG(DEBUG) << "file_utils: streamToSocket fd=" << file_fd
             << " to sock=" << sock_fd << " offset=" << offset
             << " max=" << max_offset;
  while (offset < max_offset) {
    size_t to_send = static_cast<size_t>(max_offset - offset);
    if (to_send > WRITE_BUF_SIZE) {
      to_send = WRITE_BUF_SIZE;
    }

    ssize_t s = sendfile(sock_fd, file_fd, &offset, to_send);
    if (s < 0) {
      if (errno == EAGAIN || errno == EWOULDBLOCK) {
        LOG(DEBUG) << "file_utils: sendfile would block (EAGAIN)";
        return 1;
      }
      LOG_PERROR(ERROR, "file_utils: sendfile error");
      return -1;
    }

    if (s == 0) {
      LOG(DEBUG) << "file_utils: sendfile returned 0 (EOF?)";
      break;  // no more data
    }

    LOG(DEBUG) << "file_utils: sendfile wrote " << s
               << " bytes, new offset=" << offset;
  }

  return (offset >= max_offset) ? 0 : 1;
}

bool parseRange(const std::string& rangeHeader, off_t file_size,
                off_t& out_start, off_t& out_end) {
  const std::string prefix = "bytes=";
  if (rangeHeader.compare(0, prefix.size(), prefix) != 0) {
    return false;
  }

  std::string spec = rangeHeader.substr(prefix.size());
  std::size_t dash = spec.find('-');
  if (dash == std::string::npos) {
    return false;
  }

  std::string first = spec.substr(0, dash);
  std::string second = spec.substr(dash + 1);

  if (first.empty()) {
    if (second.empty()) {
      return false;
    }
    long long suffix_val;
    if (!safeStrtoll(second, suffix_val) || suffix_val <= 0) {
      return false;
    }
    off_t suffix = static_cast<off_t>(suffix_val);
    if (suffix > file_size) {
      suffix = file_size;
    }
    out_start = file_size - suffix;
    out_end = file_size - 1;
    return true;
  }

  long long start_val;
  if (!safeStrtoll(first, start_val)) {
    return false;
  }
  off_t start = static_cast<off_t>(start_val);
  off_t end = -1;
  if (second.empty()) {
    end = file_size - 1;
  } else {
    long long end_val;
    if (!safeStrtoll(second, end_val)) {
      return false;
    }
    end = static_cast<off_t>(end_val);
  }

  if (start < 0 || (file_size > 0 && start >= file_size)) {
    return false;
  }
  if (end < start) {
    return false;
  }
  if (end >= file_size) {
    end = file_size - 1;
  }

  out_start = start;
  out_end = end;
  return true;
}

int prepareFileResponse(const std::string& path, const std::string* rangeHeader,
                        ::Response& outResponse, FileInfo& outFile,
                        off_t& out_start, off_t& out_end,
                        const std::string& httpVersion) {
  outFile.fd = -1;
  outFile.size = 0;
  outFile.content_type.clear();

  if (!openFile(path, outFile)) {
    LOG(DEBUG)
        << "file_utils: prepareFileResponse - openFile returned false for '"
        << path << "'";
    return -1;  // not found
  }

  off_t file_size = outFile.size;
  bool is_partial = false;

  if (rangeHeader) {
    off_t s = 0, e = 0;
    if (!parseRange(*rangeHeader, file_size, s, e)) {
      LOG(DEBUG) << "file_utils: prepareFileResponse - invalid range '"
                 << *rangeHeader << "' for file=" << path
                 << " size=" << file_size;
      // Signal to the caller that the Range header was invalid. Do not
      // prepare an error response here so callers (e.g. Connection) can
      // produce error pages using their standard helper. To allow the
      // caller to add a proper Content-Range header, return the file size
      // via out_end (out_start set to -1).
      out_start = -1;
      out_end = file_size;
      closeFile(outFile);
      return -2;
    }
    LOG(DEBUG) << "file_utils: prepareFileResponse - parsed range start=" << s
               << " end=" << e;
    out_start = s;
    out_end = e;
    is_partial = true;
  } else {
    out_start = 0;
    out_end = file_size - 1;
  }

  if (is_partial) {
    outResponse.status_line.version = httpVersion;
    outResponse.status_line.status_code = http::S_206_PARTIAL_CONTENT;
    outResponse.status_line.reason =
        http::reasonPhrase(http::S_206_PARTIAL_CONTENT);
    off_t len = out_end - out_start + 1;
    {
      std::ostringstream tmp;
      tmp << len;
      outResponse.addHeader("Content-Length", tmp.str());
    }
    std::ostringstream cr;
    cr << "bytes " << out_start << "-" << out_end << "/" << file_size;
    outResponse.addHeader("Content-Range", cr.str());
  } else {
    outResponse.status_line.version = httpVersion;
    outResponse.status_line.status_code = http::S_200_OK;
    outResponse.status_line.reason = http::reasonPhrase(http::S_200_OK);
    {
      std::ostringstream tmp;
      tmp << file_size;
      outResponse.addHeader("Content-Length", tmp.str());
    }
  }

  outResponse.addHeader("Content-Type", outFile.content_type);
  LOG(DEBUG) << "file_utils: prepareFileResponse prepared response code="
             << outResponse.status_line.status_code
             << " content-type=" << outFile.content_type
             << " length=" << outFile.size;
  return 0;
}

}  // namespace file_utils
