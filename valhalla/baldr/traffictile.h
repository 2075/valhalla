#ifndef VALHALLA_BALDR_TRAFFICTILE_H_
#define VALHALLA_BALDR_TRAFFICTILE_H_

// This header supports exporting as a set of C++ minimal structs
// with namespaces, but with C-99 only types and headers, to make
// binding generation in other languages simpler.
// If you do `-DC_ONLY_INTERFACE`, you'll get a header with
// C99 stdint.h, and POD structs with no constructors
#ifndef C_ONLY_INTERFACE
#include <algorithm>
#include <cassert>
#include <cstdint>
#include <vector>
#else
#include <stdint.h>
#endif

#ifndef C_ONLY_INTERFACE
namespace valhalla {
namespace baldr {
namespace traffic {

using std::uint16_t;
using std::uint32_t;
using std::uint64_t;
#endif

struct Speed {
  uint16_t speed_kmh : 7;        // km/h - so max range is 0-127km/h
  uint16_t congestion_level : 3; // some value from 0 to 7 to report back
  uint16_t is_scale : 1;         // treat speed as a floating point multiplier to edge speed
  uint16_t age : 4;              //
  uint16_t has_incident : 1;     // TODO: reserved for whether edge has an incident or not

#ifndef C_ONLY_INTERFACE
  Speed() : speed_kmh{0}, congestion_level{0}, is_scale{0}, age{0}, has_incident{0} {
  }
  Speed(const Speed& other) = default;
  Speed(const volatile Speed& other)
      : speed_kmh{other.speed_kmh}, congestion_level{other.congestion_level},
        is_scale{other.is_scale}, age{other.age}, has_incident{other.has_incident} {
  }
#endif
};

// single incident record
struct Incident {
  uint64_t edge_index : 21;
  uint64_t incident_type : 8;
  uint64_t start_location : 10;
  uint64_t length : 10;
  uint64_t spare : 15;
#ifndef C_ONLY_INTERFACE
  Incident() : edge_index{0}, incident_type{0}, start_location{0}, length{0}, spare{0} {
  }
  Incident(const Incident& other) = default;
  Incident(const volatile Incident& other)
      : edge_index{other.edge_index}, incident_type{other.incident_type},
        start_location{other.start_location}, length{other.length}, spare{0} {
  }
  Incident& operator=(const Incident& other) = default;
#endif
};

// per-speed-tile header
struct TileHeader {
  uint64_t tile_id;
  uint32_t directed_edge_count;
  uint32_t incident_buffer_size;
  uint64_t active_incident_buffer : 1;
  uint64_t last_update : 63; // seconds since epoch
};

/**
 * A tile of live traffic data.  The layout is:
 *
 * TileHeader (24 bytes)
 * n x Speed entries (n x 2 bytes)
 * IncidentsHeader (8 bytes)
 * m x Incidents entries (buffer 0) (m x 8 bytes)
 * m x Incidents entries (buffer 1) (m x 8 bytes)
 *
 * The Incidents are sparse - the TileHeader->incident_buffer_size specifies how
 * much space is available in the tile.  The IncidentsHeader fields specify where
 * incidents begin/end in this buffer.  This allows for new incident lists to be
 * placed in the buffer, then the begin/end updated to atomicly move readers to
 * a new location.
 */
#ifndef C_ONLY_INTERFACE
class Tile {
public:
  Tile(char* tile_ptr)
      : header{reinterpret_cast<TileHeader*>(tile_ptr)}, speeds{reinterpret_cast<volatile Speed*>(
                                                             tile_ptr + sizeof(TileHeader))},
        incident_count_0{reinterpret_cast<volatile uint32_t*>(
            tile_ptr + sizeof(TileHeader) +
            sizeof(Speed) * (tile_ptr == nullptr ? 0 : header->directed_edge_count))},
        incident_count_1{reinterpret_cast<volatile uint32_t*>(
            tile_ptr + sizeof(TileHeader) +
            sizeof(Speed) * (tile_ptr == nullptr ? 0 : header->directed_edge_count) +
            sizeof(*incident_count_0))},
        incidents_0{reinterpret_cast<volatile Incident*>(
            tile_ptr + sizeof(TileHeader) +
            sizeof(Speed) * (tile_ptr == nullptr ? 0 : header->directed_edge_count) +
            sizeof(*incident_count_0) * 2)},
        incidents_1{reinterpret_cast<volatile Incident*>(
            tile_ptr + sizeof(TileHeader) +
            sizeof(Speed) * (tile_ptr == nullptr ? 0 : header->directed_edge_count) +
            sizeof(*incident_count_0) * 2 +
            (tile_ptr == nullptr ? 0 : header->incident_buffer_size) * sizeof(Incident))} {
  }

  Tile(const Tile& other) = default;
  Tile& operator=(const Tile& other) = default;

  const Speed getTrafficForDirectedEdge(const uint32_t directed_edge_offset) const {
    if (header == nullptr)
      return {};
    assert(directed_edge_offset < header->directed_edge_count);
    return *(speeds + directed_edge_offset);
  }

  const std::vector<Incident> getIncidentsForDirectedEdge(const uint32_t directed_edge_offset) const {
    // Sanity check and exit early if false
    if (header == nullptr)
      return {};

    assert(directed_edge_offset < header->directed_edge_count);
    if (!(speeds + directed_edge_offset)->has_incident)
      return {};

    // Copy the current active buffer so it won't change during our usage below
    auto active_buffer = header->active_incident_buffer;

    const auto& count = active_buffer == 0 ? *incident_count_0 : *incident_count_1;
    const auto& buffer = active_buffer == 0 ? incidents_0 : incidents_1;

    // Find the beginning and end of edges that match our directed_edge_offset
    // Note that equal_range uses a binary search when given a LegacyRandomAccessIterator,
    // which raw pointers satisfy.
    struct Comp {
      bool operator()(const volatile Incident& incident, uint32_t edge_index) const {
        return incident.edge_index < edge_index;
      }
      bool operator()(uint32_t edge_index, const volatile Incident& incident) const {
        return edge_index < incident.edge_index;
      }
    };
    const auto range = std::equal_range(buffer, buffer + count, directed_edge_offset, Comp{});

    // Copy the results so that they're non-volatile for our caller
    // TODO: race condition: make sure our range is still valid
    return std::vector<Incident>(range.first, range.second);
  }

  // Returns true if this tile is valid or not
  bool operator()() const {
    return header != nullptr;
  }

  // These are all const pointers to data structures - once assigned,
  // the pointer values won't change.  The pointer targets are marked
  // as const volatile because they can be modified by code outside
  // our control (another process accessing a mmap'd file for example)
  volatile TileHeader* header;
  volatile Speed* speeds;
  volatile uint32_t* incident_count_0;
  volatile uint32_t* incident_count_1;
  volatile Incident* incidents_0;
  volatile Incident* incidents_1;
};

} // namespace traffic
} // namespace baldr
} // namespace valhalla
#endif
#endif // VALHALLA_BALDR_TRAFFICTILE_H_
