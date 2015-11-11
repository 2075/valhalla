#include "baldr/graphtile.h"
#include "baldr/datetime.h"
#include <valhalla/midgard/tiles.h>
#include <valhalla/midgard/aabb2.h>
#include <valhalla/midgard/pointll.h>
#include <valhalla/midgard/logging.h>

#include <ctime>
#include <string>
#include <vector>
#include <iostream>
#include <fstream>
#include <locale>
#include <iomanip>
#include <cmath>
#include <boost/algorithm/string.hpp>

namespace {
  struct dir_facet : public std::numpunct<char> {
   protected:
    virtual char do_thousands_sep() const {
        return '/';
    }

    virtual std::string do_grouping() const {
        return "\03";
    }
  };
  template <class numeric_t>
  size_t digits(numeric_t number) {
    size_t digits = (number < 0 ? 1 : 0);
    while (static_cast<long long int>(number)) {
        number /= 10;
        digits++;
    }
    return digits;
  }
  const std::locale dir_locale(std::locale("C"), new dir_facet());
  const AABB2<PointLL> world_box(PointLL(-180, -90), PointLL(180, 90));
}

namespace valhalla {
namespace baldr {

// Default constructor
GraphTile::GraphTile()
    : size_(0),
      header_(nullptr),
      nodes_(nullptr),
      directededges_(nullptr),
      departures_(nullptr),
      transit_stops_(nullptr),
      transit_routes_(nullptr),
      transit_transfers_(nullptr),
      access_restrictions_(nullptr),
      signs_(nullptr),
      admins_(nullptr),
      edge_cells_(nullptr),
      edgeinfo_(nullptr),
      textlist_(nullptr),
      edgeinfo_size_(0),
      textlist_size_(0){
}

// Constructor given a filename. Reads the graph data into memory.
GraphTile::GraphTile(const TileHierarchy& hierarchy, const GraphId& graphid)
    : size_(0) {

  // Don't bother with invalid ids
  if (!graphid.Is_Valid())
    return;

  // Open to the end of the file so we can immediately get size;
  std::string file_location = hierarchy.tile_dir() + "/" +
                FileSuffix(graphid.Tile_Base(), hierarchy);
  std::ifstream file(file_location, std::ios::in | std::ios::binary | std::ios::ate);
  if (file.is_open()) {
    // Read binary file into memory. TODO - protect against failure to
    // allocate memory
    size_t filesize = file.tellg();
    graphtile_.reset(new char[filesize]);
    file.seekg(0, std::ios::beg);
    file.read(graphtile_.get(), filesize);
    file.close();

    // Set a pointer to the header (first structure in the binary data).
    char* ptr = graphtile_.get();
    header_ = reinterpret_cast<GraphTileHeader*>(ptr);
    ptr += sizeof(GraphTileHeader);

    // TODO check version

    // Set a pointer to the node list
    nodes_ = reinterpret_cast<NodeInfo*>(ptr);
    ptr += header_->nodecount() * sizeof(NodeInfo);

    // Set a pointer to the directed edge list
    directededges_ = reinterpret_cast<DirectedEdge*>(ptr);
    ptr += header_->directededgecount() * sizeof(DirectedEdge);

    // Set a pointer to the transit departure list
    departures_ = reinterpret_cast<TransitDeparture*>(ptr);
    ptr += header_->departurecount() * sizeof(TransitDeparture);

    // Set a pointer to the transit stop list
    transit_stops_ = reinterpret_cast<TransitStop*>(ptr);
    ptr += header_->stopcount() * sizeof(TransitStop);

    // Set a pointer to the transit route list
    transit_routes_ = reinterpret_cast<TransitRoute*>(ptr);
    ptr += header_->routecount() * sizeof(TransitRoute);

    // Set a pointer to the transit transfer list
    transit_transfers_ = reinterpret_cast<TransitTransfer*>(ptr);
    ptr += header_->transfercount() * sizeof(TransitTransfer);

    // Set a pointer access restriction list
    access_restrictions_ = reinterpret_cast<AccessRestriction*>(ptr);
    ptr += header_->access_restriction_count() * sizeof(AccessRestriction);

/*
LOG_INFO("Tile: " + std::to_string(graphid.tileid()) + "," + std::to_string(graphid.level()));
LOG_INFO("Departures: " + std::to_string(header_->departurecount()) +
         " Stops: " + std::to_string(header_->stopcount()) +
         " Routes: " + std::to_string(header_->routecount()) +
         " Transfers: " + std::to_string(header_->transfercount()) +
         " Exceptions: " + std::to_string(header_->calendarcount()));
    */

    // Set a pointer to the sign list
    signs_ = reinterpret_cast<Sign*>(ptr);
    ptr += header_->signcount() * sizeof(Sign);

    // Set a pointer to the admininstrative information list
    admins_ = reinterpret_cast<Admin*>(ptr);
    ptr += header_->admincount() * sizeof(Admin);

    // Set a pointer to the edge cell list
    edge_cells_ = reinterpret_cast<GraphId*>(ptr);
    ptr += header_->cell_offset(kGridDim - 1, kGridDim - 1).second * sizeof(GraphId);

    // Start of edge information and its size
    edgeinfo_ = graphtile_.get() + header_->edgeinfo_offset();
    edgeinfo_size_ = header_->textlist_offset() - header_->edgeinfo_offset();

    // Start of text list and its size
    textlist_ = graphtile_.get() + header_->textlist_offset();
    textlist_size_ = filesize - header_->textlist_offset();

    // Set the size to indicate success
    size_ = filesize;
  }
  else {
    LOG_DEBUG("Tile " + file_location + " was not found");
  }
}

GraphTile::~GraphTile() {
}

std::string GraphTile::FileSuffix(const GraphId& graphid, const TileHierarchy& hierarchy) {
  /*
  if you have a graphid where level == 8 and tileid == 24134109851
  you should get: 8/024/134/109/851.gph
  since the number of levels is likely to be very small this limits
  the total number of objects in any one directory to 1000, which is an
  empirically derived good choice for mechanical harddrives
  this should be fine for s3 (even though it breaks the rule of most
  unique part of filename first) because there will be just so few
  objects in general in practice
  */

  //figure the largest id for this level
  const auto level = hierarchy.levels().find(graphid.level());
  if(level == hierarchy.levels().end())
    throw std::runtime_error("Could not compute FileSuffix for non-existent level");
  const uint32_t max_id = Tiles<PointLL>::MaxTileId(world_box, level->second.tiles.TileSize());

  //figure out how many digits
  //TODO: dont convert it to a string to get the length there are faster ways..
  size_t max_length = digits<uint32_t>(max_id);
  const size_t remainder = max_length % 3;
  if(remainder)
    max_length += 3 - remainder;

  //make a locale to use as a formatter for numbers
  std::ostringstream stream;
  stream.imbue(dir_locale);

  //if it starts with a zero the pow trick doesn't work
  if(graphid.level() == 0) {
    stream << static_cast<uint32_t>(std::pow(10, max_length)) + graphid.tileid() << ".gph";
    std::string suffix = stream.str();
    suffix[0] = '0';
    return suffix;
  }
  //it was something else
  stream << graphid.level() * static_cast<uint32_t>(std::pow(10, max_length)) + graphid.tileid() << ".gph";
  return stream.str();
}

// Get the tile Id given the full path to the file.
GraphId GraphTile::GetTileId(const std::string& fname, const TileHierarchy& hierarchy) {
  //strip off the unuseful part
  auto pos = fname.find(hierarchy.tile_dir());
  if(pos == std::string::npos)
    throw std::runtime_error("File name for tile does not match hierarchy root dir");
  auto name = fname.substr(pos + hierarchy.tile_dir().size());
  boost::algorithm::trim_if(name, boost::is_any_of("/.gph"));

  //split on slash
  std::vector<std::string> tokens;
  boost::split(tokens, name, boost::is_any_of("/"));

  //need at least level and id
  if(tokens.size() < 2)
    throw std::runtime_error("Invalid tile path");

  // Compute the Id
  uint32_t id = 0;
  uint32_t multiplier = std::pow(1000, tokens.size() - 2);
  bool first = true;
  for(const auto& token : tokens) {
    if(first) {
      first = false;
      continue;
    }
    id += std::atoi(token.c_str()) * multiplier;
    multiplier /= 1000;
  }
  uint32_t level = std::atoi(tokens.front().c_str());
  return {id, level, 0};
}

// Get the bounding box of this graph tile.
AABB2<PointLL> GraphTile::BoundingBox(const TileHierarchy& hierarchy) const {
  auto tiles = hierarchy.levels().find(header_->graphid().level())->second.tiles;
  return tiles.TileBounds(header_->graphid().tileid());
}

size_t GraphTile::size() const {
  return size_;
}

GraphId GraphTile::id() const {
  return header_->graphid();
}

const GraphTileHeader* GraphTile::header() const {
  return header_;
}

const NodeInfo* GraphTile::node(const GraphId& node) const {
  if (node.id() < header_->nodecount())
    return &nodes_[node.id()];
  throw std::runtime_error("GraphTile NodeInfo index out of bounds: " +
                             std::to_string(node.tileid()) + "," +
                             std::to_string(node.level()) + "," +
                             std::to_string(node.id()) + " nodecount= " +
                             std::to_string(header_->nodecount()));
}

const NodeInfo* GraphTile::node(const size_t idx) const {
  if (idx < header_->nodecount())
    return &nodes_[idx];
  throw std::runtime_error("GraphTile NodeInfo index out of bounds: " +
                           std::to_string(header_->graphid().tileid()) + "," +
                           std::to_string(header_->graphid().level()) + "," +
                           std::to_string(idx)  + " nodecount= " +
                           std::to_string(header_->nodecount()));
}

// Get the directed edge given a GraphId
const DirectedEdge* GraphTile::directededge(const GraphId& edge) const {
  if (edge.id() < header_->directededgecount())
    return &directededges_[edge.id()];
  throw std::runtime_error("GraphTile DirectedEdge index out of bounds: " +
                           std::to_string(header_->graphid().tileid()) + "," +
                           std::to_string(header_->graphid().level()) + "," +
                           std::to_string(edge.id())  + " directededgecount= " +
                           std::to_string(header_->directededgecount()));
  throw std::runtime_error("GraphTile DirectedEdge id out of bounds");
}

// Get the directed edge at the specified index.
const DirectedEdge* GraphTile::directededge(const size_t idx) const {
  if (idx < header_->directededgecount())
    return &directededges_[idx];
  throw std::runtime_error("GraphTile DirectedEdge index out of bounds: " +
                           std::to_string(header_->graphid().tileid()) + "," +
                           std::to_string(header_->graphid().level()) + "," +
                           std::to_string(idx)  + " directededgecount= " +
                           std::to_string(header_->directededgecount()));
}

std::unique_ptr<const EdgeInfo> GraphTile::edgeinfo(const size_t offset) const {
  return std::unique_ptr<EdgeInfo>(new EdgeInfo(edgeinfo_ + offset, textlist_, textlist_size_));
}

// Get the directed edges outbound from the specified node index.
const DirectedEdge* GraphTile::GetDirectedEdges(const uint32_t node_index,
                                                uint32_t& count,
                                                uint32_t& edge_index) const {
  const NodeInfo* nodeinfo = node(node_index);
  count = nodeinfo->edge_count();
  edge_index = nodeinfo->edge_index();
  return directededge(nodeinfo->edge_index());
}

// Convenience method to get the names for an edge given the offset to the
// edge info
std::vector<std::string> GraphTile::GetNames(const uint32_t edgeinfo_offset) const {
  return edgeinfo(edgeinfo_offset)->GetNames();
}

// Get the admininfo at the specified index.
AdminInfo GraphTile::admininfo(const size_t idx) const {
  if (idx < header_->admincount()) {
    const Admin& admin = admins_[idx];
    return AdminInfo(textlist_ + admin.country_offset(),
                     textlist_ + admin.state_offset(),
                     admin.country_iso(), admin.state_iso());
  }
  throw std::runtime_error("GraphTile AdminInfo index out of bounds");
}

// Get the admin at the specified index.
const Admin* GraphTile::admin(const size_t idx) const {
  if (idx < header_->admincount()) {
    return &admins_[idx];
  }
  throw std::runtime_error("GraphTile Admin index out of bounds");
}

// Convenience method to get the text/name for a given offset to the textlist
std::string GraphTile::GetName(const uint32_t textlist_offset) const {

  if (textlist_offset < textlist_size_) {
    return textlist_ + textlist_offset;
  } else {
    throw std::runtime_error("GetName: offset exceeds size of text list");
  }
}

// Convenience method to get the signs for an edge given the
// directed edge index.
std::vector<SignInfo> GraphTile::GetSigns(const uint32_t idx) const {
  uint32_t count = header_->signcount();
  std::vector<SignInfo> signs;
  if (count == 0) {
    return signs;
  }

  // Binary search
  int32_t low = 0;
  int32_t high = count-1;
  int32_t mid;
  bool found = false;
  while (low <= high) {
    mid = (low + high) / 2;
    if (signs_[mid].edgeindex() == idx) {
      found = true;
      break;
    }
    if (idx < signs_[mid].edgeindex() ) {
      high = mid - 1;
    } else {
      low = mid + 1;
    }
  }

  if (found) {
    // Back up while prior is equal (or at the beginning)
    while (mid > 0 && signs_[mid-1].edgeindex() == idx) {
      mid--;
    }

    // Add signs
    while (signs_[mid].edgeindex() == idx && mid < count) {
      if (signs_[mid].text_offset() < textlist_size_) {
        signs.emplace_back(signs_[mid].type(),
                (textlist_ + signs_[mid].text_offset()));
      } else {
        throw std::runtime_error("GetSigns: offset exceeds size of text list");
      }
      mid++;
    }
  }
  if (signs.size() == 0) {
    LOG_ERROR("No signs found for idx = " + std::to_string(idx));
  }
  return signs;
}

// Get the next departure given the directed edge Id and the current
// time (seconds from midnight).
const TransitDeparture* GraphTile::GetNextDeparture(const uint32_t lineid,
                 const uint32_t current_time, const uint32_t date,
                 const uint32_t dow) const {
  uint32_t count = header_->departurecount();
  if (count == 0) {
    return nullptr;
  }

  // Departures are sorted by edge Id and then by departure time.
  // Binary search to find a departure with matching edge Id.
  int32_t low = 0;
  int32_t high = count-1;
  int32_t mid;
  bool found = false;
  while (low <= high) {
    mid = (low + high) / 2;
    if (departures_[mid].lineid() == lineid) {
      found = true;
      break;
    }
    if (lineid < departures_[mid].lineid() ) {
      high = mid - 1;
    } else {
      low = mid + 1;
    }
  }

  if (!found) {
    LOG_DEBUG("No departures found for lineid = " + std::to_string(lineid));
    return nullptr;
  }

  // Back up until the prior departure from this edge has departure time
  // less than the current time
  while (mid > 0 &&
         departures_[mid-1].lineid() == lineid &&
         departures_[mid-1].departure_time() >= current_time) {
    mid--;
  }

  // Iterate through departures until one is found with valid date, dow or
  // calendar date, and does not have a calendar exception.
  uint32_t valid_date_plus_60 = header_->date_created() + 60;
  while (true) {
    // Make sure valid departure time
    if (departures_[mid].departure_time() >= current_time) {
      // If within 60 days of tile creation use the days mask else fallback
      // to the day of week mask
      const TransitDeparture& dep = departures_[mid];
      if (date <= valid_date_plus_60) {
       ; //if (DateTime::is_service_available(dep.days(), dep.start_date(),
         //                                  date, dep.end_date()))
        //  return &departures_[mid];
      } else {
        if ((dep.days_of_week() & dow) > 0)
          return &departures_[mid];
      }
    }

    // Advance to next departure
    if (mid+1 < count && departures_[mid+1].lineid() == lineid) {
      mid++;
    } else {
      break;
    }
  }

  // TODO - maybe wrap around, try next day?
  LOG_WARN("No more departures found for lineid = " + std::to_string(lineid));
  return nullptr;
}

// Get the departure given the line Id and tripid
const TransitDeparture* GraphTile::GetTransitDeparture(const uint32_t lineid,
                     const uint32_t tripid) const {
  uint32_t count = header_->departurecount();
  if (count == 0) {
    return nullptr;
  }

  // Departures are sorted by edge Id and then by departure time.
  // Binary search to find a departure with matching edge Id.
  int32_t low = 0;
  int32_t high = count-1;
  int32_t mid;
  bool found = false;
  while (low <= high) {
    mid = (low + high) / 2;
    if (departures_[mid].lineid() == lineid) {
      found = true;
      break;
    }
    if (lineid < departures_[mid].lineid() ) {
      high = mid - 1;
    } else {
      low = mid + 1;
    }
  }

  if (!found) {
    LOG_INFO("No departures found for lineid = " + std::to_string(lineid) +
             " and tripid = " + std::to_string(tripid));
    return nullptr;
  }

  if (found) {
    // Back up while prior is equal (or at the beginning)
    while (mid > 0 && departures_[mid-1].lineid() == lineid) {

      if (departures_[mid].tripid() == tripid)
        return &departures_[mid];

      mid--;
    }

    while (departures_[mid].tripid() != tripid && mid < count) {
      mid++;
    }

    if (departures_[mid].tripid() == tripid)
      return &departures_[mid];
  }

  LOG_INFO("No departures found for lineid = " + std::to_string(lineid) +
           " and tripid = " + std::to_string(tripid));
  return nullptr;
}

// Get the transit stop given its GraphId.
const TransitStop* GraphTile::GetTransitStop(const uint32_t idx) const {

  uint32_t count = header_->stopcount();
  if (count == 0)
    return nullptr;

  if (idx < count)
    return &transit_stops_[idx];
  throw std::runtime_error("GraphTile Transit Stop index out of bounds");
}

// Get the transit route given its route Id.
const TransitRoute* GraphTile::GetTransitRoute(const uint32_t routeid) const {
  uint32_t count = header_->routecount();
  if (count == 0) {
    return nullptr;
  }

  // Binary search - stop Ids should be unique
  int32_t low = 0;
  int32_t high = count-1;
  int32_t mid;
  while (low <= high) {
    mid = (low + high) / 2;
    if (transit_routes_[mid].routeid() == routeid) {
      return &transit_routes_[mid];
    }
    if (routeid < transit_routes_[mid].routeid() ) {
      high = mid - 1;
    } else {
      low = mid + 1;
    }
  }

  // Not found
  LOG_ERROR("No route found for routeid = " + std::to_string(routeid));
  return nullptr;
}

// Get a pointer to the first transfer record given the stop Id and also
// compute the number of transfer records for the stop.
std::pair<TransitTransfer*, uint32_t> GraphTile::GetTransfers(
              const uint32_t stopid) const {
  uint32_t count = header_->transfercount();
  if (count == 0) {
    return {nullptr, 0};
  }

  // Binary search
  int32_t low = 0;
  int32_t high = count-1;
  int32_t mid;
  bool found = false;
  while (low <= high) {
    mid = (low + high) / 2;
    if (transit_transfers_[mid].from_stopid() == stopid) {
      found = true;
      break;
    }
    if (stopid < transit_transfers_[mid].from_stopid() ) {
      high = mid - 1;
    } else {
      low = mid + 1;
    }
  }

  if (found) {
    // Back up while prior is equal (or at the beginning)
    while (mid > 0 &&
          transit_transfers_[mid-1].from_stopid() == stopid) {
      mid--;
    }

    // Set the start and increment until not equal to get the count
    uint32_t n = 0;
    TransitTransfer* start = &transit_transfers_[mid];
    while (transit_transfers_[mid].from_stopid() == stopid && mid < count) {
      n++;
      mid++;
    }
    return {start, n};
  } else {
    LOG_DEBUG("No transfers found from stopid = " + std::to_string(stopid));
    return {nullptr, 0};
  }
}

// Get a pointer to the transfer record given the from stop Id and
// the to stop id.
TransitTransfer* GraphTile::GetTransfer(const uint32_t from_stopid,
                                        const uint32_t to_stopid) const {
  uint32_t count = header_->transfercount();
  if (count == 0) {
    return nullptr;
  }

  // Binary search
  int32_t low = 0;
  int32_t high = count-1;
  int32_t mid;
  bool found = false;
  while (low <= high) {
    mid = (low + high) / 2;
    if (transit_transfers_[mid].from_stopid() == from_stopid) {
      found = true;
      break;
    }
    if (from_stopid < transit_transfers_[mid].from_stopid() ) {
      high = mid - 1;
    } else {
      low = mid + 1;
    }
  }

  if (found) {
    // Back up while prior is equal (or at the beginning)
    uint32_t m = mid;
    while (mid > 0 &&
          transit_transfers_[mid-1].from_stopid() == from_stopid) {
      if (transit_transfers_[mid-1].to_stopid() == to_stopid) {
        return &transit_transfers_[mid-1];
      }
      mid--;
    }

    // Set the start and increment until not equal to get the count
    mid = m;
    TransitTransfer* start = &transit_transfers_[mid];
    while (transit_transfers_[mid].from_stopid() == from_stopid && mid < count) {
      if (transit_transfers_[mid].to_stopid() == to_stopid) {
        return &transit_transfers_[mid];
      }
      mid++;
    }
    return nullptr;
  } else {
    LOG_DEBUG("No transfers found from stopid = " + std::to_string(from_stopid) +
              " to stopid " + std::to_string(to_stopid));
    return nullptr;
  }
}

// Get the access restriction given its directed edge index
std::vector<AccessRestriction> GraphTile::GetAccessRestrictions(const uint32_t idx) const {

  std::vector<AccessRestriction> restrictions;
  uint32_t count = header_->access_restriction_count();
  if (count == 0) {
    return restrictions;
  }

  // Access restriction are sorted by edge Id.
  // Binary search to find a access restriction with matching edge Id.
  int32_t low = 0;
  int32_t high = count-1;
  int32_t mid;
  bool found = false;
  while (low <= high) {
    mid = (low + high) / 2;
    if (access_restrictions_[mid].edgeindex() == idx) {
      found = true;
      break;
    }
    if (idx < access_restrictions_[mid].edgeindex() ) {
      high = mid - 1;
    } else {
      low = mid + 1;
    }
  }

  if (!found) {
    return restrictions;
  }

  // Back up while prior is equal (or at the beginning)
  while (mid > 0 && access_restrictions_[mid - 1].edgeindex() == idx) {
    mid--;
  }

  // Add restrictions
  while (access_restrictions_[mid].edgeindex() == idx && mid < count) {
    restrictions.emplace_back(access_restrictions_[mid]);
    mid++;
  }

  if (restrictions.size() == 0) {
    LOG_ERROR("No restrictions found for edge index = " + std::to_string(idx));
    return restrictions;
  }
}

// Get the array of graphids for this cell
midgard::iterable_t<GraphId> GraphTile::GetCell(size_t column, size_t row) const {
  auto offsets = header_->cell_offset(column, row);
  return iterable_t<GraphId>{edge_cells_ + offsets.first, edge_cells_ + offsets.second};
}

}
}
