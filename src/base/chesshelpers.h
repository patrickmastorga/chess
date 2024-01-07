#pragma once
#include <string>

namespace chesshelpers
{
    /**
     * @param algebraic notation for position on chess board (ex e3, a1, c8)
     * @return uint8 index [0, 63] -> [a1, h8] of square on board
     */
    int algebraicNotationToBoardIndex(std::string& algebraic);

    /**
     * @param boardIndex index [0, 63] -> [a1, h8] of square on board
     * @return std::string notation for position on chess board (ex e3, a1, c8)
     */
    std::string boardIndexToAlgebraicNotation(int boardIndex);
}