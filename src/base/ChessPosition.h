#pragma once
#include "StandardMove.h"
#include <string>
#include <vector>
#include <optional>

class ChessPosition
{
public:
    /**
     * Loads the starting position into the engine
     */
    virtual void loadStartingPosition() = 0;

    /**
     * Loads the specified position into the engine
     * @param fenString string in Forsyth–Edwards Notation
     */
    virtual void loadFEN(const std::string& fenString) = 0;

    /**
     * @return std::vector<StandardMove> of legal moves for the current position
     */
    virtual std::vector<StandardMove> getLegalMoves() noexcept = 0;

    /**
     * @return -1 if black is to move, or 1 if white is to move
     */
    virtual int colorToMove() noexcept = 0;

    /**
     * Updates the position stored in the engine with the inputted move
     */
    virtual void inputMove(const StandardMove& move) = 0;

    /**
     * @return a value if game is over (-1 if black has won, 0 if forced draw, 1 if white has won)
     */
    virtual std::optional<int> gameOver() noexcept = 0;

    /**
     * @return true if the player who is to move is in check
     */
    virtual bool inCheck() const noexcept = 0;

    /**
    * @return std::string in Forsyth–Edwards Notation representing the current position
    */
    virtual std::string asFEN() const noexcept = 0;
};