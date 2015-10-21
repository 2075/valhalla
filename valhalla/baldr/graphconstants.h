#ifndef VALHALLA_BALDR_GRAPHCONSTANTS_H_
#define VALHALLA_BALDR_GRAPHCONSTANTS_H_

#include <unordered_map>
#include <string>

namespace valhalla {
namespace baldr {

// Access constants. Bit constants.
constexpr uint8_t kAutoAccess       = 1;
constexpr uint8_t kPedestrianAccess = 2;
constexpr uint8_t kBicycleAccess    = 4;
constexpr uint8_t kTruckAccess      = 8;
constexpr uint8_t kEmergencyAccess  = 16;
constexpr uint8_t kTaxiAccess       = 32;
constexpr uint8_t kBusAccess        = 64;
constexpr uint8_t kHOVAccess        = 128;
constexpr uint8_t kAllAccess        = 255;

// Payment constants. Bit constants.
constexpr uint8_t kCoins  = 1; // Coins
constexpr uint8_t kNotes  = 2; // Bills
constexpr uint8_t kETC    = 4; // Electronic Toll Collector

// Edge traversability
enum class Traversability {
  kNone = 0,        // Edge is not traversable in either direction
  kForward = 1,     // Edge is traversable in the forward direction
  kBackward = 2,    // Edge is traversable in the backward direction
  kBoth = 3         // Edge is traversable in both directions
};

// Maximum speed. This impacts the effectiveness of A* for driving routes
// so it should be set as low as is reasonable. Speeds above this in OSM are
// clamped to this maximum value.
constexpr uint32_t kMaxSpeedKph = 140;      // ~85 MPH

// Road class or importance of an edge
enum class RoadClass : uint8_t {
  kMotorway = 0,
  kTrunk = 1,
  kPrimary = 2,
  kSecondary = 3,
  kTertiary = 4,
  kUnclassified = 5,
  kResidential = 6,
  kServiceOther = 7
};
const std::unordered_map<std::string, RoadClass> stringToRoadClass =
  { {"Motorway", RoadClass::kMotorway}, {"Trunk", RoadClass::kTrunk}, {"Primary", RoadClass::kPrimary},
    {"Secondary", RoadClass::kSecondary}, {"Tertiary", RoadClass::kTertiary},
    {"Unclassified", RoadClass::kUnclassified},{"Residential", RoadClass::kResidential},
    {"ServiceOther", RoadClass::kServiceOther}
  };
const std::unordered_map<uint8_t, std::string> RoadClassStrings = {
  {static_cast<uint8_t>(RoadClass::kMotorway), "motorway"},
  {static_cast<uint8_t>(RoadClass::kTrunk), "trunk"},
  {static_cast<uint8_t>(RoadClass::kPrimary), "primary"},
  {static_cast<uint8_t>(RoadClass::kSecondary), "secondary"},
  {static_cast<uint8_t>(RoadClass::kTertiary), "tertiary"},
  {static_cast<uint8_t>(RoadClass::kUnclassified), "unclassified"},
  {static_cast<uint8_t>(RoadClass::kResidential), "residential"},
  {static_cast<uint8_t>(RoadClass::kServiceOther), "service_other"},
};
inline std::string to_string(RoadClass r) {
  auto i = RoadClassStrings.find(static_cast<uint8_t>(r));
  if(i == RoadClassStrings.cend())
    return "null";
  return i->second;
}

// Maximum length in meters of an internal intersection edge
constexpr float kMaxInternalLength = 32.0f;

// Maximum length in meters of a "link" that can be assigned use=kTurnChannel
// (vs. kRamp)
constexpr float kMaxTurnChannelLength = 200.0f;

// Bicycle Network constants. Bit constants.
constexpr uint8_t kNcn = 1;   // Part of national bicycle network
constexpr uint8_t kRcn = 2;   // Part of regional bicycle network
constexpr uint8_t kLcn = 4;   // Part of local bicycle network
constexpr uint8_t kMcn = 8;   // Part of mountain bicycle network
constexpr uint8_t kMaxBicycleNetwork = 15;

// Maximum offset to edge information
constexpr uint32_t kMaxEdgeInfoOffset = 33554431;   // 2^25 bytes

// Maximum length of an edge
constexpr uint32_t kMaxEdgeLength = 16777215;   // 2^24 meters

// Maximum number of edges allowed in a turn restriction mask
constexpr uint32_t kMaxTurnRestrictionEdges = 8;

// Maximum speed (kph)
constexpr float kMaxSpeed = 255.0f;

// Maximum lane count
constexpr uint32_t kMaxLaneCount = 15;

// Number of edges considered for edge transitions
constexpr uint32_t kNumberOfEdgeTransitions = 8;

// Maximum shortcuts edges from a node. More than this can be
// added but this is the max. that can supersede an edge
constexpr uint32_t kMaxShortcutsFromNode = 7;

// Maximum stop impact
constexpr uint32_t kMaxStopImpact = 7;

// Maximum grade and curvature factors.
constexpr uint32_t kMaxGradeFactor = 15;
constexpr uint32_t kMaxCurvatureFactor = 15;

// Node types.
enum class NodeType : uint8_t {
  kStreetIntersection = 0,  // Regular intersection of 2 roads
  kGate = 1,                // Gate or rising bollard
  kBollard = 2,             // Bollard (fixed obstruction)
  kTollBooth = 3,           // Toll booth / fare collection
  kRailStop = 4,            // Rail/metro/subway stop
  kBusStop = 5,             // Bus stop
  kMultiUseTransitStop = 6, // Multi-use transit stop (rail and bus)
  kBikeShare = 7,           // Bike share location
  kParking = 8,             // Parking location
  kMotorWayJunction = 9     // Highway = motorway_junction
};
const std::unordered_map<uint8_t, std::string> NodeTypeStrings = {
  {static_cast<uint8_t>(NodeType::kStreetIntersection), "street_intersection"},
  {static_cast<uint8_t>(NodeType::kGate), "gate"},
  {static_cast<uint8_t>(NodeType::kBollard), "bollard"},
  {static_cast<uint8_t>(NodeType::kTollBooth), "toll_booth"},
  {static_cast<uint8_t>(NodeType::kRailStop), "rail_stop"},
  {static_cast<uint8_t>(NodeType::kBusStop), "bus_stop"},
  {static_cast<uint8_t>(NodeType::kMultiUseTransitStop), "multi_use_transit_stop"},
  {static_cast<uint8_t>(NodeType::kBikeShare), "bike_share"},
  {static_cast<uint8_t>(NodeType::kParking), "parking"},
  {static_cast<uint8_t>(NodeType::kMotorWayJunction), "motor_way_junction"},
};
inline std::string to_string(NodeType n) {
  auto i = NodeTypeStrings.find(static_cast<uint8_t>(n));
  if(i == NodeTypeStrings.cend())
    return "null";
  return i->second;
}

// Intersection types. Classifications of various intersections.
// Maximum value = 31 (DO NOT EXCEED!)
enum class IntersectionType : uint8_t {
  kRegular = 0,       // Regular, unclassified intersection
  kFalse = 1,         // False intersection. Only 2 edges connect. Typically
                      // where 2 ways are split or where attributes force a split.
  kDeadEnd = 2,       // Node only connects to one edge ("dead-end").
  kFork = 3           // All edges are links OR all edges are not links
                      // and node is a motorway_junction.
};
const std::unordered_map<uint8_t, std::string> IntersectionTypeStrings = {
  {static_cast<uint8_t>(IntersectionType::kRegular), "road"},
  {static_cast<uint8_t>(IntersectionType::kFalse), "ramp"},
  {static_cast<uint8_t>(IntersectionType::kDeadEnd), "turn_channel"},
  {static_cast<uint8_t>(IntersectionType::kFork), "track"},
};
inline std::string to_string(IntersectionType x) {
  auto i = IntersectionTypeStrings.find(static_cast<uint8_t>(x));
  if(i == IntersectionTypeStrings.cend())
    return "null";
  return i->second;
}

// Edge use. Indicates specialized uses.
// Maximum value that can be stored for a directed edge is 63 - DO NOT EXCEED!
enum class Use : uint8_t {
  // Road specific uses
  kRoad = 0,
  kRamp = 1,              // Link - exits/entrance ramps.
  kTurnChannel = 2,       // Link - turn lane.
  kTrack = 3,             // Agricultural use, forest tracks
  kDriveway = 4,          // Driveway/private service
  kAlley = 5,             // Service road - limited route use
  kParkingAisle = 6,      // Access roads in parking areas
  kEmergencyAccess = 7,   // Emergency vehicles only
  kDriveThru = 8,         // Commercial drive-thru (banks/fast-food)
  kCuldesac = 9,          // Cul-de-sac (edge that forms a loop and is only
                          // connected at one node to another edge.

  // Bicycle specific uses
  kCycleway = 20,          // Dedicated bicycle path
  kMountainBike = 21,      // Mountain bike trail

  // Pedestrian specific uses
  kFootway = 25,
  kSteps   = 26,           // Stairs

  // Other...
  kOther = 40,

  // Transit specific uses. Must be last in the list
  kRail = 50,              // Rail line
  kBus = 51,               // Bus line
  kRailConnection = 52,    // Connection to a rail stop
  kBusConnection = 53,     // Connection to a bus stop
  kTransitConnection = 54  // Connection to multi-use transit stop
};
const std::unordered_map<uint8_t, std::string> UseStrings = {
  {static_cast<uint8_t>(Use::kRoad), "road"},
  {static_cast<uint8_t>(Use::kRamp), "ramp"},
  {static_cast<uint8_t>(Use::kTurnChannel), "turn_channel"},
  {static_cast<uint8_t>(Use::kTrack), "track"},
  {static_cast<uint8_t>(Use::kDriveway), "driveway"},
  {static_cast<uint8_t>(Use::kAlley), "alley"},
  {static_cast<uint8_t>(Use::kParkingAisle), "parking_aisle"},
  {static_cast<uint8_t>(Use::kEmergencyAccess), "emergency_access"},
  {static_cast<uint8_t>(Use::kDriveThru), "drive_through"},
  {static_cast<uint8_t>(Use::kCuldesac), "culdesac"},
  {static_cast<uint8_t>(Use::kCycleway), "cycleway"},
  {static_cast<uint8_t>(Use::kMountainBike), "mountain_bike"},
  {static_cast<uint8_t>(Use::kFootway), "footway"},
  {static_cast<uint8_t>(Use::kSteps), "steps"},
  {static_cast<uint8_t>(Use::kOther), "other"},
  {static_cast<uint8_t>(Use::kRail), "rail"},
  {static_cast<uint8_t>(Use::kBus), "bus"},
  {static_cast<uint8_t>(Use::kRailConnection), "rail_connection"},
  {static_cast<uint8_t>(Use::kBusConnection), "bus_connnection"},
  {static_cast<uint8_t>(Use::kTransitConnection), "transit_connection"},
};
inline std::string to_string(Use u) {
  auto i = UseStrings.find(static_cast<uint8_t>(u));
  if(i == UseStrings.cend())
    return "null";
  return i->second;
}

// Speed type
enum class SpeedType : uint8_t {
  kTagged = 0,            // Tagged maximum speed
  kClassified = 1,        // Speed assigned based on highway classification
  kClassifiedUrban = 2,   // Classified speed in urban area
  kClassifiedRural = 3    // Classified speed in rural area
};
const std::unordered_map<uint8_t, std::string> SpeedTypeStrings = {
  {static_cast<uint8_t>(SpeedType::kTagged), "tagged"},
  {static_cast<uint8_t>(SpeedType::kClassified), "classified"},
  {static_cast<uint8_t>(SpeedType::kClassifiedUrban), "classified_urban"},
  {static_cast<uint8_t>(SpeedType::kClassifiedRural), "classified_rural"},
};
inline std::string to_string(SpeedType s) {
  auto i = SpeedTypeStrings.find(static_cast<uint8_t>(s));
  if(i == SpeedTypeStrings.cend())
    return "null";
  return i->second;
}

// Indication of the type of cycle lane (if any) present along an edge.
// Higher values are more favorable to safe bicycling.
enum class CycleLane : uint8_t {
  kNone = 0,      // No specified bicycle lane
  kShared = 1,    // Shared use lane (could be shared with pedestrians)
  kDedicated = 2, // Dedicated cycle lane
  kSeparated = 3  // A separate cycle lane (physical separation from the
                  // main carriageway)
};
const std::unordered_map<uint8_t, std::string> CycleLaneStrings = {
  {static_cast<uint8_t>(CycleLane::kNone), "none"},
  {static_cast<uint8_t>(CycleLane::kShared), "shared"},
  {static_cast<uint8_t>(CycleLane::kDedicated), "dedicated"},
  {static_cast<uint8_t>(CycleLane::kSeparated), "separated"},
};
inline std::string to_string(CycleLane c) {
  auto i = CycleLaneStrings.find(static_cast<uint8_t>(c));
  if(i == CycleLaneStrings.cend())
    return "null";
  return i->second;
}

// Generalized representation of surface types. Lower values indicate smoother
// surfaces. Vehicle or bicycle type can use this to avoid or disallow edges
// that are "too rough" or inappropriate for the vehicle to travel on.
enum class Surface : uint8_t {
  kPavedSmooth = 0,
  kPaved = 1,
  kPavedRough = 2,
  kCompacted = 3,
  kDirt = 4,
  kGravel = 5,
  kPath = 6,
  kImpassable = 7
};
const std::unordered_map<uint8_t, std::string> SurfaceStrings = {
  {static_cast<uint8_t>(Surface::kPavedSmooth), "paved_smooth"},
  {static_cast<uint8_t>(Surface::kPaved), "paved"},
  {static_cast<uint8_t>(Surface::kPavedRough), "paved_rough"},
  {static_cast<uint8_t>(Surface::kCompacted), "compacted"},
  {static_cast<uint8_t>(Surface::kDirt), "dirt"},
  {static_cast<uint8_t>(Surface::kGravel), "gravel"},
  {static_cast<uint8_t>(Surface::kPath), "path"},
  {static_cast<uint8_t>(Surface::kImpassable), "impassable"},
};
inline std::string to_string(Surface s) {
  auto i = SurfaceStrings.find(static_cast<uint8_t>(s));
  if(i == SurfaceStrings.cend())
    return "null";
  return i->second;
}

// Used for restrictions.  A restriction starts and ends on a particular day
enum class DOW : uint8_t {
  kNone = 0,
  kSunday = 1,
  kMonday = 2,
  kTuesday = 3,
  kWednesday = 4,
  kThursday = 5,
  kFriday = 6,
  kSaturday = 7
};

//This is our pivot date for transit.  No dates will be older than this date.
const std::string kPivotDate = "20140101";  //January 1, 2014

// Used for transit DOW mask.
constexpr uint8_t kDOWNone    = 0;
constexpr uint8_t kSunday     = 1;
constexpr uint8_t kMonday     = 2;
constexpr uint8_t kTuesday    = 4;
constexpr uint8_t kWednesday  = 8;
constexpr uint8_t kThursday   = 16;
constexpr uint8_t kFriday     = 32;
constexpr uint8_t kSaturday   = 64;

// Restriction types. If a restriction exists this value will be set.
// Restrictions with "Only" will restrict all turns not adhering to the
// only "to" road of the restriction.
enum class RestrictionType : uint8_t {
  kNoLeftTurn = 0,
  kNoRightTurn = 1,
  kNoStraightOn = 2,
  kNoUTurn = 3,
  kOnlyRightTurn = 4,
  kOnlyLeftTurn = 5,
  kOnlyStraightOn = 6
};

// Access Restriction types.
enum class AccessType : uint8_t {
  kHazmat = 0,
  kMaxHeight = 1,
  kMaxWidth= 2,
  kMaxLength = 3,
  kMaxWeight = 4,
  kMaxAxleLoad = 5
};

// ------------------------------- Transit information --------------------- //

constexpr uint32_t kOneStopIdSize = 256;

// Transit transfer types
enum class TransferType : uint8_t {
  kRecommended = 0,   // Recommended transfer point between 2 routes
  kTimed       = 1,   // Timed transfer between 2 routes. Departing vehicle
                      // is expected to wait, allowing sufficient time for
                      // passengers to transfer.
  kMinTime     = 2,   // Transfer is expected to take the time specified.
  kNotPossible = 3    // Transfers not possible between routes
};

enum class CalendarExceptionType : uint8_t {
  kAdded       = 1,   // Service added for the specified date
  kRemoved     = 2    // Service removed for the specified date
};

}
}

#endif  // VALHALLA_BALDR_GRAPHCONSTANTS_H_
