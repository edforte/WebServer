#pragma once

#include <sys/types.h>

#include <map>
#include <vector>

#include "Connection.hpp"
#include "Server.hpp"

class ServerManager {
 private:
  ServerManager(const ServerManager& other);
  ServerManager& operator=(const ServerManager& other);

  int efd_;
  int sfd_;
  bool stop_requested_;
  std::map<int, Server> servers_;
  std::map<int, Connection> connections_;

 public:
  ServerManager();
  ~ServerManager();

  // Initializes all servers from configuration
  void initServers(std::vector<Server>& servers);

  // Accepts new client connection on given listening socket
  void acceptConnection(int listen_fd);

  // Main event loop: waits for events and handles requests
  int run();

  // Updates epoll events for a file descriptor
  void updateEvents(int file_descriptor, u_int32_t events) const;

  void setupSignalHandlers();

  bool processSignalsFromFd();

  // Closes all connections and server sockets
  void shutdown();
};
