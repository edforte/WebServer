#include "AutoindexHandler.hpp"

#include <dirent.h>
#include <errno.h>
#include <sys/stat.h>

#include <algorithm>
#include <sstream>
#include <vector>

#include "Connection.hpp"
#include "HttpStatus.hpp"
#include "Logger.hpp"
#include "Uri.hpp"
#include "constants.hpp"
#include "http_utils.hpp"

// Small RAII guard for DIR* since project targets C++98 (no unique_ptr)
namespace {
struct DirGuard {
  DIR* dir;
  explicit DirGuard(DIR* d_) : dir(d_) {}
  ~DirGuard() {
    if (dir) {
      closedir(dir);
    }
  }
  DIR* get() const {
    return dir;
  }

 private:
  DirGuard(const DirGuard&);
  DirGuard& operator=(const DirGuard&);
};
}  // namespace

AutoindexHandler::AutoindexHandler(const std::string& dirpath,
                                   const std::string& display_path)
    : dirpath_(dirpath), uri_path_(display_path) {}

AutoindexHandler::~AutoindexHandler() {}

HandlerResult AutoindexHandler::start(Connection& conn) {
  const std::string& method = conn.request.request_line.method;
  // Only GET and HEAD are allowed for autoindex
  if (method != "GET" && method != "HEAD") {
    conn.response.addHeader("Allow", "GET, HEAD");
    conn.prepareErrorResponse(http::S_405_METHOD_NOT_ALLOWED);
    return HR_DONE;
  }
  DIR* raw_d = opendir(dirpath_.c_str());
  if (!raw_d) {
    LOG_PERROR(ERROR, "opendir");
    conn.prepareErrorResponse(http::S_500_INTERNAL_SERVER_ERROR);
    return HR_DONE;
  }

  // RAII wrapper: ensure directory is closed on all exits (including
  // exceptions)
  DirGuard d(raw_d);

  std::ostringstream body;
  body << "<!DOCTYPE html>" << CRLF;
  body << "<html>" << CRLF;
  // Use the user-facing URI path in title and heading instead of the
  // filesystem path to avoid leaking internal server structure.
  body << "<head>" << CRLF;
  body << "<meta charset=\"utf-8\">" << CRLF;
  body << "<title>Index of " << http::escapeHtml(uri_path_) << "</title>"
       << CRLF;
  body << "</head>" << CRLF;
  body << "<body>" << CRLF;
  body << "<h1>Index of " << http::escapeHtml(uri_path_) << "</h1>" << CRLF;
  body << "<ul>" << CRLF;

  // Collect entries first so we can sort them alphabetically
  // According to POSIX, readdir() may return NULL either on end-of-directory
  // or on error. To distinguish the two cases we must set errno = 0
  // before the loop and check errno after the loop.
  std::vector<std::string> entries;
  struct dirent* ent;
  errno = 0;
  while ((ent = readdir(d.get())) != NULL) {
    std::string name = ent->d_name;
    if (name == "." || name == "..") {
      continue;
    }
    entries.push_back(name);
  }

  if (errno != 0) {
    LOG_PERROR(ERROR, "readdir");
    conn.prepareErrorResponse(http::S_500_INTERNAL_SERVER_ERROR);
    return HR_DONE;
  }

  // sort alphabetically (lexicographical, case-sensitive)
  std::sort(entries.begin(), entries.end());

  // emit sorted entries
  for (std::vector<std::string>::size_type i = 0; i < entries.size(); ++i) {
    std::string name = entries[i];

    // detect if entry is directory
    bool is_dir = false;
    std::string fullpath = dirpath_;
    if (!fullpath.empty() && fullpath[fullpath.size() - 1] != '/') {
      fullpath += '/';
    }
    fullpath += name;
    struct stat st;
    if (stat(fullpath.c_str(), &st) == 0) {
      if (S_ISDIR(st.st_mode)) {
        is_dir = true;
      }
    } else {
      LOG_PERROR(ERROR, ("stat failed for: " + fullpath).c_str());
    }

    // Build href as an absolute path by prefixing the user-visible URI path.
    // Ensure uri_path_ starts with '/' and ends with '/'.
    std::string base = uri_path_;
    if (base.empty()) {
      base = "/";
    }
    if (base[0] != '/') {
      base.insert(base.begin(), '/');
    }
    if (base[base.size() - 1] != '/') {
      base += '/';
    }

    // URL-encode the filename for safe use in href attribute
    std::string href = base + http::Uri::encode(name);
    std::string display = name;
    if (is_dir) {
      href += '/';
      display += '/';
    }

    // HTML-escape the href and display text for safe HTML output
    std::string href_escaped = http::escapeHtml(href);
    std::string disp_escaped = http::escapeHtml(display);
    body << "<li><a href=\"" << href_escaped << "\">" << disp_escaped
         << "</a></li>" << CRLF;
  }

  body << "</ul>" << CRLF;
  body << "</body>" << CRLF;
  body << "</html>" << CRLF;
  // Prepare response headers. For HEAD requests we must send headers only,
  // but Content-Length must reflect the body size that would have been sent
  // for a GET.
  std::string body_str = body.str();

  conn.response.status_line.version = HTTP_VERSION;
  conn.response.status_line.status_code = http::S_200_OK;
  conn.response.status_line.reason = http::reasonPhrase(http::S_200_OK);
  conn.response.addHeader("Content-Type", "text/html; charset=utf-8");
  std::ostringstream oss;
  oss << body_str.size();
  conn.response.addHeader("Content-Length", oss.str());

  if (method == "HEAD") {
    // No body for HEAD; send headers only.
    conn.response.getBody().data = "";
    std::ostringstream header_stream;
    header_stream << conn.response.startLine() << CRLF;
    header_stream << conn.response.serializeHeaders();
    header_stream << CRLF;
    conn.write_buffer = header_stream.str();
    conn.write_offset = 0;
    return HR_DONE;
  }

  // GET (or other methods that return body) - include the body.
  conn.response.getBody().data = body_str;
  conn.write_buffer = conn.response.serialize();
  conn.write_offset = 0;

  return HR_DONE;
}

HandlerResult AutoindexHandler::resume(Connection& conn) {
  (void)conn;
  return HR_DONE;
}
