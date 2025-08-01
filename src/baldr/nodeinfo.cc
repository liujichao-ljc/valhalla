#include "baldr/nodeinfo.h"
#include "baldr/datetime.h"
#include "baldr/graphtile.h"
#include "baldr/rapidjson_utils.h"
#include "midgard/logging.h"

using namespace valhalla::midgard;
using namespace valhalla::baldr;

namespace {

void access_json(uint16_t access, rapidjson::writer_wrapper_t& writer) {
  writer("bicycle", static_cast<bool>(access & kBicycleAccess));
  writer("bus", static_cast<bool>(access & kBusAccess));
  writer("car", static_cast<bool>(access & kAutoAccess));
  writer("emergency", static_cast<bool>(access & kEmergencyAccess));
  writer("HOV", static_cast<bool>(access & kHOVAccess));
  writer("pedestrian", static_cast<bool>(access & kPedestrianAccess));
  writer("taxi", static_cast<bool>(access & kTaxiAccess));
  writer("truck", static_cast<bool>(access & kTruckAccess));
  writer("wheelchair", static_cast<bool>(access & kWheelchairAccess));
}

void admin_json(const AdminInfo& admin, uint16_t tz_index, rapidjson::writer_wrapper_t& writer) {
  // admin
  writer("iso_3166-1", admin.country_iso());
  writer("country", admin.country_text());
  writer("iso_3166-2", admin.state_iso());
  writer("state", admin.state_text());

  // timezone
  auto tz = DateTime::get_tz_db().from_index(tz_index);
  if (tz) {
    writer("time_zone_name", tz->name());
  }
}

/**
 * Get the updated bit field.
 * @param dst  Data member to be updated.
 * @param src  Value to be updated.
 * @param pos  Position (pos element within the bit field).
 * @param len  Length of each element within the bit field.
 * @return  Returns an updated value for the bit field.
 */
uint32_t
OverwriteBits(const uint32_t dst, const uint32_t src, const uint32_t pos, const uint32_t len) {
  uint32_t shift = (pos * len);
  uint32_t mask = (((uint32_t)1 << len) - 1) << shift;
  return (dst & ~mask) | (src << shift);
}

} // namespace

namespace valhalla {
namespace baldr {

// Default constructor. Initialize to all 0's.
NodeInfo::NodeInfo() {
  memset(this, 0, sizeof(NodeInfo));
}

NodeInfo::NodeInfo(const PointLL& tile_corner,
                   const PointLL& ll,
                   const uint32_t access,
                   const NodeType type,
                   const bool traffic_signal,
                   const bool tagged_access,
                   const bool private_access,
                   const bool cash_only_toll) {
  memset(this, 0, sizeof(NodeInfo));
  set_latlng(tile_corner, ll);
  set_access(access);
  set_type(type);
  set_traffic_signal(traffic_signal);
  set_tagged_access(tagged_access);
  set_private_access(private_access);
  set_cash_only_toll(cash_only_toll);
}

// Sets the latitude and longitude.
void NodeInfo::set_latlng(const PointLL& tile_corner, const PointLL& ll) {
  // Protect against a node being slightly outside the tile (due to float roundoff)
  lat_offset_ = 0;
  if (ll.lat() > tile_corner.lat()) {
    auto lat = std::round((ll.lat() - tile_corner.lat()) / 1e-7);
    lat_offset_ = lat / 10;
    lat_offset7_ = lat - lat_offset_ * 10;
  }

  lon_offset_ = 0;
  if (ll.lng() > tile_corner.lng()) {
    auto lon = std::round((ll.lng() - tile_corner.lng()) / 1e-7);
    lon_offset_ = lon / 10;
    lon_offset7_ = lon - lon_offset_ * 10;
  }
}

// Set the index in the node's tile of its first outbound edge.
void NodeInfo::set_edge_index(const uint32_t edge_index) {
  if (edge_index > kMaxGraphId) {
    // Consider this a catastrophic error
    throw std::runtime_error("NodeInfo: edge index exceeds max");
  }
  edge_index_ = edge_index;
}

// Set the number of outbound directed edges.
void NodeInfo::set_edge_count(const uint32_t edge_count) {
  if (edge_count > kMaxEdgesPerNode) {
    // Log an error and set count to max.
    LOG_ERROR("NodeInfo: edge count exceeds max: " + std::to_string(edge_count));
    edge_count_ = kMaxEdgesPerNode;
  } else {
    edge_count_ = edge_count;
  }
}

// Set the access modes (bit mask) allowed to pass through the node.
void NodeInfo::set_access(const uint32_t access) {
  if (access > kAllAccess) {
    LOG_ERROR("NodeInfo: access exceeds maximum allowed: " + std::to_string(access));
    access_ = (access & kAllAccess);
  } else {
    access_ = access;
  }
}

// Set the intersection type.
void NodeInfo::set_intersection(const IntersectionType type) {
  intersection_ = static_cast<uint32_t>(type);
}

// Set the index of the administrative information within this tile.
void NodeInfo::set_admin_index(const uint16_t admin_index) {
  if (admin_index > kMaxAdminsPerTile) {
    // Log an error and set count to max.
    LOG_ERROR("NodeInfo: admin index exceeds max: " + std::to_string(admin_index));
    admin_index_ = kMaxAdminsPerTile;
  } else {
    admin_index_ = admin_index;
  }
}

// Set the timezone index.
void NodeInfo::set_timezone(const uint32_t tz_idx) {
  if (tz_idx > kMaxTimeZoneIdExt1)
    throw std::runtime_error("NodeInfo: timezone index exceeds max: " + std::to_string(tz_idx));
  timezone_ = tz_idx & ((1 << 9) - 1); // first 9 bits for backwards compat
  timezone_ext_1_ =
      (tz_idx & (1 << 9)) >> 9; // 10th bit for new timezones carved out of old ones in 2023
  // uncomment if a new timezone ever gets created from a previously new
  // timezone (reference release is 2023c)
  // timezone_ext_2_ = (tz_idx & (1 << 10)) >> 10;
}

// Set the driveability of the local directed edge given a local
// edge index.
void NodeInfo::set_local_driveability(const uint32_t localidx, const Traversability t) {
  if (localidx > kMaxLocalEdgeIndex) {
    LOG_WARN("Exceeding max local index on set_local_driveability - skip");
  } else {
    local_driveability_ = OverwriteBits(local_driveability_, static_cast<uint32_t>(t), localidx, 2);
  }
}

// Set the relative density
void NodeInfo::set_density(const uint32_t density) {
  if (density > kMaxDensity) {
    LOG_WARN("Exceeding max. density: " + std::to_string(density));
    density_ = kMaxDensity;
  } else {
    density_ = density;
  }
}

// Set the node type.
void NodeInfo::set_type(const NodeType type) {
  type_ = static_cast<uint32_t>(type);
}

// Set the number of drivable edges on the local level. Subtract 1 so
// a value up to kMaxLocalEdgeIndex+1 can be stored.
void NodeInfo::set_local_edge_count(const uint32_t n) {
  if (n > kMaxLocalEdgeIndex + 1) {
    LOG_INFO("Exceeding max. local edge count: " + std::to_string(n));
    local_edge_count_ = kMaxLocalEdgeIndex;
  } else if (n == 0) {
    LOG_ERROR("Node with 0 local edges found");
  } else {
    local_edge_count_ = n - 1;
  }
}

// Set the flag indicating driving is on the right hand side of the road
// for outbound edges from this node.
void NodeInfo::set_drive_on_right(const bool rsd) {
  drive_on_right_ = rsd;
}

// Set the elevation at this node.
void NodeInfo::set_elevation(const float elevation) {
  if (elevation < kNodeMinElevation) {
    elevation_ = 0;
  } else {
    uint32_t elev = static_cast<uint32_t>((elevation - kNodeMinElevation) / kNodeElevationPrecision);
    elevation_ = (elev > kNodeMaxStoredElevation) ? kNodeMaxStoredElevation : elev;
  }
}

// Sets the flag indicating if access was originally tagged.
void NodeInfo::set_tagged_access(const bool tagged_access) {
  tagged_access_ = tagged_access;
}

// Sets the flag indicating a mode change is allowed at this node.
// The access data tells which modes are allowed at the node. Examples
// include transit stops, bike share locations, and parking locations.
void NodeInfo::set_mode_change(const bool mc) {
  mode_change_ = mc;
}

// Sets the named intersection.
void NodeInfo::set_named_intersection(const bool named) {
  named_ = named;
}

// Set the traffic signal flag.
void NodeInfo::set_traffic_signal(const bool traffic_signal) {
  traffic_signal_ = traffic_signal;
}

// Set the transit stop index.
void NodeInfo::set_stop_index(const uint32_t stop_index) {
  transition_index_ = stop_index;
}

// Set the heading of the local edge given its local index. Supports
// up to 8 local edges. Headings are reduced to 8 bits.
void NodeInfo::set_heading(uint32_t localidx, uint32_t heading) {
  if (localidx > kMaxLocalEdgeIndex) {
    LOG_WARN("Local index exceeds max in set_heading, skip");
  } else {
    // Has to be 64 bit!
    uint64_t hdg = static_cast<uint64_t>(std::round((heading % 360) * kHeadingShrinkFactor));
    headings_ |= hdg << static_cast<uint64_t>(localidx * 8);
  }
}

// Set the connecting way id for a transit stop.
void NodeInfo::set_connecting_wayid(const uint64_t wayid) {
  if (wayid >> 63)
    throw std::logic_error("Way ids larger than 63 bits are not allowed for transit connections");
  headings_ = wayid;
}

void NodeInfo::set_connecting_point(const midgard::PointLL& p) {
  if (!p.InRange())
    throw std::logic_error("Invalid coordinates are not allowed for transit connections");
  headings_ = static_cast<uint64_t>(p) | (1ull << 63);
}

void NodeInfo::json(const graph_tile_ptr& tile, rapidjson::writer_wrapper_t& writer) const {
  auto ll = latlng(tile->header()->base_ll());

  writer.set_precision(6);
  writer("lon", ll.first);
  writer("lat", ll.second);
  writer.set_precision(2);
  writer("elevation", elevation());
  writer.set_precision(3);

  writer("edge_count", static_cast<uint64_t>(edge_count_));

  writer.start_object("access");
  access_json(access_, writer);
  writer.end_object();

  writer("tagged_access", static_cast<bool>(tagged_access_));
  writer("intersection_type", to_string(static_cast<IntersectionType>(intersection_)));

  writer.start_object("administrative");
  admin_json(tile->admininfo(admin_index_), timezone_, writer);
  writer.end_object();

  writer("density", static_cast<uint64_t>(density_));
  writer("local_edge_count", static_cast<uint64_t>(local_edge_count_ + 1));
  writer("drive_on_right", static_cast<bool>(drive_on_right_));
  writer("mode_change", static_cast<bool>(mode_change_));
  writer("private_access", static_cast<bool>(private_access_));
  writer("traffic_signal", static_cast<bool>(traffic_signal_));
  writer("type", to_string(static_cast<NodeType>(type_)));
  writer("transition_count", static_cast<uint64_t>(transition_count_));
  writer("named_intersection", static_cast<bool>(named_));

  if (is_transit()) {
    writer("stop_index", static_cast<uint64_t>(stop_index()));
  }
}

} // namespace baldr
} // namespace valhalla
