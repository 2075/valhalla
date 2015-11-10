#ifndef VALHALLA_BALDR_TRANSITDEPARTURE_H_
#define VALHALLA_BALDR_TRANSITDEPARTURE_H_

#include <valhalla/baldr/graphconstants.h>

namespace valhalla {
namespace baldr {

/**
 * Information held for each departure from a transit stop. Departures within
 * a tile are ordered by the stop Id of the departure stop, followed by the
 * time of the departure.
 */
class TransitDeparture {
 public:
  // Construct with arguments
  TransitDeparture(const uint32_t lineid, const uint32_t tripid,
                   const uint32_t routeid, const uint32_t blockid,
                   const uint32_t headsign_offset,
                   const uint32_t departure_time,
                   const uint32_t elapsed_time,
                   const uint32_t days_of_week,
                   const uint64_t days);

  /**
   * Gets the days that this departure is valid.  60 days
   * @return  Returns the days
   */
  uint64_t days() const;

  /**
   * Get the line Id - for lookup of all departures along this edge. Each
   * line Id represents a unique departure/arrival stop pair and route Id.
   * @return  Returns the departure line Id.
   */
  uint32_t lineid() const;

  /**
   * Get the internal trip Id for this departure.
   * @return  Returns the internal trip Id.
   */
  uint32_t tripid() const;

  /**
   * Get the route Id (internal) for this departure.
   * @return  Returns the internal route Id.
   */
  uint32_t routeid() const;

  /**
   * Get the block Id oof this trip.
   * @return  Returns the block Id.
   */
  uint32_t blockid() const;

  /**
   * Get the headsign offset into the names/text list.
   * @return  Returns the offset into the names/text list.
   */
  uint32_t headsign_offset() const;

  /**
   * Get the departure time.
   * @return  Returns the departure time in seconds from midnight.
   */
  uint32_t departure_time() const;

  /**
   * Get the elapsed time until arrival at the next stop.
   * @return  Returns the time in seconds.
   */
  uint32_t elapsed_time() const;

  /**
   * Gets the days of the week for this departure.
   * @return  Returns the days of the week (form TODO)
   */
  uint32_t days_of_week() const;

  /**
   * operator < - for sorting. Sort by line Id and departure time.
   * @param  other  Other transit departure to compare to.
   * @return  Returns true if line Id < other line Id or if line Ids are
   *          equal and departure < other departure.
   */
  bool operator < (const TransitDeparture& other) const;

 protected:

  // Stores bits for until the end date.  The end date will be less than or
  // equal to 60 days.
  uint64_t days_;

  // Line Id - lookup departures by unique line Id (which indicates a unique
  // departure / arrival stop pair.
  uint32_t lineid_;

  // TripId (internal).
  uint32_t tripid_;

  // Route Id (internal).
  uint32_t routeid_;

  // Block Id
  uint32_t blockid_      : 25;
  uint32_t days_of_week_ : 7;      // Days of the week

  // Headsign offset into the names/text list.
  uint32_t headsign_offset_;

  // Departure time (seconds from midnight)
  uint32_t departure_time_ : 17;   // Seconds from midnight (86400 secs per day)
  uint32_t elapsed_time_   : 15;   // Time until arrival at next stop

  // TODO - fare info, frequencies
};

}
}

#endif  // VALHALLA_BALDR_TRANSITDEPARTURE_H_
