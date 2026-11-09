#pragma once

#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

namespace catalist::kdl {

struct value {
  enum class type { String, Number, Boolean, Null } valueType = type::Null;
  std::string stringVal;
  double numberVal = 0.0;
  bool boolVal = false;

  std::string asString() const;
  int asInt(int defaultVal = 0) const;
  double asDouble(double defaultVal = 0.0) const;
  bool asBool(bool defaultVal = false) const;
};

struct node {
  std::string name;
  std::vector<value> args;
  std::unordered_map<std::string, value> props;
  std::vector<node> children;

  bool hasProp(const std::string& key) const;
  value getProp(const std::string& key,
                const value& defaultVal = value{}) const;
  std::string getPropString(const std::string& key,
                            const std::string& defaultVal = "") const;
  int getPropInt(const std::string& key, int defaultVal = 0) const;
  bool getPropBool(const std::string& key, bool defaultVal = false) const;

  value getArg(size_t index, const value& defaultVal = value{}) const;
  std::string getArgString(size_t index,
                           const std::string& defaultVal = "") const;

  std::vector<node> getChildren(const std::string& childName) const;
  std::optional<node> getChild(const std::string& childName) const;
};

struct document {
  std::vector<node> rootNodes;

  std::vector<node> getNodes(const std::string& name) const;
  std::optional<node> getNode(const std::string& name) const;

  static document parse(const std::string& content, std::string& outErr);
  std::string serialize(int indent = 0) const;
};

}  // namespace catalist::kdl
