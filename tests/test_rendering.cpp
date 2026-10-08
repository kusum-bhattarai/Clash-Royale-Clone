// Tests for the terminal layout.
//
// These assert what ends up on screen. Nothing used to, which is how the frame
// came to sit off-centre, and how health bars came to be drawn two rows below
// their unit and one column to its right. Those are not simulation bugs, so no
// amount of testing the simulation would have caught them.

#include <gtest/gtest.h>

#include <string>
#include <vector>

#include "clash_royale/sim/simulation.hpp"
#include "clash_royale/tui/renderer.hpp"

#include "test_helpers.hpp"

using namespace cr;
using namespace cr::testing;

namespace {

/// Renders a board and hands back the composed rows.
std::vector<std::string> renderOf(const Board& board) {
    Renderer renderer;
    renderer.clear();
    renderer.drawBoard(board);
    return renderer.frame();
}

/// Columns on `row` holding anything other than a space.
std::vector<int> inkColumns(const std::string& row) {
    std::vector<int> columns;
    for (int x = 0; x < static_cast<int>(row.size()); ++x) {
        if (row[static_cast<std::size_t>(x)] != ' ') {
            columns.push_back(x);
        }
    }
    return columns;
}

}  // namespace

TEST(Rendering, FrameFillsTheBufferAndIsCentredOnTheRiver) {
    Simulation sim;
    const std::vector<std::string> frame = renderOf(sim.board());

    ASSERT_EQ(frame.size(), static_cast<std::size_t>(kArenaHeight));

    // Border on the first and last row, and the first and last column, so the
    // middle of the frame is the middle of the arena. The corners carry the
    // vertical wall's character.
    EXPECT_EQ(frame.front().find_first_not_of("-|"), std::string::npos);
    EXPECT_EQ(frame.back().find_first_not_of("-|"), std::string::npos);
    for (const std::string& row : frame) {
        ASSERT_EQ(row.size(), static_cast<std::size_t>(kArenaWidth));
        EXPECT_EQ(row.front(), '|');
        EXPECT_EQ(row.back(), '|');
    }

    // The river is on the frame's centre row, not half a row off it.
    const int river = sim.arena().riverRows().front();
    EXPECT_EQ(river, (static_cast<int>(frame.size()) - 1) / 2);
}

TEST(Rendering, TheTwoHalvesAreMirrorImages) {
    // The complaint that started this: player two's towers looked closer to the
    // river than player one's, even though the simulation places them
    // symmetrically. Whatever is drawn in the top half must appear mirrored in
    // the bottom half.
    Simulation sim;
    const std::vector<std::string> frame = renderOf(sim.board());
    const int lastRow = static_cast<int>(frame.size()) - 1;

    for (int y = 2; y < lastRow / 2; ++y) {
        const std::vector<int> top = inkColumns(frame[static_cast<std::size_t>(y)]);
        const std::vector<int> bottom = inkColumns(frame[static_cast<std::size_t>(lastRow - y)]);
        EXPECT_EQ(top.size(), bottom.size())
            << "row " << y << " and its mirror " << (lastRow - y) << " differ in how much is drawn";
    }
}

TEST(Rendering, HealthBarsSitDirectlyBesideTheirUnit) {
    Board board = openBoard();
    auto tower = board.spawn(defaultCards().get(cards::KingTower), 20, 27, /*isPlayer=*/true, Lane::LEFT);

    const std::vector<std::string> frame = renderOf(board);

    // One row away, not two. The bar used to be offset twice: once by the
    // caller and once again inside drawHealthBar.
    const std::string& barRow = frame[static_cast<std::size_t>(tower->getY() + 1)];
    const std::size_t open = barRow.find('[');
    const std::size_t close = barRow.find(']');
    ASSERT_NE(open, std::string::npos) << "no health bar one row from the tower";
    ASSERT_NE(close, std::string::npos);

    // And centred on the tower rather than a column to its right.
    const int centre = static_cast<int>(open + close) / 2;
    EXPECT_EQ(centre, tower->getX()) << "bar spans columns " << open << " to " << close
                                     << ", centred on " << centre << " rather than " << tower->getX();
}

TEST(Rendering, HealthBarsFaceAwayFromTheRiver) {
    // Which side the bar goes determines whether the two halves look alike.
    Board board;  // Arena::standard()
    auto theirs = board.spawn(defaultCards().get(cards::QueenTower), 8, 9, /*isPlayer=*/false, Lane::LEFT);
    auto ours = board.spawn(defaultCards().get(cards::QueenTower), 8, 25, /*isPlayer=*/true, Lane::LEFT);

    const std::vector<std::string> frame = renderOf(board);
    const auto hasBar = [&](int row) {
        return frame[static_cast<std::size_t>(row)].find('[') != std::string::npos;
    };

    EXPECT_TRUE(hasBar(theirs->getY() - 1)) << "a tower above the river should carry its bar above it";
    EXPECT_TRUE(hasBar(ours->getY() + 1)) << "a tower below the river should carry its bar below it";
}

TEST(Rendering, TerrainIsDrawnAndTheBridgesAreVisible) {
    Simulation sim;
    const std::vector<std::string> frame = renderOf(sim.board());
    const int river = sim.arena().riverRows().front();
    const std::string& row = frame[static_cast<std::size_t>(river)];

    EXPECT_NE(row.find('~'), std::string::npos) << "no water drawn";
    EXPECT_NE(row.find('='), std::string::npos) << "no bridge drawn";

    for (int column : sim.arena().bridgeColumns()) {
        EXPECT_EQ(row[static_cast<std::size_t>(column)], '=') << "bridge missing at column " << column;
    }
}

TEST(Rendering, NothingIsDrawnOverTheBorder) {
    // The prompt and the status line used to be written into the buffer, and
    // the prompt landed on the bottom border.
    Simulation sim;
    Renderer renderer;
    renderer.clear();
    renderer.drawBoard(sim.board());
    renderer.drawStatus(10.0f, 10.0f, 0.0f);
    renderer.drawPrompt("SELECT LANE: (L)eft or (R)ight. (X) to cancel.");

    const std::vector<std::string>& frame = renderer.frame();
    EXPECT_EQ(frame.front().find_first_not_of("-|"), std::string::npos) << "top border was written over";
    EXPECT_EQ(frame.back().find_first_not_of("-|"), std::string::npos) << "bottom border was written over";
}
