#include <string.h>
#include "baldr/transitstop.h"

namespace valhalla {
namespace baldr {

// Constructor with arguments
TransitStop::TransitStop(const uint32_t stopid, const uint32_t one_stop_offset,
            const uint32_t name_offset, const uint32_t desc_offset,
            const uint32_t parent_stopid, const uint32_t fare_zoneid)
    : stopid_(stopid),
      one_stop_offset_(one_stop_offset),
      name_offset_(name_offset),
      desc_offset_(desc_offset),
      parent_stopid_(parent_stopid),
      fare_zoneid_(fare_zoneid) {
}

// Get the internal stop Id.
uint32_t TransitStop::stopid() const {
  return stopid_;
}

// Get the TransitLand one-stop Id.
uint32_t TransitStop::one_stop_offset() const {
  return one_stop_offset_;
}

// Get the text/name offset for the stop name.
uint32_t TransitStop::name_offset() const {
  return name_offset_;
}

// Get the text/name offset for the stop description.
uint32_t TransitStop::desc_offset() const {
  return desc_offset_;
}

// Get the internal stop Id of the parent stop.
uint32_t TransitStop::parent_stopid() const {
  return parent_stopid_;
}

// Get the fare zone Id.
uint32_t TransitStop::fare_zoneid() const {
  return fare_zoneid_;
}

// operator < - for sorting. Sort by stop Id.
bool TransitStop::operator < (const TransitStop& other) const {
  return stopid() < other.stopid();
}

}
}
