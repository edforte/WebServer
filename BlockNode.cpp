#include "BlockNode.hpp"

BlockNode::BlockNode() {}

BlockNode::BlockNode(const std::string& type_str, const std::string& param_str)
    : type(type_str), param(param_str) {}

BlockNode::BlockNode(const BlockNode& other)
    : type(other.type),
      param(other.param),
      directives(other.directives),
      sub_blocks(other.sub_blocks) {}

BlockNode& BlockNode::operator=(const BlockNode& other) {
  if (this != &other) {
    type = other.type;
    param = other.param;
    directives = other.directives;
    sub_blocks = other.sub_blocks;
  }
  return *this;
}

BlockNode::~BlockNode() {}
