#include <doctest/doctest.h>

#include <string>

#include "catalist/kdl.hpp"

using catalist::kdl::document;
using catalist::kdl::node;
using catalist::kdl::value;

namespace {

void checkSameValue(const value& lhs, const value& rhs) {
  CHECK(lhs.valueType == rhs.valueType);
  CHECK(lhs.asString() == rhs.asString());
}

void checkSameNode(const node& lhs, const node& rhs) {
  CAPTURE(lhs.name);
  CHECK(lhs.name == rhs.name);
  REQUIRE(lhs.args.size() == rhs.args.size());
  for (size_t i = 0; i < lhs.args.size(); ++i)
    checkSameValue(lhs.args[i], rhs.args[i]);
  REQUIRE(lhs.props.size() == rhs.props.size());
  for (const auto& [key, propValue] : lhs.props) {
    CAPTURE(key);
    REQUIRE(rhs.hasProp(key));
    checkSameValue(propValue, rhs.getProp(key));
  }
  REQUIRE(lhs.children.size() == rhs.children.size());
  for (size_t i = 0; i < lhs.children.size(); ++i)
    checkSameNode(lhs.children[i], rhs.children[i]);
}

const char* kSampleKdl = R"kdl(// leading comment
general {
    theme "nord"
    mouse false
    reverse-sort true
}

top-bar height=3 border=true {
    tab "anime" label="Anime" show-count=true
    spacer
    text "v1.2.3" color="overlay0"
}
ratio 1.5
)kdl";

}  // namespace

TEST_CASE("kdl parses nodes args props and children") {
  std::string err;
  auto doc = document::parse(kSampleKdl, err);
  CHECK(err == "");
  REQUIRE(doc.rootNodes.size() == 3);

  auto general = doc.getNode("general");
  REQUIRE(general.has_value());
  CHECK(general->getChild("theme")->getArgString(0) == "nord");
  CHECK(general->getChild("mouse")->getArg(0).asBool(true) == false);

  auto topBar = doc.getNode("top-bar");
  REQUIRE(topBar.has_value());
  CHECK(topBar->getPropInt("height") == 3);
  CHECK(topBar->getPropBool("border") == true);
  REQUIRE(topBar->children.size() == 3);
  CHECK(topBar->children[0].getPropString("label") == "Anime");
  CHECK(topBar->children[2].getArgString(0) == "v1.2.3");

  CHECK(doc.getNode("ratio")->getArg(0).asDouble() == doctest::Approx(1.5));
}

TEST_CASE("kdl serialize then parse round trips the tree") {
  std::string err;
  auto original = document::parse(kSampleKdl, err);
  auto serialized = original.serialize();
  auto reparsed = document::parse(serialized, err);
  CHECK(err == "");

  REQUIRE(original.rootNodes.size() == reparsed.rootNodes.size());
  for (size_t i = 0; i < original.rootNodes.size(); ++i)
    checkSameNode(original.rootNodes[i], reparsed.rootNodes[i]);
}

TEST_CASE("kdl serialize uses four space indentation") {
  std::string err;
  auto doc = document::parse("parent {\n child \"x\"\n}\n", err);
  CHECK(doc.serialize() == "parent {\n    child \"x\"\n}\n");
}
