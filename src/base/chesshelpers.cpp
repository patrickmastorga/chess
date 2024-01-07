#include "chesshelpers.h"

#include <stdexcept>
#include <string>

int chesshelpers::algebraicNotationToBoardIndex(std::string& algebraic)

{
    if (algebraic.size() != 2) {
        throw std::invalid_argument("Algebraic notation should only be two letters long!");
    }

    int file = algebraic[0] - 'a';
    int rank = algebraic[1] - '1';

    if (file < 0 || file > 7 || rank < 0 || rank > 7) {
        throw std::invalid_argument("Algebraic notation should be in the form [a-h][1-8]!");
    }

    return rank * 8 + file;
}

std::string chesshelpers::boardIndexToAlgebraicNotation(int boardIndex)
{
    if (boardIndex < 0 || boardIndex > 63) {
        throw std::invalid_argument("Algebraic notation should only be two letters long!");
    }

    char file = 'a' + boardIndex % 8;
    char rank = '1' + boardIndex / 8;

    std::string algebraic;
    algebraic = file;
    algebraic += rank;

    return algebraic;
}