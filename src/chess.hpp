#include <chrono>
#include <string>
#include <vector>

/**
 * Struct for represening a chess move
 */
struct StandardMove
{
    /**
     * Starting square of the move [0, 63] -> [a1, h8]
     */
    int startSquare;

    /**
     * Ending square of the move [0, 63] -> [a1, h8]
     */
    int targetSquare;

    /**
     * In case of promotion, what is the indentity of the promoted peice
     * 0 - none; 1 - knight; 2 - bishop; 3 - rook; 4 - queen
     */
    int promotion;
    
    StandardMove(int start, int target, int promote) : startSquare(start), targetSquare(target), promotion(promote) {}
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
    virtual void loadStartingPosition() = 0;

    /**
     * Loads the specified position into the engine
     * @param fenString string in Forsyth–Edwards Notation
     */
    virtual void loadFEN(std::string &fenString) = 0;

    /**
     * @return std::vector<StandardMove> of legal moves for the current position
     */
    virtual std::vector<StandardMove> generateLegalMoves() = 0;

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
     * @return true if the player who is to move is in check
     */
    virtual bool inCheck() = 0;

    /**
     * @return true if the game has reached a draw
     */
    virtual bool isDraw() = 0;
};