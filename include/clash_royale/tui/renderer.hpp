#pragma once
#include "clash_royale/core/types.hpp"
#include "clash_royale/sim/board.hpp"
#include <string>
#include <vector>

namespace cr {

class Renderer {
public:
    Renderer();
    void clear();
    void drawBoard(const Board& board);
    void drawStatus(float elixirPlayerOne, float elixirPlayerTwo, float gameTimer = 0.0f);
    void drawPrompt(const std::string& message);
    void display();

    /// The composed arena, one string per row, as display() would print it.
    ///
    /// Exposed so the layout can be asserted rather than eyeballed. The frame
    /// being off-centre, and health bars sitting two rows below their unit and
    /// a column to its right, all survived for as long as they did because
    /// nothing tested what was actually drawn.
    const std::vector<std::string>& frame() const { return buffer; }

private:
    std::vector<std::string> buffer;
    
    void drawTerrain(const Arena& arena);
    void drawHealthBar(int x, int y, int health, int maxHealth);
    void drawBorders();

    /// True when (x, y) is inside the frame, so nothing writes over the border.
    bool insideFrame(int x, int y) const;

    /// Writes `text` centred on `row` of the arena buffer.
    void centreText(int row, const std::string& text);

    /// Pads `text` so it sits under the middle of the arena.
    std::string centred(const std::string& text) const;

    std::string m_status;
    std::string m_prompt;
};

}  // namespace cr
