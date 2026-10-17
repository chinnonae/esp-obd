#pragma once

#include <cstdint>

// Pure acceptance/filter matching, usable identically by software filtering
// in the diagnostic/monitoring layers and (optionally) to derive a TWAI
// acceptance filter -- see docs/ARCHITECTURE.md, "All CAN filtering must be
// applied in software ... it must not change the observable matching rules."

namespace esp_obd::can {

struct CanFilter {
  uint32_t mask = 0;
  uint32_t filterValue = 0;
};

constexpr bool operator==(const CanFilter& a, const CanFilter& b) {
  return a.mask == b.mask && a.filterValue == b.filterValue;
}

constexpr bool matchesFilter(uint32_t id, const CanFilter& filter) {
  return (id & filter.mask) == (filter.filterValue & filter.mask);
}

// An exact-match receive address (e.g. ATCRAxxx) is a filter with every bit
// masked and the target id as both filter value and mask.
constexpr CanFilter exactIdFilter(uint32_t id) {
  return CanFilter{/*mask=*/0xFFFFFFFFu, /*filterValue=*/id};
}

}  // namespace esp_obd::can
