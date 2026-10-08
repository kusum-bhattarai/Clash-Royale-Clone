#pragma once

#include <string_view>

namespace cr {

/// Ids of the cards the game ships with.
///
/// These are plain strings rather than enumerators on purpose: a closed enum
/// could not name a card that a downstream project defines, and any code
/// switching on one would be incomplete by construction. The constants exist so
/// built-in cards can still be referred to without spelling the literal.
namespace cards {

inline constexpr std::string_view Knight = "knight";
inline constexpr std::string_view Golem = "golem";
inline constexpr std::string_view Pekka = "pekka";
inline constexpr std::string_view Goblins = "goblins";
inline constexpr std::string_view Dragon = "dragon";
inline constexpr std::string_view Wizard = "wizard";
inline constexpr std::string_view Archers = "archers";
inline constexpr std::string_view Canon = "canon";
inline constexpr std::string_view KingTower = "king_tower";
inline constexpr std::string_view QueenTower = "queen_tower";

}  // namespace cards

}  // namespace cr
