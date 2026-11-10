#ifndef CATALIST_CORE_CLOCK_HPP_
#define CATALIST_CORE_CLOCK_HPP_

#include <chrono>

#include "catalist/core/date.hpp"

namespace catalist {

// Where "today" comes from, so the tests can pin the date.
class iClock {
public:
  using timePoint = std::chrono::system_clock::time_point;

  virtual ~iClock() = default;
  [[nodiscard]] virtual timePoint now() const = 0;
};

class realClock : public iClock {
public:
  [[nodiscard]] timePoint now() const override;
};

[[nodiscard]] isoDate localDate(const iClock& clock);

}  // namespace catalist

#endif  // CATALIST_CORE_CLOCK_HPP_
