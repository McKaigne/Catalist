#ifndef CATALIST_STORAGE_MAL_XML_HPP_
#define CATALIST_STORAGE_MAL_XML_HPP_

#include <cstddef>
#include <string>
#include <vector>

#include "core/entry.hpp"

namespace catalist::mal_xml {

[[nodiscard]] std::vector<Entry> Parse(const std::string& xml);

struct Exported {
	std::string xml;
	size_t written = 0;
	size_t skipped = 0;
};

[[nodiscard]] Exported Write(const std::vector<Entry>& entries, MediaType type);

}  // namespace catalist::mal_xml

#endif  // CATALIST_STORAGE_MAL_XML_HPP_
