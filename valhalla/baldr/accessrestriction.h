#ifndef VALHALLA_BALDR_ACCESSRESTRICTION_H_
#define VALHALLA_BALDR_ACCESSRESTRICTION_H_

#include <valhalla/baldr/graphconstants.h>
#include <valhalla/baldr/rapidjson_fwd.h>

#include <cstdint>

namespace valhalla {
namespace baldr {

/**
 * Information held for each access restriction.
 */
class AccessRestriction {
public:
  // Constructor with arguments
  AccessRestriction(const uint32_t edgeindex,
                    const AccessType type,
                    const uint32_t modes,
                    const uint64_t value,
                    const bool except_destination);

  /**
   * Get the internal edge index to which this access restriction applies.
   * @return  Returns the directed edge index within the tile.
   */
  uint32_t edgeindex() const;

  /**
   * Set the directed edge index to which this access restriction applies.
   * @param edgeindex   Edge index.
   */
  void set_edgeindex(const uint32_t edgeindex);

  /**
   * Get the type of the restriction.  See graphconstants.h
   * @return  Returns the type of the restriction
   */
  AccessType type() const;

  /**
   * Get the modes impacted by access restriction.
   * @return  Returns a bit mask of affected modes.
   */
  uint32_t modes() const;

  /**
   * Get the flag telling Whether local traffic is exempted
   * from this restriction.
   *
   * @return Returns whether or not local traffic can
   * ignore this restriction.
   */
  bool except_destination() const;

  /**
   * Set the exemption flag for local traffic.
   */
  void set_except_destination(const bool except_destination);

  /**
   * Get the value for this restriction.
   * @return  Returns the value
   */
  uint64_t value() const;

  /**
   * Set the value for this restriction.
   * @param  v  Value for this restriction.
   */
  void set_value(const uint64_t v);

  void json(rapidjson::writer_wrapper_t& writer) const;

  /**
   * operator < - for sorting. Sort by edge Id.
   * @param  other  Other access restriction to compare to.
   * @return  Returns true if edgeid < other edgeid.
   */
  bool operator<(const AccessRestriction& other) const;

protected:
  uint64_t edgeindex_ : 22;         // Directed edge index. Max index is:
                                    // kMaxTileEdgeCount in nodeinfo.h: 22 bits.
  uint64_t type_ : 6;               // Access type
  uint64_t modes_ : 12;             // Mode(s) this access restriction applies to
  uint64_t except_destination_ : 1; // Whether local traffic is exempted from this restriction
  uint64_t spare_ : 23;

  uint64_t value_; // Value for this restriction. Can take on
                   // different meanings per type
};

} // namespace baldr
} // namespace valhalla

#endif // VALHALLA_BALDR_ACCESSRESTRICTION_H_
