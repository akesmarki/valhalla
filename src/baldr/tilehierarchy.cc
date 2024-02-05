#include "baldr/tilehierarchy.h"
#include "baldr/graphtileheader.h"
#include "boost/foreach.hpp"
#include "config.h"
#include "midgard/vector2.h"
#include <cassert>

using namespace valhalla::midgard;

namespace valhalla {
namespace baldr {

static std::vector<TileLevel> levels_;
static std::vector<TileLevel> transit_levels_;

void getLevelsfromConfig() {
  std::vector<int32_t> columnsvector;
  std::vector<int32_t> rowsvector;
  std::vector<float> tilesizesvector;
  bool bSizesInConfigOk = false;
  midgard::PointLL minpt = {-180., -90.};

  try {
    minpt = {config().get<float>("baldr.tiling_scheme.minpt.lng", -180),
             config().get<float>("baldr.tiling_scheme.minpt.lat", -90)};
    const boost::optional<const boost::property_tree::ptree&> columnsptree =
        config().get_child_optional("baldr.tiling_scheme.columns");
    const boost::optional<const boost::property_tree::ptree&> rowsptree =
        config().get_child_optional("baldr.tiling_scheme.rows");
    const boost::optional<const boost::property_tree::ptree&> tilesizesptree =
        config().get_child_optional("baldr.tiling_scheme.tilesizes");

    if (columnsptree && rowsptree && tilesizesptree) {
      BOOST_FOREACH (auto& v, *columnsptree) { columnsvector.push_back(v.second.get<int32_t>("")); }
      BOOST_FOREACH (auto& v, *rowsptree) { rowsvector.push_back(v.second.get<int32_t>("")); }
      BOOST_FOREACH (auto& v, *tilesizesptree) { tilesizesvector.push_back(v.second.get<float>("")); }
      if (columnsvector.size() >= 4 && rowsvector.size() >= 4 && tilesizesvector.size() >= 4) {
        bSizesInConfigOk = true;
      }
    }
  } catch (std::runtime_error&) {
    // fallback to default OSM tile layout
    ;
  }

  if (!bSizesInConfigOk) {
    // Default OSM tile-grid
    columnsvector = {90, 360, 1440, 1440};
    rowsvector = {45, 180, 720, 720};
    tilesizesvector = {4., 1., .25, .25};
    minpt = {-180., -90.};
  }

  levels_ = {

      TileLevel{0, stringToRoadClass("Primary"), "highway",
                midgard::Tiles<midgard::PointLL>{minpt, tilesizesvector[0], columnsvector[0],
                                                 rowsvector[0],
                                                 static_cast<unsigned short>(kBinsDim)}},

      TileLevel{1, stringToRoadClass("Tertiary"), "arterial",
                midgard::Tiles<midgard::PointLL>{minpt, tilesizesvector[1], columnsvector[1],
                                                 rowsvector[1],
                                                 static_cast<unsigned short>(kBinsDim)}},

      TileLevel{2, stringToRoadClass("ServiceOther"), "local",
                midgard::Tiles<midgard::PointLL>{minpt, tilesizesvector[2], columnsvector[2],
                                                 rowsvector[2],
                                                 static_cast<unsigned short>(kBinsDim)}},

  };

  transit_levels_ = {
      TileLevel{3, stringToRoadClass("ServiceOther"), "transit",
                midgard::Tiles<midgard::PointLL>{minpt, tilesizesvector[3], columnsvector[3],
                                                 rowsvector[3],
                                                 static_cast<unsigned short>(kBinsDim)}},
  };
}

const std::vector<TileLevel>& TileHierarchy::levels() {
  if (levels_.size()==0) {
    getLevelsfromConfig();
  }
  return levels_;
}

const TileLevel& TileHierarchy::GetTransitLevel() {
  // Should we make a class lower than service other for transit?
  if (transit_levels_.size()==0) {
    getLevelsfromConfig();
  }
  return transit_levels_[0];
}

midgard::AABB2<midgard::PointLL> TileHierarchy::GetGraphIdBoundingBox(const GraphId& id) {
  assert(id.Is_Valid());
  auto const& tileLevel = levels().at(id.level());
  return tileLevel.tiles.TileBounds(id.tileid());
}

// Returns the GraphId of the requested tile based on a lat,lng and a level.
// If the level is not supported an invalid id will be returned.
GraphId TileHierarchy::GetGraphId(const midgard::PointLL& pointll, const uint8_t level) {
  GraphId id;
  if (level < levels().size()) {
    auto tile_id = levels()[level].tiles.TileId(pointll);
    if (tile_id >= 0) {
      id = {static_cast<uint32_t>(tile_id), level, 0};
    }
  }
  return id;
}

// Gets the hierarchy level given the road class.
uint8_t TileHierarchy::get_level(const RoadClass roadclass) {
  if (roadclass <= levels()[0].importance) {
    return 0;
  } else if (roadclass <= levels()[1].importance) {
    return 1;
  } else {
    return 2;
  }
}

// Get the max hierarchy level.
uint8_t TileHierarchy::get_max_level() {
  return GetTransitLevel().level;
}

// Returns all the GraphIds of the tiles which intersect the given bounding
// box at that level.
std::vector<GraphId> TileHierarchy::GetGraphIds(const midgard::AABB2<midgard::PointLL>& bbox,
                                                const uint8_t level) {
  std::vector<GraphId> ids;
  if (level < levels().size()) {
    auto tile_ids = levels()[level].tiles.TileList(bbox);
    ids.reserve(tile_ids.size());

    for (auto tile_id : tile_ids) {
      ids.emplace_back(tile_id, level, 0);
    }
  }
  return ids;
}

// Returns all the GraphIds of the tiles which intersect the given bounding
// box at any level.
std::vector<GraphId> TileHierarchy::GetGraphIds(const midgard::AABB2<midgard::PointLL>& bbox) {
  std::vector<GraphId> ids;
  for (const auto& entry : levels()) {
    auto level_ids = GetGraphIds(bbox, entry.level);
    ids.reserve(ids.size() + level_ids.size());
    ids.insert(ids.end(), level_ids.begin(), level_ids.end());
  }
  return ids;
}

/**
 * Get the tiling system for a specified level.
 * @param level  Level Id.
 * @return Returns a const reference to the tiling system for this level.
 */
const midgard::Tiles<midgard::PointLL>& TileHierarchy::get_tiling(const uint8_t level) {
  const auto& transit_level = GetTransitLevel();
  if (level < levels().size()) {
    return levels()[level].tiles;
  } else if (level == transit_level.level) {
    return transit_level.tiles;
  }
  throw std::runtime_error("Invalid level Id for TileHierarchy::get_tiling");
}

GraphId TileHierarchy::parent(const GraphId& child_tile_id) {
  // bail if there is no parent
  if (child_tile_id.level() == 0)
    return GraphId(kInvalidGraphId);
  // get the tilings so we can use coordinates to pick the parent for the child
  auto parent_level = child_tile_id.level() - 1;
  const auto& parent_tiling = get_tiling(parent_level);
  const auto& child_tiling = get_tiling(child_tile_id.level());
  // grab just off of the child's corner to avoid edge cases
  auto corner = child_tiling.Base(child_tile_id.tileid()) +
                midgard::VectorXY<double>{parent_tiling.TileSize() / 2, parent_tiling.TileSize() / 2};
  // pick the parent from the child's coordinate
  auto parent_tile_index = parent_tiling.TileId(corner);
  return GraphId(parent_tile_index, parent_level, 0);
}

} // namespace baldr
} // namespace valhalla
