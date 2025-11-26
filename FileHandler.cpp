#include "FileHandler.hpp"

#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>

#include <cerrno>
#include <cstring>
#include <sstream>

#include "Connection.hpp"
#include "HttpStatus.hpp"
#include "Logger.hpp"
#include "Request.hpp"
#include "constants.hpp"
#include "file_utils.hpp"

FileHandler::FileHandler(const std::string& path)
    : path_(path), fi_(), start_offset_(0), end_offset_(-1), active_(false) {
  fi_.fd = -1;
}

FileHandler::~FileHandler() {
  if (fi_.fd >= 0) {
    file_utils::closeFile(fi_);
  }
}

HandlerResult FileHandler::start(Connection& conn) {
  const std::string& method = conn.request.request_line.method;

  LOG(DEBUG) << "FileHandler: processing " << method
             << " request for fd=" << conn.fd << " path=" << path_;

  if (method == "GET") {
    return handleGet(conn);
  } else if (method == "HEAD") {
    return handleHead(conn);
  } else if (method == "POST") {
    return handlePost(conn);
  } else if (method == "PUT") {
    return handlePut(conn);
  } else if (method == "DELETE") {
    return handleDelete(conn);
  }

  // Unsupported method for file resources
  conn.prepareErrorResponse(http::S_405_METHOD_NOT_ALLOWED);
  return HR_DONE;
}

HandlerResult FileHandler::resume(Connection& conn) {
  if (!active_) {
    return HR_DONE;
  }

  // Only GET needs streaming (HEAD/PUT/DELETE complete in start())
  int r = file_utils::streamToSocket(conn.fd, fi_.fd, start_offset_,
                                     end_offset_ + 1);
  if (r < 0) {
    file_utils::closeFile(fi_);
    active_ = false;
    return HR_ERROR;
  }
  if (r == 1) {
    return HR_WOULD_BLOCK;
  }
  file_utils::closeFile(fi_);
  active_ = false;
  return HR_DONE;
}

HandlerResult FileHandler::handleGet(Connection& conn) {
  std::string range;
  const std::string* rangePtr = NULL;
  if (conn.request.getHeader("Range", range)) {
    rangePtr = &range;
  }

  off_t out_start = 0, out_end = 0;
  int r = file_utils::prepareFileResponse(path_, rangePtr, conn.response, fi_,
                                          out_start, out_end);
  if (r == -1) {
    conn.prepareErrorResponse(http::S_404_NOT_FOUND);
    return HR_DONE;
  }
  if (r == -2) {
    // Invalid range: caller should prepare a 416 response using Connection
    std::ostringstream cr;
    cr << "bytes */" << out_end;  // out_end carries file_size on -2
    conn.response.addHeader("Content-Range", cr.str());
    conn.prepareErrorResponse(http::S_416_RANGE_NOT_SATISFIABLE);
    return HR_DONE;
  }

  start_offset_ = out_start;
  end_offset_ = out_end;
  active_ = true;

  // Write only headers to connection so we can stream body
  std::ostringstream header_stream;
  header_stream << conn.response.startLine() << CRLF;
  header_stream << conn.response.serializeHeaders();
  header_stream << CRLF;
  conn.write_buffer = header_stream.str();
  conn.write_offset = 0;

  return HR_WOULD_BLOCK;  // Body streaming will occur via resume/sendfile
}

HandlerResult FileHandler::handleHead(Connection& conn) {
  // HEAD is like GET but without the response body
  FileInfo fi;
  off_t start = 0, end = 0;

  std::string range;
  const std::string* rangePtr = NULL;
  if (conn.request.getHeader("Range", range)) {
    rangePtr = &range;
  }

  int r = file_utils::prepareFileResponse(path_, rangePtr, conn.response, fi,
                                          start, end);

  if (r == -1) {
    conn.prepareErrorResponse(http::S_404_NOT_FOUND);
    return HR_DONE;
  }
  if (r == -2) {
    // Invalid range: caller should prepare a 416 response using Connection
    std::ostringstream cr;
    cr << "bytes */" << end;  // end carries file_size on -2
    conn.response.addHeader("Content-Range", cr.str());
    conn.prepareErrorResponse(http::S_416_RANGE_NOT_SATISFIABLE);
    return HR_DONE;
  }

  // Success - close the file since we don't need to send body
  file_utils::closeFile(fi);

  // HEAD response has headers but no body
  conn.response.getBody().data = "";

  std::ostringstream header_stream;
  header_stream << conn.response.startLine() << CRLF;
  header_stream << conn.response.serializeHeaders();
  header_stream << CRLF;
  conn.write_buffer = header_stream.str();

  return HR_DONE;
}

HandlerResult FileHandler::handlePost(Connection& conn) {
  // Simple POST implementation: echo back the POST data with success message
  conn.response.status_line.version = HTTP_VERSION;
  conn.response.status_line.status_code = http::S_201_CREATED;
  conn.response.status_line.reason = http::reasonPhrase(http::S_201_CREATED);

  std::ostringstream resp_body;
  resp_body << "POST request processed successfully" << CRLF;
  resp_body << "URI: " << conn.request.request_line.uri << CRLF;
  resp_body << "Content received: " << conn.request.getBody().size() << " bytes"
            << CRLF;
  resp_body << "Data:" << CRLF << conn.request.getBody().data;

  conn.response.getBody().data = resp_body.str();
  conn.response.addHeader("Content-Type", "text/plain; charset=utf-8");

  std::ostringstream len;
  len << conn.response.getBody().size();
  conn.response.addHeader("Content-Length", len.str());

  conn.write_buffer = conn.response.serialize();
  return HR_DONE;
}

HandlerResult FileHandler::handlePut(Connection& conn) {
  // Atomically determine if file is being created or overwritten using O_EXCL.
  // First attempt to create exclusively (O_CREAT | O_EXCL), which fails if file
  // exists. If it fails with EEXIST, the file already exists and we overwrite.
  bool created = false;
  int fd = open(path_.c_str(), O_WRONLY | O_CREAT | O_EXCL, 0600);
  if (fd >= 0) {
    // File was created (did not exist before)
    created = true;
  } else if (errno == EEXIST) {
    // File already exists, open for overwriting (include O_CREAT for
    // robustness in case file is deleted between the two open calls)
    fd = open(path_.c_str(), O_WRONLY | O_CREAT | O_TRUNC, 0600);
  }
  if (fd < 0) {
    LOG_PERROR(ERROR, "FileHandler: Failed to open file for PUT");
    conn.prepareErrorResponse(http::S_500_INTERNAL_SERVER_ERROR);
    return HR_DONE;
  }

  // Write request body to file
  const std::string& body = conn.request.getBody().data;
  size_t total_written = 0;
  ssize_t n = 0;
  while (total_written < body.size()) {
    n = write(fd, body.c_str() + total_written, body.size() - total_written);
    if (n < 0) {
      break;
    }
    total_written += static_cast<size_t>(n);
  }
  close(fd);

  if (n < 0 || total_written != body.size()) {
    LOG_PERROR(ERROR, "FileHandler: Failed to write file for PUT");
    // Remove incomplete file to avoid accumulation of partial files
    unlink(path_.c_str());
    conn.prepareErrorResponse(http::S_500_INTERNAL_SERVER_ERROR);
    return HR_DONE;
  }

  conn.response.status_line.version = HTTP_VERSION;
  if (created) {
    conn.response.status_line.status_code = http::S_201_CREATED;
    conn.response.status_line.reason = http::reasonPhrase(http::S_201_CREATED);
  } else {
    conn.response.status_line.status_code = http::S_200_OK;
    conn.response.status_line.reason = http::reasonPhrase(http::S_200_OK);
  }

  std::ostringstream resp_body;
  resp_body << "PUT request processed successfully" << CRLF;
  resp_body << "Resource: " << path_ << CRLF;
  resp_body << "Bytes written: " << total_written << CRLF;

  conn.response.getBody().data = resp_body.str();
  conn.response.addHeader("Content-Type", "text/plain; charset=utf-8");

  std::ostringstream len;
  len << conn.response.getBody().size();
  conn.response.addHeader("Content-Length", len.str());

  conn.write_buffer = conn.response.serialize();
  return HR_DONE;
}

HandlerResult FileHandler::handleDelete(Connection& conn) {
  // Check if file exists
  struct stat st;
  if (stat(path_.c_str(), &st) != 0) {
    conn.prepareErrorResponse(http::S_404_NOT_FOUND);
    return HR_DONE;
  }

  // Only allow DELETE for regular files
  if (!S_ISREG(st.st_mode)) {
    LOG(INFO) << "FileHandler: DELETE not allowed for non-regular file: " << path_;
    conn.prepareErrorResponse(http::S_403_FORBIDDEN);
    return HR_DONE;
  }

  // Try to delete the file
  if (unlink(path_.c_str()) != 0) {
    LOG_PERROR(ERROR, "FileHandler: Failed to delete file");
    conn.prepareErrorResponse(http::S_500_INTERNAL_SERVER_ERROR);
    return HR_DONE;
  }

  // 204 No Content is the standard response for successful DELETE
  conn.response.status_line.version = HTTP_VERSION;
  conn.response.status_line.status_code = http::S_204_NO_CONTENT;
  conn.response.status_line.reason = http::reasonPhrase(http::S_204_NO_CONTENT);

  conn.response.getBody().data = "";
  conn.response.addHeader("Content-Length", "0");

  conn.write_buffer = conn.response.serialize();

  LOG(INFO) << "FileHandler: Deleted resource " << path_;
  return HR_DONE;
}
