#pragma once

#include "clash_royale/path/pathfinder.hpp"

namespace cr {

/// A straight line, for movers that terrain barely constrains.
///
/// Air units cross water freely, so searching for them is wasted work: the
/// straight line is almost always the answer. This walks a Bresenham line and
/// reports failure if anything on it is impassable, which lets a caller fall
/// back to a real search for the rare case where something does block the way.
class DirectPathfinder final : public Pathfinder {
public:
    std::optional<Path> findPath(const Arena& arena, const PathRequest& request) override;
};

}  // namespace cr
