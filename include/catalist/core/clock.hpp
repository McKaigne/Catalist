#ifndef CATALIST_CORE_CLOCK_HPP_
#define CATALIST_CORE_CLOCK_HPP_

#include <chrono>
#include <string>

#include "catalist/core/date.hpp"

namespace catalist {

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

class fixedClock : public iClock {
public:
  explicit fixedClock(timePoint frozenAt) : frozenAt_(frozenAt) {}
  [[nodiscard]] timePoint now() const override { return frozenAt_; }
  void set(timePoint frozenAt) { frozenAt_ = frozenAt; }
  void advance(std::chrono::seconds delta) { frozenAt_ += delta; }

private:
  timePoint frozenAt_;
};

[[nodiscard]] isoDate localDate(const iClock& clock);

// strftime-style formatting of clock.now() in local time.
[[nodiscard]] std::string formatLocalTime(const iClock& clock,
                                          const char* pattern);

}  // namespace catalist

#endif  // CATALIST_CORE_CLOCK_HPP_
