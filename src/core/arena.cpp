#include "clash_royale/core/arena.hpp"

#include <algorithm>
#include <cstdlib>

namespace cr {

Arena::Arena(int width, int height) : m_width(std::max(3, width)), m_height(std::max(3, height)) {
    m_tiles.assign(static_cast<std::size_t>(m_width) * static_cast<std::size_t>(m_height), Tile::Ground);

    // The border is where the front-end draws its frame; nothing may stand
    // there, which is also what keeps the interior checks and the movement
    // clamp agreeing with each other.
    for (int x = 0; x < m_width; ++x) {
        m_tiles[indexOf(x, 0)] = Tile::Blocked;
        m_tiles[indexOf(x, m_height - 1)] = Tile::Blocked;
    }
    for (int y = 0; y < m_height; ++y) {
        m_tiles[indexOf(0, y)] = Tile::Blocked;
        m_tiles[indexOf(m_width - 1, y)] = Tile::Blocked;
    }
}

Arena Arena::standard() {
    Arena arena(kArenaWidth, kArenaHeight);

    // The river sits on the midline. A single row is not an aesthetic choice:
    // it is the only placement that avoids every fixed position already in use.
    // Rows 16 and 18 are where the two Canons spawn, and widening the river
    // onto either would drop a building into the water.
    const int midline = kArenaHeight / 2;

    // Bridges are centred on the lane spawn columns, so a unit deployed in a
    // lane is already lined up with its crossing. Three tiles wide, to keep a
    // queue of units from funnelling into single file.
    arena.carveRiver({midline}, {kArenaWidth / 4, kArenaWidth * 3 / 4}, /*bridgeWidth=*/3);
    return arena;
}

std::size_t Arena::indexOf(int x, int y) const {
    return static_cast<std::size_t>(y) * static_cast<std::size_t>(m_width) + static_cast<std::size_t>(x);
}

bool Arena::inBounds(int x, int y) const {
    return x >= 0 && x < m_width && y >= 0 && y < m_height;
}

bool Arena::inInterior(int x, int y) const {
    return x >= 1 && x <= m_width - 2 && y >= 1 && y <= m_height - 2;
}

Tile Arena::tile(int x, int y) const {
    return inBounds(x, y) ? m_tiles[indexOf(x, y)] : Tile::Blocked;
}

void Arena::setTile(int x, int y, Tile value) {
    if (inBounds(x, y)) {
        m_tiles[indexOf(x, y)] = value;
    }
}

bool Arena::isPassable(int x, int y, MovementDomain domain) const {
    if (!inInterior(x, y)) {
        return false;
    }
    switch (tile(x, y)) {
        case Tile::Ground:
        case Tile::Bridge:
            return true;
        case Tile::Water:
            return domain == MovementDomain::Air;
        case Tile::Blocked:
            return false;
    }
    return false;
}

void Arena::clampToInterior(int& x, int& y) const {
    x = std::max(1, std::min(x, m_width - 2));
    y = std::max(1, std::min(y, m_height - 2));
}

int Arena::riverSide(int y) const {
    if (m_riverRows.empty()) {
        return 0;
    }
    if (y < m_riverRows.front()) {
        return -1;
    }
    if (y > m_riverRows.back()) {
        return 1;
    }
    return 0;
}

int Arena::bridgeColumnFor(Lane lane, int fromX) const {
    if (m_bridgeColumns.empty()) {
        return fromX;
    }

    switch (lane) {
        case Lane::LEFT:
            return m_bridgeColumns.front();
        case Lane::RIGHT:
            return m_bridgeColumns.back();
        case Lane::CENTER:
            break;
    }

    // CENTER has no crossing of its own, so take the nearer one.
    return *std::min_element(m_bridgeColumns.begin(), m_bridgeColumns.end(), [fromX](int a, int b) {
        return std::abs(a - fromX) < std::abs(b - fromX);
    });
}

void Arena::carveRiver(std::vector<int> rows, std::vector<int> centres, int bridgeWidth) {
    // Clear any previous river before cutting the new one.
    for (int y : m_riverRows) {
        for (int x = 1; x <= m_width - 2; ++x) {
            setTile(x, y, Tile::Ground);
        }
    }

    std::sort(rows.begin(), rows.end());
    rows.erase(std::unique(rows.begin(), rows.end()), rows.end());
    rows.erase(std::remove_if(rows.begin(), rows.end(),
                              [this](int y) { return y < 1 || y > m_height - 2; }),
               rows.end());
    m_riverRows = rows;

    for (int y : m_riverRows) {
        for (int x = 1; x <= m_width - 2; ++x) {
            setTile(x, y, Tile::Water);
        }
    }

    std::sort(centres.begin(), centres.end());
    centres.erase(std::unique(centres.begin(), centres.end()), centres.end());
    m_bridgeColumns.clear();

    const int halfWidth = std::max(1, bridgeWidth) / 2;
    for (int centre : centres) {
        bool laid = false;
        for (int x = centre - halfWidth; x <= centre - halfWidth + std::max(1, bridgeWidth) - 1; ++x) {
            if (x < 1 || x > m_width - 2) {
                continue;
            }
            for (int y : m_riverRows) {
                setTile(x, y, Tile::Bridge);
            }
            laid = true;
        }
        if (laid) {
            m_bridgeColumns.push_back(centre);
        }
    }
}

}  // namespace cr
