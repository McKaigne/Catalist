#pragma once

#include <string>
#include <vector>
#include <unordered_map>
#include <optional>

namespace catalist::kdl {

struct Value {
  enum class Type { String, Number, Boolean, Null } type = Type::Null;
  std::string stringVal;
  double numberVal = 0.0;
  bool boolVal = false;

  std::string asString() const;
  int asInt(int defaultVal = 0) const;
  double asDouble(double defaultVal = 0.0) const;
  bool asBool(bool defaultVal = false) const;
};

struct Node {
  std::string name;
  std::vector<Value> args;
  std::unordered_map<std::string, Value> props;
  std::vector<Node> children;

  bool hasProp(const std::string& key) const;
  Value getProp(const std::string& key, const Value& defaultVal = Value{}) const;
  std::string getPropString(const std::string& key, const std::string& defaultVal = "") const;
  int getPropInt(const std::string& key, int defaultVal = 0) const;
  bool getPropBool(const std::string& key, bool defaultVal = false) const;

  Value getArg(size_t index, const Value& defaultVal = Value{}) const;
  std::string getArgString(size_t index, const std::string& defaultVal = "") const;

  std::vector<Node> getChildren(const std::string& childName) const;
  std::optional<Node> getChild(const std::string& childName) const;
};

struct Document {
  std::vector<Node> rootNodes;

  std::vector<Node> getNodes(const std::string& name) const;
  std::optional<Node> getNode(const std::string& name) const;

  static Document parse(const std::string& content, std::string& outErr);
  std::string serialize(int indent = 0) const;
};

} // namespace catalist::kdl
