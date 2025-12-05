#include "CgiHandler.hpp"

#include <fcntl.h>
#include <limits.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>

#include <cerrno>
#include <cstdlib>
#include <cstring>
#include <sstream>

#include "Connection.hpp"
#include "HttpStatus.hpp"
#include "Logger.hpp"
#include "constants.hpp"
#include "utils.hpp"

CgiHandler::CgiHandler(const std::string& script_path)
    : script_path_(script_path),
      script_pid_(-1),
      pipe_read_fd_(-1),
      pipe_write_fd_(-1),
      process_started_(false),
      headers_parsed_(false),
      accumulated_output_() {}

CgiHandler::~CgiHandler() {
  cleanupProcess();
}

void CgiHandler::cleanupProcess() {
  if (pipe_read_fd_ >= 0) {
    close(pipe_read_fd_);
    pipe_read_fd_ = -1;
  }
  if (pipe_write_fd_ >= 0) {
    close(pipe_write_fd_);
    pipe_write_fd_ = -1;
  }
  if (script_pid_ > 0) {
    int status;
    waitpid(script_pid_, &status, 0);  // Blocking wait to reap child process
    script_pid_ = -1;
  }
}

int CgiHandler::getMonitorFd() const {
  return pipe_read_fd_;
}

HandlerResult CgiHandler::start(Connection& conn) {
  LOG(DEBUG) << "CgiHandler: starting CGI script " << script_path_;

  // Security validation: check script path
  std::string error_msg;
  if (!validateScriptPath(script_path_, error_msg)) {
    LOG(ERROR) << "CgiHandler: security validation failed: " << error_msg;
    conn.prepareErrorResponse(http::S_403_FORBIDDEN);
    return HR_DONE;
  }

  // Create pipes for communication
  int pipe_to_cgi[2], pipe_from_cgi[2];
  if (pipe(pipe_to_cgi) == -1 || pipe(pipe_from_cgi) == -1) {
    LOG_PERROR(ERROR, "CgiHandler: pipe failed");
    conn.prepareErrorResponse(http::S_500_INTERNAL_SERVER_ERROR);
    return HR_DONE;
  }

  // Fork CGI process
  script_pid_ = fork();
  if (script_pid_ == -1) {
    LOG_PERROR(ERROR, "CgiHandler: fork failed");
    close(pipe_to_cgi[0]);
    close(pipe_to_cgi[1]);
    close(pipe_from_cgi[0]);
    close(pipe_from_cgi[1]);
    conn.prepareErrorResponse(http::S_500_INTERNAL_SERVER_ERROR);
    return HR_DONE;
  }

  if (script_pid_ == 0) {
    // Child process - execute CGI script
    close(pipe_to_cgi[1]);    // Close write end
    close(pipe_from_cgi[0]);  // Close read end

    // Redirect stdin/stdout
    dup2(pipe_to_cgi[0], STDIN_FILENO);
    dup2(pipe_from_cgi[1], STDOUT_FILENO);
    dup2(pipe_from_cgi[1], STDERR_FILENO);

    close(pipe_to_cgi[0]);
    close(pipe_from_cgi[1]);

    // Setup environment
    setupEnvironment(conn);

    // Get absolute path before chdir
    char abs_path_buf[PATH_MAX];
    if (realpath(script_path_.c_str(), abs_path_buf) == NULL) {
      exit(1);
    }
    std::string abs_script_path = abs_path_buf;

    // Change to script directory for relative paths
    size_t last_slash = abs_script_path.find_last_of('/');
    std::string script_name;
    if (last_slash != std::string::npos) {
      std::string script_dir = abs_script_path.substr(0, last_slash);
      script_name = abs_script_path.substr(last_slash + 1);
      if (chdir(script_dir.c_str()) != 0) {
        exit(1);
      }
    } else {
      script_name = abs_script_path;
    }

    // Execute script using ./filename (we're in its directory)
    // On Unix, scripts must be executed with ./ prefix when in current
    // directory
    std::string script_exec = "./" + script_name;
    const char* script_c = script_exec.c_str();
    execl(script_c, script_name.c_str(), (char*)NULL);

    // If we reach here, exec failed
    exit(EXIT_NOT_FOUND);
  }

  // Parent process
  close(pipe_to_cgi[0]);    // Close read end
  close(pipe_from_cgi[1]);  // Close write end

  pipe_write_fd_ = pipe_to_cgi[1];
  pipe_read_fd_ = pipe_from_cgi[0];
  process_started_ = true;

  // Set pipe to non-blocking mode for asynchronous I/O
  if (set_nonblocking(pipe_read_fd_) < 0) {
    LOG_PERROR(ERROR, "CgiHandler: failed to set pipe non-blocking");
    conn.prepareErrorResponse(http::S_500_INTERNAL_SERVER_ERROR);
    cleanupProcess();
    return HR_DONE;
  }

  // Send request body to CGI if present
  const std::string& body = conn.request.getBody().data;
  if (!body.empty()) {
    size_t total_written = 0;
    const char* buf = body.c_str();
    size_t remaining = body.length();
    while (remaining > 0) {
      ssize_t written = write(pipe_write_fd_, buf + total_written, remaining);
      if (written == -1) {
        if (errno == EINTR) {
          continue;  // Retry on interrupt
        }
        LOG_PERROR(ERROR, "CgiHandler: write to CGI failed");
        break;
      }
      total_written += static_cast<size_t>(written);
      // Missing timeout handling: CGI scripts can potentially run indefinitely,
      // causing resource exhaustion. The implementation should set a timeout
      // (e.g., using alarm() in the child process or a timer in the parent) and
      // kill scripts that exceed it. This is a standard CGI security practice.
      remaining -= static_cast<size_t>(written);
    }
  }
  close(pipe_write_fd_);
  pipe_write_fd_ = -1;

  LOG(DEBUG) << "CgiHandler: fork/exec done, pid=" << script_pid_
             << ", pipe_read_fd=" << pipe_read_fd_;

  // Start reading CGI output (non-blocking)
  return readCgiOutput(conn);
}

HandlerResult CgiHandler::resume(Connection& conn) {
  if (!process_started_) {
    return HR_ERROR;
  }
  return readCgiOutput(conn);
}

HandlerResult CgiHandler::readCgiOutput(Connection& conn) {
  char buffer[WRITE_BUF_SIZE];
  ssize_t bytes_read;

  // Read available output from CGI (non-blocking)
  // The pipe is set to O_NONBLOCK, so read() will return -1 with EAGAIN
  // when no data is available, preventing blocking
  while ((bytes_read = read(pipe_read_fd_, buffer, sizeof(buffer))) > 0) {
    accumulated_output_.append(buffer, bytes_read);
  }

  if (bytes_read == -1) {
    if (errno == EAGAIN || errno == EWOULDBLOCK) {
      // No more data available right now, need to wait for more
      LOG(DEBUG) << "CgiHandler: would block, accumulated "
                 << accumulated_output_.size() << " bytes so far";
      return HR_WOULD_BLOCK;
    }
    // Real error
    LOG_PERROR(ERROR, "CgiHandler: read from CGI failed");
    cleanupProcess();
    conn.prepareErrorResponse(http::S_500_INTERNAL_SERVER_ERROR);
    return HR_DONE;
  }

  // bytes_read == 0 means EOF - CGI finished writing
  LOG(DEBUG) << "CgiHandler: CGI finished, total output: "
             << accumulated_output_.size() << " bytes";

  // Close the pipe read fd since we're done reading
  close(pipe_read_fd_);
  pipe_read_fd_ = -1;

  // Wait for process to finish
  int status;
  waitpid(script_pid_, &status, 0);
  script_pid_ = -1;

  if (!WIFEXITED(status) || WEXITSTATUS(status) != 0) {
    int exit_code = WEXITSTATUS(status);
    if (exit_code == EXIT_NOT_FOUND) {
      LOG(ERROR) << "CGI exec failed: command not found for " << script_path_;
    } else {
      LOG(ERROR) << "CGI script exited with error status: " << exit_code;
    }
    conn.prepareErrorResponse(http::S_500_INTERNAL_SERVER_ERROR);
    return HR_DONE;
  }

  // Process all output data
  if (!accumulated_output_.empty()) {
    parseOutput(conn, accumulated_output_);
  }

  // Ensure we have response headers if none were parsed
  if (!headers_parsed_) {
    conn.response.status_line.version = conn.getHttpVersion();
    conn.response.status_line.status_code = http::S_200_OK;
    conn.response.status_line.reason = "OK";
    conn.response.addHeader("Content-Type", "text/plain");

    std::ostringstream response_stream;
    response_stream << conn.response.startLine() << CRLF;
    response_stream << conn.response.serializeHeaders();
    response_stream << CRLF;
    response_stream << accumulated_output_;

    conn.write_buffer = response_stream.str();
  }

  LOG(DEBUG) << "CGI finished, response size: " << conn.write_buffer.size();
  return HR_DONE;
}

HandlerResult CgiHandler::parseOutput(Connection& conn,
                                      const std::string& data) {
  if (!headers_parsed_) {
    remaining_data_ += data;

    // Look for headers end - support both CRLF CRLF and LF LF
    size_t headers_end = remaining_data_.find(CRLF CRLF);
    size_t separator_len = 4;  // Length of "\r\n\r\n"
    if (headers_end == std::string::npos) {
      // Try LF LF (Unix-style)
      headers_end = remaining_data_.find("\n\n");
      separator_len = 2;  // Length of "\n\n"
    }
    if (headers_end == std::string::npos) {
      return HR_WOULD_BLOCK;  // Need more data
    }

    // Parse headers
    std::string headers_part = remaining_data_.substr(0, headers_end);
    std::string body_part = remaining_data_.substr(headers_end + separator_len);

    // Set default status
    conn.response.status_line.version = conn.getHttpVersion();
    conn.response.status_line.status_code = http::S_200_OK;
    conn.response.status_line.reason = "OK";

    // Parse header lines
    std::istringstream iss(headers_part);
    std::string line;
    while (std::getline(iss, line) && !line.empty()) {
      if (!line.empty() && line[line.length() - 1] == '\r') {
        line.erase(line.length() - 1);
      }

      size_t colon = line.find(':');
      if (colon != std::string::npos) {
        std::string name = line.substr(0, colon);
        std::string value = line.substr(colon + 1);

        // Trim whitespace
        while (!value.empty() && value[0] == ' ') {
          value.erase(0, 1);
        }

        if (name == "Status") {
          // Parse status line
          size_t space = value.find(' ');
          if (space != std::string::npos) {
            int status_code = atoi(value.substr(0, space).c_str());
            conn.response.status_line.status_code =
                http::intToStatus(status_code);
            conn.response.status_line.reason = value.substr(space + 1);
          }
        } else {
          conn.response.addHeader(name, value);
        }
      }
    }

    headers_parsed_ = true;
    remaining_data_ = body_part;

    // Build response headers
    std::ostringstream response_stream;
    response_stream << conn.response.startLine() << CRLF;
    response_stream << conn.response.serializeHeaders();
    response_stream << CRLF;

    conn.write_buffer = response_stream.str();

    if (!body_part.empty()) {
      conn.write_buffer += body_part;
    }

    remaining_data_.clear();
    // Headers and body parsed, parsing is done
    return HR_DONE;
  } else {
    // Headers already parsed, just add body data
    conn.write_buffer += data;
    // Body data appended, parsing is done
    return HR_DONE;
  }
}

void CgiHandler::setupEnvironment(Connection& conn) {
  // Set PATH for script execution
  setenv("PATH", "/usr/local/bin:/usr/bin:/bin", 1);

  // Standard CGI environment variables
  setenv("REQUEST_METHOD", conn.request.request_line.method.c_str(), 1);
  setenv("REQUEST_URI", conn.request.request_line.uri.c_str(), 1);
  setenv("SERVER_PROTOCOL", conn.request.request_line.version.c_str(), 1);
  setenv("GATEWAY_INTERFACE", "CGI/1.1", 1);
  setenv("SERVER_NAME", "webserv", 1);
  setenv("SERVER_PORT", "8080", 1);
  setenv("SCRIPT_NAME", script_path_.c_str(), 1);

  // Query string
  std::string uri = conn.request.request_line.uri;
  size_t query_pos = uri.find('?');
  std::string uri_no_query =
      (query_pos != std::string::npos) ? uri.substr(0, query_pos) : uri;
  std::string query_string =
      (query_pos != std::string::npos) ? uri.substr(query_pos + 1) : "";
  setenv("QUERY_STRING", query_string.c_str(), 1);

  // Determine PATH_INFO: extra path after script name
  std::string path_info;
  if (uri_no_query.find(script_path_) == 0) {
    path_info = uri_no_query.substr(script_path_.length());
    // Ensure path_info starts with '/' if present and not empty
    if (!path_info.empty() && path_info[0] != '/') {
      path_info = "/" + path_info;
    }
  } else {
    path_info = "";
  }
  setenv("PATH_INFO", path_info.c_str(), 1);

  // Content headers
  std::string content_type, content_length;
  if (conn.request.getHeader("Content-Type", content_type)) {
    setenv("CONTENT_TYPE", content_type.c_str(), 1);
  }
  if (conn.request.getHeader("Content-Length", content_length)) {
    setenv("CONTENT_LENGTH", content_length.c_str(), 1);
  } else {
    std::ostringstream len_ss;
    len_ss << conn.request.getBody().data.length();
    setenv("CONTENT_LENGTH", len_ss.str().c_str(), 1);
  }

  // HTTP headers as environment variables
  // Note: Request class needs to provide header iteration interface
}

// Security validation: check if script path is safe to execute
bool CgiHandler::validateScriptPath(const std::string& path,
                                    std::string& error_msg) {
  // Check for path traversal attacks
  if (!isPathTraversalSafe(path)) {
    error_msg = "Path traversal detected in script path";
    return false;
  }

  // Check if file exists and is a regular file
  struct stat st;
  if (stat(path.c_str(), &st) != 0) {
    error_msg = "Script file not found";
    return false;
  }

  if (!S_ISREG(st.st_mode)) {
    error_msg = "Script path is not a regular file";
    return false;
  }

  // Check if file has executable permissions
  if (!isExecutable(path)) {
    error_msg = "Script file is not executable";
    return false;
  }

  // Check if file extension is allowed
  if (!isAllowedExtension(path)) {
    error_msg = "Script file extension is not allowed";
    return false;
  }

  return true;
}

// Check if path contains path traversal sequences
bool CgiHandler::isPathTraversalSafe(const std::string& path) {
  // Check for obvious path traversal patterns
  if (path.find("..") != std::string::npos) {
    return false;
  }

  // Get absolute path and verify it doesn't escape allowed directory
  char resolved_path[PATH_MAX];
  if (realpath(path.c_str(), resolved_path) == NULL) {
    // If realpath fails, the file may not exist yet or path is invalid
    // For security, we reject such paths
    return false;
  }

  // Verify the resolved path starts with allowed CGI directory
  // This prevents symlink attacks and ensures scripts are in designated area
  std::string resolved(resolved_path);

  // Expected CGI directories (adjust based on your configuration)
  const char* allowed_dirs[] = {"./www/cgi-bin/", "/www/cgi-bin/",
                                "www/cgi-bin/", NULL};

  for (int i = 0; allowed_dirs[i] != NULL; ++i) {
    std::string allowed_dir = allowed_dirs[i];
    // Convert relative path to absolute if needed
    char abs_allowed[PATH_MAX];
    if (realpath(allowed_dir.c_str(), abs_allowed) != NULL) {
      std::string abs_allowed_str(abs_allowed);
      if (resolved.find(abs_allowed_str) == 0) {
        return true;
      }
    }
  }

  // Also check if the original path (before resolution) is within allowed
  // dirs
  for (int i = 0; allowed_dirs[i] != NULL; ++i) {
    if (path.find(allowed_dirs[i]) == 0) {
      return true;
    }
  }

  return false;
}

// Check if file has executable permissions
bool CgiHandler::isExecutable(const std::string& path) {
  struct stat st;
  if (stat(path.c_str(), &st) != 0) {
    return false;
  }

  // Check if file has execute permission for owner, group, or others
  return (st.st_mode & (S_IXUSR | S_IXGRP | S_IXOTH)) != 0;
}

// Check if file extension is in the allowed list
bool CgiHandler::isAllowedExtension(const std::string& path) {
  // Whitelist of allowed CGI script extensions
  const char* allowed_extensions[] = {".sh",   // Shell scripts
                                      ".py",   // Python scripts
                                      ".pl",   // Perl scripts
                                      ".php",  // PHP scripts
                                      ".cgi",  // Generic CGI scripts
                                      NULL};

  size_t dot_pos = path.find_last_of('.');
  if (dot_pos == std::string::npos) {
    // No extension found
    return false;
  }

  std::string extension = path.substr(dot_pos);

  for (int i = 0; allowed_extensions[i] != NULL; ++i) {
    if (extension == allowed_extensions[i]) {
      return true;
    }
  }

  return false;
}
