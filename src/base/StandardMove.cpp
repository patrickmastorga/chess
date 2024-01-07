#include "StandardMove.h"
#include "chesshelpers.h"
#include <iostream>

StandardMove::StandardMove(int start, int target, int promotion) : startSquare(start), targetSquare(target), promotion(promotion) {}

StandardMove::StandardMove() : StandardMove(0, 0) {}

bool StandardMove::operator==(const StandardMove& other) const
{
    return this->startSquare == other.startSquare
        && this->targetSquare == other.targetSquare
        && this->promotion == other.promotion;
}

std::ostream& operator<<(std::ostream& os, const StandardMove& obj)
{
    os << chesshelpers::boardIndexToAlgebraicNotation(obj.startSquare) << chesshelpers::boardIndexToAlgebraicNotation(obj.targetSquare);
    if (obj.promotion) {
        char values[4] = { 'n', 'b', 'r', 'q' };
        os << values[obj.promotion - 1];
    }
    return os;
}
