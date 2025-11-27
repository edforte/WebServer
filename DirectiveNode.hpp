#pragma once

#include <string>
#include <vector>

class DirectiveNode {
 public:
  DirectiveNode();
  DirectiveNode(const std::string& name_str,
                const std::vector<std::string>& args_vec);
  DirectiveNode(const DirectiveNode& other);
  DirectiveNode& operator=(const DirectiveNode& other);
  ~DirectiveNode();

  std::string name;
  std::vector<std::string> args;
};
