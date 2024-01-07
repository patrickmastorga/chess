#pragma once
#include "ChessPosition.h"
#include "StandardMove.h"
#include <chrono>

/**
 * Standard interface for a chess engine
 */
class StandardEngine : public ChessPosition
{
public:
    /**
     * @return the engine's best move for a the current position
     * @param thinkTime the maximum time the engine is allowed to think for
     */
    virtual StandardMove computerMove(std::chrono::milliseconds thinkTime) = 0;
};