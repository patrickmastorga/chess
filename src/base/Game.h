#pragma once
#include "ChessPosition.h"

#include "StandardEngine.h"
#include "PerftTestableEngine.h"
#include "StandardMove.h"

#include <cstdint>
#include <optional>
#include <vector>
#include <list>
#include <unordered_set>
#include <string>
#include <map>

#define GENERATE_ONLY_QUEEN_PROMOTIONS true

/*
IMPLEMENTATION AND STRUCTURE BASED ON EngineV1_1
*/

class Game : public ChessPosition
{
public:
	

public:
    Game(const std::string& fenString);

    Game();

    void loadStartingPosition() override;

    void loadFEN(const std::string& fenString) override;

    std::vector<StandardMove> getLegalMoves() noexcept override;

    int colorToMove() noexcept override;

    void inputMove(const StandardMove& move) override;

    std::optional<int> gameOver() noexcept override;

    bool inCheck() const noexcept override;

    std::string asFEN() const noexcept override;

    /**
    * Returns a string representation of the game in Portable Game Notation
    */
    std::string asPGN(std::map<std::string, std::string> headers={}) noexcept;

protected:
    // DEFINITIONS
    static constexpr std::uint_fast8_t WHITE = 0b0000;
    static constexpr std::uint_fast8_t BLACK = 0b1000;
    static constexpr std::uint_fast8_t PAWN = 0b001;
    static constexpr std::uint_fast8_t KNIGHT = 0b010;
    static constexpr std::uint_fast8_t BISHOP = 0b011;
    static constexpr std::uint_fast8_t ROOK = 0b100;
    static constexpr std::uint_fast8_t QUEEN = 0b101;
    static constexpr std::uint_fast8_t KING = 0b110;

    // BOARD MEMBERS
    // color and peice type at every square (index [0, 63] -> [a1, h8])
    std::uint_fast8_t peices[64];

    // Legal moves for the current position stored in the engine
    std::vector<StandardMove> currentLegalMoves;

    // List of all moves in game in algebraic notation
    std::list<std::string> gameMovesInAlgebraicNotation;

    // List of all moves in game in algebraic notation
    std::list<StandardMove> gameMoves;

    // total half moves since game start (half move is one player taking a turn)
    std::uint_fast32_t totalHalfmoves;

private:
    // contains the halfmove number when the kingside castling rights were lost for white or black (index 0 and 1)
    bool canKingsideCastle[2];

    // contains the halfmove number when the queenside castling rights were lost for white or black (index 0 and 1)
    bool canQueensideCastle[2];

    // Square over which a pawn double moved over in the previous turn
    std::uint_fast8_t eligibleEnPassantSquare;

    // Used for 50 move rule
    std::uint_fast8_t halfmovesSincePawnMoveOrCapture;

    // 32 bit zobrist hash of current position for checking for repitition
    std::uint_fast32_t positionHistory[50];

    // index of the white and black king (index 0 and 1)
    std::uint_fast8_t kingIndex[2];

    // zobrist hash of the current position
    std::uint_fast64_t zobrist;

    // number of peices on the board for either color and for every peice
    std::uint_fast8_t numPeices[15];

    // number of total on the board for either color 
    std::uint_fast8_t numTotalPeices[2];

    // BOARD METHODS
    // Initialize engine members for position
    void initializeFen(const std::string& fenString);

    // Generates legal moves for the current position
    std::vector<StandardMove> legalMoves();

    // returns true if the last move played has led to a draw by threefold repitition
    bool isDrawByThreefoldRepitition() const noexcept;

    // returns true if the last move played has led to a draw by the fifty move rule
    inline bool isDrawByFiftyMoveRule() const noexcept;

    // returns true if there isnt enough material on the board to deliver checkmate
    bool isDrawByInsufficientMaterial() const noexcept;

    // return true if the king belonging to the inputted color is currently being attacked
    bool inCheck(std::uint_fast8_t c) const;

    // return true if inputted pseudo legal move is legal in the current position
    bool isLegal(const StandardMove& move);

    // @param move pseudo legal castling move (castling rights are not lost and king is not in check)
    // @return true if the castling move is legal in the current position
    bool castlingMoveIsLegal(const StandardMove& move);

    std::string getCurrentDate();
};