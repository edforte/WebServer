# Configuration Guide

This document describes the configuration file format for webserv, an HTTP/1.1 web server. The configuration uses an nginx-like syntax, so users familiar with nginx configuration should find this format intuitive.

## Overview

The configuration file uses a block-based syntax inspired by nginx. Configuration consists of:
- **Global directives** that apply to all servers
- **Server blocks** that define virtual hosts
- **Location blocks** that define URI-specific settings within a server

## Basic Syntax

- Comments start with `#` and continue to the end of the line
- Directives end with a semicolon `;`
- Blocks are enclosed in curly braces `{ }`
- Whitespace is used to separate tokens

## Configuration Structure

```
# Global directives
max_request_body 4096;
error_page 404 /404.html;

# Server blocks
server {
    listen 8080;
    root ./www;
    
    # Location blocks
    location /api {
        allow_methods GET POST;
    }
}
```

## Global Directives

These directives can be placed at the top level of the configuration file and apply as defaults to all servers.

### max_request_body

Sets the maximum allowed size of the client request body in bytes.

**Syntax:** `max_request_body <size>;`

**Context:** global, server

**Example:**
```
max_request_body 4096;
```

### error_page

Defines the URI that will be shown for the specified errors.

**Syntax:** `error_page <code> [code...] <uri>;`

**Context:** global, server, location

**Codes:** Must be 4xx or 5xx HTTP status codes

**Example:**
```
error_page 404 /404.html;
error_page 500 502 503 504 /50x.html;
```

## Server Block

A server block defines a virtual host. At least one server block is required.

**Syntax:**
```
server {
    # server directives
}
```

### listen

Sets the address and port on which the server will accept requests.

**Syntax:** `listen [address:]<port>;`

**Context:** server

**Required:** Yes

**Examples:**
```
listen 8080;           # Listen on all interfaces, port 8080
listen 127.0.0.1:8080; # Listen on localhost only
listen 0.0.0.0:80;     # Listen on all interfaces, port 80
```

### root

Sets the root directory for requests.

**Syntax:** `root <path>;`

**Context:** server, location

**Required:** Yes (at server level)

**Example:**
```
root ./www;
root /var/www/html;
```

### index

Defines files that will be used as an index when a directory is requested.

**Syntax:** `index <file>;`

**Context:** server, location

**Example:**
```
index index.html;
```

### autoindex

Enables or disables directory listing when no index file is found.

**Syntax:** `autoindex on|off;`

**Context:** server, location

**Default:** off

**Example:**
```
autoindex on;
```

### allow_methods

Limits allowed HTTP methods for the server or location.

**Syntax:** `allow_methods <method> [method...];`

**Context:** server, location

**Supported methods:** GET, POST, PUT, DELETE, HEAD

**Example:**
```
allow_methods GET POST;
allow_methods GET;
```

## Location Block

Location blocks define configuration for specific URI paths within a server.

**Syntax:**
```
location <path> {
    # location directives
}
```

The `<path>` parameter specifies the URI prefix to match.

### redirect

Defines a redirect response for the location.

**Syntax:** `redirect <code> <url>;`

**Context:** location

**Codes:** 301, 302, 303, 307, 308

**Example:**
```
location /old {
    redirect 301 /new;
}

location /external {
    redirect 302 https://example.com;
}
```

### cgi

Enables or disables CGI script execution for the location.

**Syntax:** `cgi on|off;`

**Context:** location

**Default:** off

**Example:**
```
location /cgi-bin {
    cgi on;
}
```

### Location-specific Directives

The following directives can also be used within location blocks to override server-level settings:

- `root` - Override document root
- `index` - Override index files
- `autoindex` - Override directory listing
- `allow_methods` - Override allowed methods
- `error_page` - Override error pages

## Complete Example

```
# Global settings
max_request_body 4096;
error_page 404 /404.html;
error_page 500 502 503 504 /50x.html;

# Main server
server {
    listen 8080;
    root ./www;
    autoindex on;
    index index.html;

    # Static file location
    location /static {
        root ./static;
        autoindex off;
    }

    # API with restricted methods
    location /api {
        allow_methods GET POST;
    }

    # Redirect old URLs
    location /old-page {
        redirect 301 /new-page;
    }

    # CGI scripts
    location /cgi-bin {
        cgi on;
    }
}

# Second server on different port
server {
    listen 127.0.0.1:9000;
    root ./admin;
    index admin.html;
    allow_methods GET;
}
```

## Configuration Validation

The server validates the configuration at startup and will report errors for:

- Missing required directives (listen, root in server blocks)
- Invalid port numbers (must be 1-65535)
- Invalid IP addresses
- Unrecognized directives
- Invalid boolean values (must be on/off)
- Invalid HTTP methods
- Invalid status codes for error_page (must be 4xx or 5xx)
- Invalid redirect codes (must be 301, 302, 303, 307, or 308)

## Running with a Configuration File

```bash
# Use default configuration (conf/default.conf)
./webserv

# Use a specific configuration file
./webserv my_config.conf

# Set log level (0=DEBUG, 1=INFO, 2=ERROR)
./webserv -l:0 my_config.conf
```
