#include "baldr/connectivity_map.h"
#include "baldr/json.h"
#include "baldr/graphtile.h"

#include <valhalla/midgard/pointll.h>
#include <boost/filesystem.hpp>
#include <list>

using namespace valhalla::baldr;
using namespace valhalla::midgard;

namespace {
/*
   { "type": "FeatureCollection",
    "features": [
      { "type": "Feature",
        "geometry": {"type": "Point", "coordinates": [102.0, 0.5]},
        "properties": {"prop0": "value0"}
        },
      { "type": "Feature",
        "geometry": {
          "type": "LineString",
          "coordinates": [
            [102.0, 0.0], [103.0, 1.0], [104.0, 0.0], [105.0, 1.0]
            ]
          },
        "properties": {
          "prop0": "value0",
          "prop1": 0.0
          }
        },
      { "type": "Feature",
         "geometry": {
           "type": "Polygon",
           "coordinates": [
             [ [100.0, 0.0], [101.0, 0.0], [101.0, 1.0],
               [100.0, 1.0], [100.0, 0.0] ]
             ]
         },
         "properties": {
           "prop0": "value0",
           "prop1": {"this": "that"}
           }
         }
       ]
     }
   */

  json::MapPtr to_properties(const size_t color) {
    return json::map({
      {std::string("color"), static_cast<uint64_t>(color)}
    });
  }

  json::MapPtr to_geometry(const std::list<PointLL>& tiles) {
    auto multipoint = json::array({});
    for(const auto& tile : tiles)
      multipoint->emplace_back(json::array({json::fp_t{tile.first, 6}, json::fp_t{tile.second, 6}}));
    return json::map({
      {std::string("type"), std::string("MultiPoint")},
      {std::string("coordinates"), multipoint}
    });
  }

  json::MapPtr to_feature(const std::pair<size_t, std::list<PointLL> >& region) {
    return json::map({
      {std::string("type"), std::string("Feature")},
      {std::string("geometry"), to_geometry(region.second)},
      {std::string("properties"), to_properties(region.first)}
    });
  }

  template <class T>
  std::string to_feature_collection(const std::unordered_map<size_t, std::list<PointLL> >& regions, const std::multimap<size_t, size_t, T>& arities) {
    auto features = json::array({});
    for(const auto& arity : arities)
      features->emplace_back(to_feature(*regions.find(arity.second)));
    std::stringstream ss;
    ss << *json::map({
      {std::string("type"), std::string("FeatureCollection")},
      {std::string("features"), features}
    });
    return ss.str();
  }
}

namespace valhalla {
  namespace baldr {
    connectivity_map_t::connectivity_map_t(const TileHierarchy& tile_hierarchy):tile_hierarchy(tile_hierarchy) {
      // Set the transit level
      transit_level = tile_hierarchy.levels().rbegin()->second.level + 1;

      // Populate a map for each level of the tiles that exist
      for (uint32_t tile_level = 0; tile_level <= transit_level; tile_level++) {
        try {
          auto& level_colors = colors.insert({tile_level, std::unordered_map<uint32_t, size_t>{}}).first->second;
          boost::filesystem::path root_dir(tile_hierarchy.tile_dir() + '/' + std::to_string(tile_level) + '/');
          if(boost::filesystem::exists(root_dir) && boost::filesystem::is_directory(root_dir)) {
            for (boost::filesystem::recursive_directory_iterator i(root_dir), end; i != end; ++i) {
              if (!boost::filesystem::is_directory(i->path())) {
                GraphId id = GraphTile::GetTileId(i->path().string(), tile_hierarchy.tile_dir());
                level_colors.insert({id.tileid(), 0});
              }
            }
          }

          // All tiles have color 0 (not connected), go through and connect
          // (build the ColorMap). Transit level uses local hierarchy tiles
          auto c = colors.find(tile_level);
          if (tile_level == transit_level) {
            tile_hierarchy.levels().rbegin()->second.tiles.ColorMap(c->second);
          } else {
            tile_hierarchy.levels().find(tile_level)->second.tiles.ColorMap(c->second);
          }
        }
        catch(...) {
        }
      }
    }

    size_t connectivity_map_t::get_color(const GraphId& id) const {
      auto level = colors.find(id.level());
      if(level == colors.cend())
        return 0;
      auto color = level->second.find(id.tileid());
      if(color == level->second.cend())
        return 0;
      return color->second;
    }

    std::string connectivity_map_t::to_geojson(const uint32_t hierarchy_level) const {
      // Get the color level (throw exception if we don't have it)
      auto level = colors.find(hierarchy_level);
      if (level == colors.cend()) {
        throw std::runtime_error("No connectivity map for level");
      }

      // Get the tile bounding box (special case for transit tiles).
      uint32_t tile_level = (hierarchy_level == transit_level) ? transit_level - 1 : hierarchy_level;
      auto bbox = tile_hierarchy.levels().find(tile_level);
      if (bbox == tile_hierarchy.levels().cend())
        throw std::runtime_error("hierarchy level not found");

      //make a region map (inverse mapping of color to lists of tiles)
      //could cache this but shouldnt need to call it much
      std::unordered_map<size_t, std::list<PointLL> > regions;
      for(const auto& tile : level->second) {
        auto region = regions.find(tile.second);
        if(region == regions.end())
          regions.emplace(tile.second, std::list<PointLL>{bbox->second.tiles.Center(tile.first)});
        else
          region->second.push_back(bbox->second.tiles.Center(tile.first));
      }

      //record the arity of each region so we can put the biggest ones first
      auto comp = [](const size_t& a, const size_t& b){return a > b;};
      std::multimap<size_t, size_t, decltype(comp)> arities(comp);
      for(const auto& region : regions)
        arities.emplace(region.second.size(), region.first);

      //turn it into geojson
      return to_feature_collection<decltype(comp)>(regions, arities);
    }

    std::vector<size_t> connectivity_map_t::to_image(const uint32_t hierarchy_level) const {
      auto level = colors.find(hierarchy_level);
      if (level == colors.cend()) {
        throw std::runtime_error("No connectivity map for level");
      }

      uint32_t tile_level = (hierarchy_level == transit_level) ? transit_level - 1 : hierarchy_level;
      auto bbox = tile_hierarchy.levels().find(tile_level);
      if (bbox == tile_hierarchy.levels().cend())
        throw std::runtime_error("hierarchy level not found");

      std::vector<size_t> tiles(bbox->second.tiles.nrows() * bbox->second.tiles.ncolumns(), static_cast<uint32_t>(0));
      for(size_t i = 0; i < tiles.size(); ++i) {
        const auto color = level->second.find(static_cast<uint32_t>(i));
        if(color != level->second.cend())
          tiles[i] = color->second;
      }

      return tiles;
    }
  }
}
