#include "DirectiveNode.hpp"

DirectiveNode::DirectiveNode() {}

DirectiveNode::DirectiveNode(const std::string& name_str,
                             const std::vector<std::string>& args_vec)
    : name(name_str), args(args_vec) {}

DirectiveNode::DirectiveNode(const DirectiveNode& other)
    : name(other.name), args(other.args) {}

DirectiveNode& DirectiveNode::operator=(const DirectiveNode& other) {
  if (this != &other) {
    name = other.name;
    args = other.args;
  }
  return *this;
}

DirectiveNode::~DirectiveNode() {}
