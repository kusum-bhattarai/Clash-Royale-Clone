#include "clash_royale/path/direct.hpp"

#include <cstdlib>

#include "clash_royale/core/arena.hpp"

namespace cr {

std::optional<Path> DirectPathfinder::findPath(const Arena& arena, const PathRequest& request) {
    const Point from = request.from;
    const Point to = request.to;

    if (!arena.inInterior(from.x, from.y) || !arena.inInterior(to.x, to.y)) {
        return std::nullopt;
    }
    if (from == to) {
        return Path{};
    }

    const auto enterable = [&](Point p) {
        if (!arena.isPassable(p.x, p.y, request.domain)) {
            return false;
        }
        if (p == to) {
            return true;
        }
        return request.obstacles == nullptr || !request.obstacles->blocks(p, request.domain);
    };

    // Bresenham, generalised to both octants.
    int x = from.x;
    int y = from.y;
    const int dx = std::abs(to.x - x);
    const int dy = -std::abs(to.y - y);
    const int stepX = (from.x < to.x) ? 1 : -1;
    const int stepY = (from.y < to.y) ? 1 : -1;
    int error = dx + dy;

    Path path;
    while (true) {
        const int doubled = 2 * error;
        if (doubled >= dy) {
            error += dy;
            x += stepX;
        }
        if (doubled <= dx) {
            error += dx;
            y += stepY;
        }

        const Point next{x, y};
        if (!enterable(next)) {
            return std::nullopt;  // let the caller fall back to a search
        }
        path.push_back(next);

        if (next == to) {
            return path;
        }
    }
}

}  // namespace cr
