#pragma once

#include <string>
#include <vector>

#include "DirectiveNode.hpp"

class BlockNode {
 public:
  BlockNode();
  BlockNode(const std::string& type_str, const std::string& param_str = "");
  BlockNode(const BlockNode& other);
  BlockNode& operator=(const BlockNode& other);
  ~BlockNode();

  std::string type;   // e.g. "server" or "location" or "root"
  std::string param;  // optional parameter for block (e.g. /path)
  std::vector<DirectiveNode>
      directives;                     // directives directly inside this block
  std::vector<BlockNode> sub_blocks;  // nested blocks
};
