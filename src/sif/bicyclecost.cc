#include "sif/bicyclecost.h"
#include <valhalla/baldr/directededge.h>
#include <valhalla/baldr/nodeinfo.h>
#include <valhalla/midgard/constants.h>
#include <valhalla/midgard/logging.h>

using namespace valhalla::baldr;

namespace valhalla {
namespace sif {

// Default options/values
namespace {
constexpr float kDefaultManeuverPenalty         = 10.0f;  // Seconds
constexpr float kDefaultDestinationOnlyPenalty  = 300.0f; // Seconds
constexpr float kDefaultAlleyPenalty            = 30.0f;  // Seconds
constexpr float kDefaultGateCost                = 30.0f;  // Seconds
constexpr float kDefaultGatePenalty             = 300.0f; // Seconds
constexpr float kDefaultCountryCrossingCost     = 600.0f; // Seconds
constexpr float kDefaultCountryCrossingPenalty  = 0.0f;   // Seconds

// Default turn costs.
constexpr float kTCStraight         = 0.25f;
constexpr float kTCSlight           = 0.5f;
constexpr float kTCFavorable        = 0.75f;
constexpr float kTCFavorableSharp   = 1.0f;
constexpr float kTCCrossing         = 1.5f;
constexpr float kTCUnfavorable      = 2.0f;
constexpr float kTCUnfavorableSharp = 2.5f;
constexpr float kTCReverse          = 5.0f;

// Turn costs based on side of street driving
constexpr float kRightSideTurnCosts[] = { kTCStraight, kTCSlight,
      kTCFavorable, kTCFavorableSharp, kTCReverse, kTCUnfavorableSharp,
      kTCUnfavorable, kTCSlight };
constexpr float kLeftSideTurnCosts[]  = { kTCStraight, kTCSlight,
      kTCUnfavorable, kTCUnfavorableSharp, kTCReverse, kTCFavorableSharp,
      kTCFavorable, kTCSlight };

// Cost of traversing an edge with steps. Make this high but not impassible.
// Equal to about 5 minutes (penalty) but fixed time of 30 seconds.
const Cost kBicycleStepsCost = { 300.0f, 30.0f };

// Default cycling speed on smooth, flat roads - based on bicycle type
constexpr float kDefaultCyclingSpeed[] = {
    25.0f,    // Road bicycle: ~15.5 MPH
    20.0f,    // Cross bicycle: ~13 MPH
    18.0f,    // Hybrid or "city" bicycle: ~11.5 MPH
    16.0f     // Mountain bicycle: ~10 MPH
};

// Minimum and maximum average bicycling speed (to validate input).
// Maximum is just above the fastest average speed in Tour de France time trial
constexpr float kMinCyclingSpeed = 5.0f;
constexpr float kMaxCyclingSpeed = 60.0f;

// Speed factors based on surface types (defined for each bicycle type).
// These values determine the percentage by which speed us reduced for
// each surface type. (0 values indicate unusable surface types).
constexpr float kRoadSurfaceSpeedFactors[] =
        { 1.0f, 1.0f, 0.9f, 0.6f, 0.0f, 0.0f, 0.0f, 0.0f };
constexpr float kHybridSurfaceSpeedFactors[] =
        { 1.0f, 1.0f, 1.0f, 0.8f, 0.5f, 0.0f, 0.0f, 0.0f };
constexpr float kCrossSurfaceSpeedFactors[] =
        { 1.0f, 1.0f, 1.0f, 0.8f, 0.7f, 0.5f, 0.0f, 0.0f };
constexpr float kMountainSurfaceSpeedFactors[] =
        { 1.0f, 1.0f, 1.0f, 1.0f, 0.9f, 0.8f, 0.7f, 0.0f };

// User propensity to use roads. Range of values from 0 (avoid roads - try to
// stay on cycleways and paths) to 1 (totally comfortable riding on roads).
constexpr float kDefaultUseRoadsFactor = 0.5f;

// Avoid driveways
constexpr float kDrivewayFactor = 20.0f;

// Weighting factor based on road class. These apply penalties to higher class
// roads. These penalties are modulated by the useroads factor - further
// avoiding higher class roads for those with low propensity for using roads.
constexpr float kRoadClassFactor[] = {
    0.75f,  // Motorway
    0.5f,   // Trunk
    0.35f,  // Primary
    0.25f,  // Secondary
    0.1f,   // Tertiary
    0.05f,  // Unclassified
    0.0f,   // Residential
    0.5f    // Service, other
};

// Speed adjustment factors based on weighted grade. Comments here show an
// example of speed changes based on "grade", using a base speed of 18 MPH
// on flat roads
constexpr float kGradeBasedSpeedFactor[] = {
  2.5f,      // -10%  - 45
  2.25f,     // -8%   - 40.5
  2.0f,      // -6.5% - 36
  1.7f,      // -5%   - 30.6
  1.4f,      // -3%   - 25
  1.2f,      // -1.5% - 21.6
  1.0f,      // 0%    - 18
  0.95f,     // 1.5%  - 17
  0.85f,     // 3%    - 15
  0.75f,     // 5%    - 13.5
  0.65f,     // 6.5%  - 12
  0.55f,     // 8%    - 10
  0.5f,      // 10%   - 9
  0.45f,     // 11.5% - 8
  0.4f,      // 13%   - 7
  0.3f       // 15%   - 5.5
};

// User propensity to use "hilly" roads. Ranges from a value of 0 (avoid
// hills) to 1 (take hills when they offer a more direct, less time, path).
constexpr float kDefaultUseHillsFactor = 0.5f;

// Avoid hills "strength". How much do we want to avoid a hill. Combines
// with the usehills factor (1.0 - usehills = avoidhills factor) to create
// a weighting penalty per weighted grade factor. This indicates how strongly
// edges with the specified grade are weighted. Note that speed also is
// influenced by grade, so these weights help furhte ravoid hills.
constexpr float kAvoidHillsStrength[] = {
  2.0f,      // -10%  - Treacherous descent possible
  1.0f,      // -8%   - Steep downhill
  0.5f,      // -6.5% - Good downhill - where is the bottom?
  0.3f,      // -5%   - Picking up speed!
  0.2f,      // -3%   - Modest downhill
  0.1f,      // -1.5% - Smooth slight downhill, ride this all day!
  0.0f,      // 0%    - Flat, no avoidance
  0.1f,      // 1.5%  - These are called "false flat"
  0.2f,      // 3%    - Slight rise
  0.3f,      // 5%    - Small hill
  0.5f,      // 6.5%  - Starting to feel this...
  1.0f,      // 8%    - Moderately steep
  2.5f,      // 10%   - Getting tough
  5.0f,      // 11.5% - Tiring!
  7.5f,      // 13%   - Ooof - this hurts
  10.0f      // 15%   - Only for the strongest!
};

// Edge speed above which extra penalties apply (to avoid roads with higher
// speed traffic). This threshold is adjusted upwards with higher useroads
// factors.
constexpr uint32_t kSpeedPenaltyThreshold = 40;      // 40 KPH ~ 25 MPH

// How much to favor bicycle networks.
constexpr float kBicycleNetworkFactor = 0.85f;
}

/**
 * Derived class providing dynamic edge costing for bicycle routes.
 */
class BicycleCost : public DynamicCost {
 public:
  /**
   * Constructor. Configuration / options for bicycle costing are provided
   * via a property tree.
   * @param  config  Property tree with configuration/options.
   */
  BicycleCost(const boost::property_tree::ptree& config);

  virtual ~BicycleCost();

  /**
   * Checks if access is allowed for the provided directed edge.
   * This is generally based on mode of travel and the access modes
   * allowed on the edge. However, it can be extended to exclude access
   * based on other parameters.
   * @param  edge  Pointer to a directed edge.
   * @param  pred  Predecessor edge information.
   * @return  Returns true if access is allowed, false if not.
   */
  virtual bool Allowed(const baldr::DirectedEdge* edge,
                       const EdgeLabel& pred) const;

  /**
   * Checks if access is allowed for an edge on the reverse path
   * (from destination towards origin). Both opposing edges are
   * provided.
   * @param  edge  Pointer to a directed edge.
   * @param  pred  Predecessor edge information.
   * @param  opp_edge  Pointer to the opposing directed edge.
   * @param  opp_pred_edge  Pointer to the opposing directed edge to the
   *                        predecessor.
   * @return  Returns true if access is allowed, false if not.
   */
  virtual bool AllowedReverse(const baldr::DirectedEdge* edge,
                 const EdgeLabel& pred,
                 const baldr::DirectedEdge* opp_edge,
                 const baldr::DirectedEdge* opp_pred_edge) const;

  /**
   * Checks if access is allowed for the provided node. Node access can
   * be restricted if bollards or gates are present. (TODO - others?)
   * @param  edge  Pointer to node information.
   * @return  Returns true if access is allowed, false if not.
   */
  virtual bool Allowed(const baldr::NodeInfo* node) const;

  /**
   * Get the cost to traverse the specified directed edge. Cost includes
   * the time (seconds) to traverse the edge.
   * @param   edge  Pointer to a directed edge.
   * @param   density  Relative road density.
   * @return  Returns the cost and time (seconds)
   */
  virtual Cost EdgeCost(const baldr::DirectedEdge* edge,
                        const uint32_t density) const;

  /**
   * Returns the cost to make the transition from the predecessor edge.
   * Defaults to 0. Costing models that wish to include edge transition
   * costs (i.e., intersection/turn costs) must override this method.
   * @param  edge  Directed edge (the to edge)
   * @param  node  Node (intersection) where transition occurs.
   * @param  pred  Predecessor edge information.
   * @return  Returns the cost and time (seconds)
   */
  virtual Cost TransitionCost(const baldr::DirectedEdge* edge,
                              const baldr::NodeInfo* node,
                              const EdgeLabel& pred) const;

  /**
   * Returns the cost to make the transition from the predecessor edge
   * when using a reverse search (from destination towards the origin).
   * @param  idx   Directed edge local index
   * @param  node  Node (intersection) where transition occurs.
   * @param  opp_edge  Pointer to the opposing directed edge - this is the
   *                   "from" or predecessor edge in the transition.
   * @param  opp_pred_edge  Pointer to the opposing directed edge to the
   *                        predecessor. This is the "to" edge.
   * @return  Returns the cost and time (seconds)
   */
  virtual Cost TransitionCostReverse(const uint32_t idx,
                              const baldr::NodeInfo* node,
                              const baldr::DirectedEdge* opp_edge,
                              const baldr::DirectedEdge* opp_pred_edge) const;

  /**
   * Get the cost factor for A* heuristics. This factor is multiplied
   * with the distance to the destination to produce an estimate of the
   * minimum cost to the destination. The A* heuristic must underestimate the
   * cost to the destination. So a time based estimate based on speed should
   * assume the maximum speed is used to the destination such that the time
   * estimate is less than the least possible time along roads.
   */
  virtual float AStarCostFactor() const;

 protected:
  enum class BicycleType {
    kRoad     = 0,
    kCross    = 1,    // Cyclocross bike - road bike setup with wider tires
    kHybrid   = 2,    // Hybrid or city bike
    kMountain = 3
  };

  float speedfactor_[100];          // Cost factors based on speed in kph
  float density_factor_[16];        // Density factor
  float maneuver_penalty_;          // Penalty (seconds) when inconsistent names
  float destination_only_penalty_;  // Penalty (seconds) using a driveway or parking aisle
  float gate_cost_;                 // Cost (seconds) to go through gate
  float gate_penalty_;              // Penalty (seconds) to go through gate
  float alley_penalty_;             // Penalty (seconds) to use a alley
  float country_crossing_cost_;     // Cost (seconds) to go through toll booth
  float country_crossing_penalty_;  // Penalty (seconds) to go across a country border

  // Density factor used in edge transition costing
  std::vector<float> trans_density_factor_;

  // Average speed (kph) on smooth, flat roads.
  float speed_;

  // Bicycle type
  BicycleType bicycletype_;

  // Minimal surface type usable by the bicycle type
  Surface minimal_allowed_surface_;

  // Surface speed factors
  const float* surface_speed_factor_;

  // A measure of willingness to ride with traffic. Ranges from 0-1 with
  // 0 being not willing at all and 1 being totally comfortable. This factor
  // determines how much cycle lanes and paths are preferred over roads (if
  // at all). When useroads factor is low there is more penalty to higher
  // class and higher speed roads.
  // Experienced road riders and messengers may use a value = 1 while
  // beginners may use a value of 0.1 to stay away from roads unless
  // absolutely necessary.
  float useroads_;
  float road_factor_;

  // Elevation/grade penalty (weighting applied based on the edge's weighted
  // grade (relative value from 0-15)
  float grade_penalty[16];

  // Threshold above which speed based penalties apply (based on the useroads
  // factor).
  uint32_t speed_penalty_threshold_;
  float speed_factor_;

  /**
   * Compute a turn cost based on the turn type, crossing flag,
   * and whether right or left side of road driving.
   * @param  turn_type  Turn type (see baldr/turn.h)
   * @param  crossing   Crossing another road if true.
   * @param  drive_on_right  Right hand side of road driving if true.
   */
  float TurnCost(const baldr::Turn::Type turn_type, const bool crossing,
                 const bool drive_on_right) const;

 public:
  /**
   * Returns a function/functor to be used in location searching which will
   * exclude results from the search by looking at each edges attribution
   * @return Function to be used in filtering out edges
   */
  virtual const EdgeFilter GetFilter() const {
    //throw back a lambda that checks the access for this type of costing
    BicycleType b = bicycletype_;
    return [b](const baldr::DirectedEdge* edge){
      // Prohibit certain roads based on surface type and bicycle type
      if(!edge->trans_up() && !edge->trans_down() &&
        (edge->forwardaccess() & kBicycleAccess)) {
        if (b == BicycleType::kRoad)
          return edge->surface() > Surface::kCompacted;
        else if (b == BicycleType::kHybrid)
          return edge->surface() > Surface::kDirt;
        else if (b == BicycleType::kCross)
          return edge->surface() > Surface::kGravel;
        else if (b == BicycleType::kMountain)
          return edge->surface() >= Surface::kPath;
      }
      return true;
    };
  }
};

// Bicycle route costs are distance based with some favor/avoid based on
// attribution.

// Constructor
BicycleCost::BicycleCost(const boost::property_tree::ptree& pt)
    : DynamicCost(pt, TravelMode::kBicycle),
      trans_density_factor_{ 1.0f, 1.0f, 1.0f, 1.0f,
                             1.0f, 1.0f, 1.1f, 1.1f,
                             1.2f, 1.3f, 1.4f, 1.5f,
                             1.6f, 1.7f, 1.8f, 2.0f } {
  // Transition penalties (similar to auto)
  maneuver_penalty_ = pt.get<float>("maneuver_penalty",
                                    kDefaultManeuverPenalty);
  destination_only_penalty_ = pt.get<float>("destination_only_penalty",
                                            kDefaultDestinationOnlyPenalty);
  gate_cost_ = pt.get<float>("gate_cost", kDefaultGateCost);
  gate_penalty_ = pt.get<float>("gate_penalty", kDefaultGatePenalty);
  alley_penalty_ = pt.get<float>("alley_penalty",  kDefaultAlleyPenalty);
  country_crossing_cost_ = pt.get<float>("country_crossing_cost",
                                           kDefaultCountryCrossingCost);
  country_crossing_penalty_ = pt.get<float>("country_crossing_penalty",
                                           kDefaultCountryCrossingPenalty);

  // Get the bicycle type - enter as string and convert to enum
  std::string bicycle_type = pt.get("bicycle_type", "Road");
  if (bicycle_type == "Cross") {
    bicycletype_ = BicycleType::kCross;
  } else if (bicycle_type == "Hybrid" || bicycle_type == "City") {
    bicycletype_ = BicycleType::kHybrid;
  } else if (bicycle_type == "Mountain") {
    bicycletype_ = BicycleType::kMountain;
  } else {
    bicycletype_ = BicycleType::kRoad;
  }

  // Get default speed from the config. This is the average speed on smooth,
  // flat roads. If not present or outside the valid range use a default speed
  // based on the bicycle type.
  uint32_t t = static_cast<uint32_t>(bicycletype_);
  speed_ = pt.get<float>("cycling_speed", kDefaultCyclingSpeed[t]);

  // Set the minimal surface type usable by the bicycle type and the
  // surface speed factors.
  if (bicycletype_ == BicycleType::kRoad) {
    minimal_allowed_surface_ = Surface::kCompacted;
    surface_speed_factor_ = kRoadSurfaceSpeedFactors;
  } else if (bicycletype_ == BicycleType::kHybrid) {
    minimal_allowed_surface_ =  Surface::kDirt;
    surface_speed_factor_ = kHybridSurfaceSpeedFactors;
  } else if (bicycletype_ == BicycleType::kCross) {
    minimal_allowed_surface_ =  Surface::kGravel;
    surface_speed_factor_ = kCrossSurfaceSpeedFactors;
  } else {
    // Mountain - allow all but impassable
    minimal_allowed_surface_ =  Surface::kPath;
    surface_speed_factor_ = kMountainSurfaceSpeedFactors;
  }

  // Validate speed (make sure it is in the accepted range)
  if (speed_ < kMinCyclingSpeed || speed_ > kMaxCyclingSpeed) {
    LOG_WARN("Outside valid cycling speed range " + std::to_string(speed_) +
                ": using default");
    speed_ = kDefaultCyclingSpeed[t];
  }

  // Willingness to use roads. Make sure this is within range [0, 1].
  useroads_ = pt.get<float>("use_roads", kDefaultUseRoadsFactor);
  if (useroads_ < 0.0f || useroads_ > 1.0f) {
    LOG_WARN("Outside valid useroads factor range " +
              std::to_string(useroads_) + ": using default");
  }

  // Set the road classification factor. use_roads factors above 0.5 start to
  // reduce the weight difference between road classes while factors below 0.5
  // start to increase the differences.
  road_factor_ = (useroads_ > 0.5f) ?
                 (useroads_ - 0.5f) * 0.5f : (0.5f - useroads_) * 5.0f;

  // Set the speed penalty threshold and factor. With useroads = 1 the
  // threshold is 70 kph (near 50 MPH).
  speed_penalty_threshold_ = kSpeedPenaltyThreshold +
      static_cast<uint32_t>(useroads_ * 30.0f);
  speed_factor_ = 1.1f / static_cast<float>(speed_penalty_threshold_);

  // Create speed cost table (to avoid division in costing)
  speedfactor_[0] = kSecPerHour;
  for (uint32_t s = 1; s < 100; s++) {
    speedfactor_[s] = (kSecPerHour * 0.001f) / static_cast<float>(s);
  }

  // Populate the grade penalties (based on use_hills factor).
  float usehills   = pt.get<float>("use_hills", kDefaultUseHillsFactor);
  float avoidhills = (1.0f - usehills);
  for (uint32_t i = 0; i <= kMaxGradeFactor; i++) {
    grade_penalty[i] = 1.0f + avoidhills * kAvoidHillsStrength[i];
  }
}

// Destructor
BicycleCost::~BicycleCost() {
}

// Check if access is allowed on the specified edge.
bool BicycleCost::Allowed(const baldr::DirectedEdge* edge,
                          const EdgeLabel& pred) const {
  // Check bicycle access and turn restrictions. Bicycles should obey
  // vehicular turn restrictions. Disallow Uturns. Do not allow entering
  // not-thru edges except near the destination. Skip impassable edges.
  if (!(edge->forwardaccess() & kBicycleAccess) ||
      (pred.opp_local_idx() == edge->localedgeidx()) ||
      (pred.restrictions() & (1 << edge->localedgeidx())) ||
      (edge->not_thru() && pred.distance() > not_thru_distance_)) {
    return false;
  }

  // Prohibit certain roads based on surface type and bicycle type
  return edge->surface() <= minimal_allowed_surface_;
}

// Checks if access is allowed for an edge on the reverse path (from
// destination towards origin). Both opposing edges are provided.
bool BicycleCost::AllowedReverse(const baldr::DirectedEdge* edge,
               const EdgeLabel& pred,
               const baldr::DirectedEdge* opp_edge,
               const baldr::DirectedEdge* opp_pred_edge) const {
  // Check access, U-turn, and simple turn restriction.
  // Check if edge is not-thru (no need to check distance from destination
  // since the search is heading out of any not_thru regions)
  if (!(opp_edge->forwardaccess() & kBicycleAccess) ||
       (pred.opp_local_idx() == edge->localedgeidx()) ||
       (opp_edge->restrictions() & (1 << opp_pred_edge->localedgeidx())) ||
        edge->not_thru()) {
    return false;
  }

  // Prohibit certain roads based on surface type and bicycle type
  return opp_edge->surface() <= minimal_allowed_surface_;
}

// Check if access is allowed at the specified node.
bool BicycleCost::Allowed(const baldr::NodeInfo* node) const {
  return (node->access() & kBicycleAccess);
}

// Returns the cost to traverse the edge and an estimate of the actual time
// (in seconds) to traverse the edge.
Cost BicycleCost::EdgeCost(const baldr::DirectedEdge* edge,
                           const uint32_t density) const {
  // Stairs/steps - use a high fixed cost so they are generally avoided.
  if (edge->use() == Use::kSteps) {
    return kBicycleStepsCost;
  }

  // Update speed based on surface factor. Lower speed for rougher surfaces
  // depending on the bicycle type. Modulate speed based on weighted grade
  // (relative measure of elevation change along the edge)
  float speed = speed_ *
                surface_speed_factor_[static_cast<uint32_t>(edge->surface())] *
		            kGradeBasedSpeedFactor[edge->weighted_grade()];

  // Apply a weighting factor to the cost based on desirability of cycling
  // on this edge. Based on several factors: rider propensity to ride on roads,
  // road classification and use type of road, presence of bike lanes,
  // does the road belong to a bike network, and the hilliness/elevation change
  // (based on an avoid hills factor).
  float factor = 1.0f;

  // Special use cases: cycleway and footway
  if (edge->use() == Use::kCycleway) {
    // Experienced cyclists might not favor cycleways, but most do...
    factor = (0.5f + useroads_ * 0.65f);
  } else if (edge->use() == Use::kFootway) {
    // Cyclists who favor using roads may want to avoid paths with pedestrian
    // traffic. Most cyclists would use them though.
    factor = 0.75f + (useroads_ * 0.5f);
  } else if (edge->use() == Use::kMountainBike &&
             bicycletype_ == BicycleType::kMountain) {
    factor = 0.5f;
  } else if (edge->use() == Use::kDriveway) {
    // Heavily penalize driveways
    factor = kDrivewayFactor;
  } else {
    // On a road - set a cost factor based on useroads factor and road
    // classification.
    factor += (road_factor_ *
              kRoadClassFactor[static_cast<uint32_t>(edge->classification())]);

    // Add a penalty for higher speed roads above a threshold that
    // depends on the useroads factor.
    if (edge->speed() > speed_penalty_threshold_) {
      factor *= static_cast<float>(edge->speed()) * speed_factor_;
    }

    // Favor roads where a cycle lane exists
    if (edge->cyclelane() == CycleLane::kShared) {
      factor *= 0.9f;
    } else if (edge->cyclelane() == CycleLane::kDedicated) {
      factor *= 0.8f;
    } else if (edge->cyclelane() == CycleLane::kSeparated) {
      factor *= 0.7f;
    }
  }

  // Favor bicycle networks.
  // TODO - do we need to differentiate between types of network?
  if (edge->bikenetwork() > 0) {
    factor *= kBicycleNetworkFactor;
  }

  // Update factor based on penalties applied for weighted_grade
  // TODO - do we need to modulate by length?
  factor *= grade_penalty[edge->weighted_grade()];

  // Compute elapsed time based on speed. Modulate cost with weighting factors.
  float sec = (edge->length() * speedfactor_[static_cast<uint32_t>(speed + 0.5f)]);
  return { sec * factor, sec };
}

// Returns the time (in seconds) to make the transition from the predecessor
Cost BicycleCost::TransitionCost(const baldr::DirectedEdge* edge,
                               const baldr::NodeInfo* node,
                               const EdgeLabel& pred) const {
  // Accumulate cost and penalty
  float seconds = 0.0f;
  float penalty = 0.0f;

  // Special cases with both time and penalty: country crossing,
  // gate, toll booth
  if (edge->ctry_crossing()) {
    seconds += country_crossing_cost_;
    penalty += country_crossing_penalty_;
  }
  if (node->type() == NodeType::kGate) {
    seconds += gate_cost_;
    penalty += gate_penalty_;
  }

  // Additional penalties without any time cost
  uint32_t idx = pred.opp_local_idx();
  if (!pred.destonly() && edge->destonly()) {
    penalty += destination_only_penalty_;
  }
  if (pred.use() != Use::kAlley && edge->use() == Use::kAlley) {
    penalty += alley_penalty_;
  }
  if (!node->name_consistency(idx, edge->localedgeidx())) {
    penalty += maneuver_penalty_;
  }

  // Transition time = densityfactor * stopimpact * turncost
  if (edge->stopimpact(idx) > 0) {
    float turn_cost;
    if (edge->edge_to_right(idx) && edge->edge_to_left(idx)) {
      turn_cost = kTCCrossing;
    } else {
      turn_cost = (edge->drive_on_right()) ?
          kRightSideTurnCosts[static_cast<uint32_t>(edge->turntype(idx))] :
          kLeftSideTurnCosts[static_cast<uint32_t>(edge->turntype(idx))];
    }
    seconds += trans_density_factor_[node->density()] *
               edge->stopimpact(idx) * turn_cost;
  }

  // Return cost (time and penalty)
  return { seconds + penalty, seconds };
}

// Returns the cost to make the transition from the predecessor edge
// when using a reverse search (from destination towards the origin).
// pred is the opposing current edge in the reverse tree
// edge is the opposing predecessor in the reverse tree
Cost BicycleCost::TransitionCostReverse(const uint32_t idx,
                            const baldr::NodeInfo* node,
                            const baldr::DirectedEdge* pred,
                            const baldr::DirectedEdge* edge) const {
  // Accumulate cost and penalty
  float seconds = 0.0f;
  float penalty = 0.0f;

  // Special cases with both time and penalty: country crossing,
  // gate, toll booth
  if (edge->ctry_crossing()) {
    seconds += country_crossing_cost_;
    penalty += country_crossing_penalty_;
  }
  if (node->type() == NodeType::kGate) {
    seconds += gate_cost_;
    penalty += gate_penalty_;
  }

  // Additional penalties without any time cost
  if (!pred->destonly() && edge->destonly()) {
    penalty += destination_only_penalty_;
  }
  if (pred->use() != Use::kAlley && edge->use() == Use::kAlley) {
    penalty += alley_penalty_;
  }
  if (!node->name_consistency(idx, edge->localedgeidx())) {
    penalty += maneuver_penalty_;
  }

  // Transition time = densityfactor * stopimpact * turncost
  if (edge->stopimpact(idx) > 0) {
    float turn_cost;
    if (edge->edge_to_right(idx) && edge->edge_to_left(idx)) {
      turn_cost = kTCCrossing;
    } else {
      turn_cost = (edge->drive_on_right()) ?
          kRightSideTurnCosts[static_cast<uint32_t>(edge->turntype(idx))] :
          kLeftSideTurnCosts[static_cast<uint32_t>(edge->turntype(idx))];
    }
    seconds += trans_density_factor_[node->density()] *
              edge->stopimpact(idx) * turn_cost;
  }

  // Return cost (time and penalty)
  return { seconds + penalty, seconds };
}

/**
 * Get the cost factor for A* heuristics. This factor is multiplied
 * with the distance to the destination to produce an estimate of the
 * minimum cost to the destination. The A* heuristic must underestimate the
 * cost to the destination. So a time based estimate based on speed should
 * assume the maximum speed is used to the destination such that the time
 * estimate is less than the least possible time along roads.
 */
float BicycleCost::AStarCostFactor() const {
  // Assume max speed of 80 kph (50 MPH)
  return speedfactor_[80];
}

cost_ptr_t CreateBicycleCost(const boost::property_tree::ptree& config) {
  return std::make_shared<BicycleCost>(config);
}

}
}
