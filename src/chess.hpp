#ifndef CHESS_H
#define CHESS_H

#include <stdexcept>
#include <chrono>
#include <string>
#include <vector>
#include <optional>
#include <cstdint>

namespace ChessHelpers
{
    /**
     * @param algebraic notation for position on chess board (ex e3, a1, c8)
     * @return uint8 index [0, 63] -> [a1, h8] of square on board
     */
    int algebraicNotationToBoardIndex(const std::string &algebraic)
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

    /**
     * @param boardIndex index [0, 63] -> [a1, h8] of square on board
     * @return std::string notation for position on chess board (ex e3, a1, c8)
     */
    std::string boardIndexToAlgebraicNotation(int boardIndex)
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
}

/**
 * Struct for represening a chess move
 */
struct StandardMove
{
    /**
     * Starting square of the move [0, 63] -> [a1, h8]
     */
    const int startSquare;

    /**
     * Ending square of the move [0, 63] -> [a1, h8]
     */
    const int targetSquare;

    /**
     * In case of promotion, what is the indentity of the promoted peice
     * 0 - none; 1 - knight; 2 - bishop; 3 - rook; 4 - queen
     */
    const int promotion;

    /**
     * Constructor for Standard Move
    */
    StandardMove(int start, int target, int promotion=0) : startSquare(start), targetSquare(target), promotion(promotion) {}

    /**
     * Override equality operator 
     */
    bool operator==(const StandardMove& other) const
    {
        return this->startSquare == other.startSquare
            && this->targetSquare == other.targetSquare
            && this->promotion == other.promotion;
    }

    /**
     * Override stream insertion operator to display info about the move
     */
    friend std::ostream& operator<<(std::ostream& os, const StandardMove &obj)
    {
        os << "(" << ChessHelpers::boardIndexToAlgebraicNotation(obj.startSquare) << " -> " << ChessHelpers::boardIndexToAlgebraicNotation(obj.targetSquare) << ")";
        return os;
    }
};

/**
 * Standard interface for a chess engine
 */
class StandardEngine
{
public:
    /**
     * Loads the starting position into the engine
     */
    virtual void loadStartingPosition() noexcept = 0;

    /**
     * Loads the specified position into the engine
     * @param fenString string in Forsyth–Edwards Notation
     */
    virtual void loadFEN(std::string &fenString) = 0;

    /**
     * @return std::vector<StandardMove> of legal moves for the current position
     */
    virtual std::vector<StandardMove> legalMoves() noexcept = 0;

    /**
     * @return -1 if black is to move, or 1 if white is to move
     */
    virtual int colorToMove() noexcept = 0;

    /**
     * @return the engine's best move for a the current position
     * @param thinkTime the maximum time the engine is allowed to think for
     */
    virtual StandardMove computerMove(std::chrono::milliseconds thinkTime) = 0;

    /**
     * Updates the position stored in the engine with the inputted move
     */
    virtual void inputMove(StandardMove &move) = 0;

    /**
     * @return a value if game is over (-1 if black has won, 0 if forced draw, 1 if white has won)
     */
    virtual std::optional<int> gameOver() noexcept = 0;
    
    /**
     * @return true if the player who is to move is in check
     */
    virtual bool inCheck() const noexcept = 0;

    /**
     * Runs a move generation test on the current position
     * @param printOut if set true, prints a per move readout to the console
     * @return number of total positions a certain depth away
     */
    virtual std::uint64_t perft(int depth, bool printOut=false) noexcept = 0;
};

#endif