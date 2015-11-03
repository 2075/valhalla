#include <unordered_map>
#include <boost/property_tree/info_parser.hpp>

#include <valhalla/baldr/json.h>
#include <valhalla/midgard/distanceapproximator.h>
#include <valhalla/midgard/logging.h>

#include "loki/service.h"
#include "loki/search.h"

using namespace prime_server;
using namespace valhalla::baldr;
using namespace valhalla::loki;

namespace {

const std::unordered_map<std::string, loki_worker_t::ACTION_TYPE> MATRIX {
  {"one_to_many",loki_worker_t::ONE_TO_MANY},
  {"many_to_one",loki_worker_t::MANY_TO_ONE},
  {"many_to_many",loki_worker_t::MANY_TO_MANY}
};

  const headers_t::value_type CORS{"Access-Control-Allow-Origin", "*"};
  const headers_t::value_type JSON_MIME{"Content-type", "application/json;charset=utf-8"};
  const headers_t::value_type JS_MIME{"Content-type", "application/javascript;charset=utf-8"};

  //TODO: move json header to baldr
  //TODO: make objects serialize themselves

  json::ArrayPtr serialize_edges(const PathLocation& location, GraphReader& reader, bool verbose) {
    auto array = json::array({});
    std::unordered_multimap<uint64_t, PointLL> ids;
    for(const auto& edge : location.edges()) {
      try {
        //get the osm way id
        auto tile = reader.GetGraphTile(edge.id);
        auto* directed_edge = tile->directededge(edge.id);
        auto edge_info = tile->edgeinfo(directed_edge->edgeinfo_offset());
        //check if we did this one before
        auto range = ids.equal_range(edge_info->wayid());
        bool duplicate = false;
        for(auto id = range.first; id != range.second; ++id) {
          if(id->second == location.vertex()) {
            duplicate = true;
            break;
          }
        }
      }
      catch(...) {
        //this really shouldnt ever get hit
        LOG_WARN("Expected edge not found in graph but found by loki::search!");
      }
    }
    return array;
  }

  void check_locations(const std::vector<Location>& locations, const size_t matrix_max_locations) {
      //check that location size does not exceed max.
      if (locations.size() > matrix_max_locations)
        throw std::runtime_error("Number of locations exceeds the max location limit.");
      LOG_INFO("Location size::" + std::to_string(locations.size()));
   }

  void check_distance(const GraphReader& reader, const std::vector<Location>& locations, const size_t origin, const size_t start, const size_t end, float matrix_max_distance) {
    //see if any locations pairs are unreachable or too far apart
    auto lowest_level = reader.GetTileHierarchy().levels().rbegin();

    //one to many should be distance between:a,b a,c ; many to one: a,c b,c ; many to many should be all pairs
    for(size_t i = start; i < end; ++i) {
      //check connectivity
      uint32_t a_id = lowest_level->second.tiles.TileId(locations[origin].latlng_);
      uint32_t b_id = lowest_level->second.tiles.TileId(locations[i].latlng_);
      if(!reader.AreConnected({a_id, lowest_level->first, 0}, {b_id, lowest_level->first, 0}))
        throw std::runtime_error("Locations are in unconnected regions. Go check/edit the map at osm.org");

      //check if distance between latlngs exceed max distance limit the chosen matrix type
      auto path_distance = locations[origin].latlng_.Distance(locations[i].latlng_);

      if (path_distance > matrix_max_distance)
        throw std::runtime_error("Path distance exceeds the max distance limit.");
    }
  }
}

namespace valhalla {
  namespace loki {

    worker_t::result_t loki_worker_t::matrix(const ACTION_TYPE& action, boost::property_tree::ptree& request) {
      for (auto& it : MATRIX){
        if (action == it.second) {
          switch (it.second) {
            case ONE_TO_MANY:
              check_locations(locations,(matrix_max_locations.find(it.first))->second);
              check_distance(reader,locations,0,0,locations.size(),(matrix_max_distance.find(it.first))->second);
              request.put("matrix_type", std::string(it.first));
              break;
            case MANY_TO_ONE:
              check_locations(locations,(matrix_max_locations.find(it.first))->second);
              check_distance(reader,locations,locations.size()-1,0,locations.size()-1,(matrix_max_distance.find(it.first))->second);
              request.put("matrix_type", std::string(it.first));
              break;
            case MANY_TO_MANY:
              check_locations(locations,(matrix_max_locations.find(it.first))->second);
              for(size_t i = 0; i < locations.size()-1; ++i)
                check_distance(reader,locations,i,(i+1),locations.size(),(matrix_max_distance.find(it.first))->second);

              request.put("matrix_type", std::string(it.first));
              break;
          }
        }
      }

      //correlate the various locations to the underlying graph
      for(size_t i = 0; i < locations.size(); ++i) {
        auto correlated = loki::Search(locations[i], reader, costing_filter);
        request.put_child("correlated_" + std::to_string(i), correlated.ToPtree(i));
      }

      std::stringstream stream;
      boost::property_tree::write_info(stream, request);
      worker_t::result_t result{true};
      result.messages.emplace_back(stream.str());
      return result;
    }
  }
}
