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

typedef std::int_fast8_t int8;
typedef std::uint_fast8_t uint8;
typedef std::int_fast32_t int32;
typedef std::uint_fast32_t uint32;
typedef std::uint_fast64_t uint64;


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
    static constexpr uint8 WHITE = 0b0000;
    static constexpr uint8 BLACK = 0b1000;
    static constexpr uint8 PAWN = 0b001;
    static constexpr uint8 KNIGHT = 0b010;
    static constexpr uint8 BISHOP = 0b011;
    static constexpr uint8 ROOK = 0b100;
    static constexpr uint8 QUEEN = 0b101;
    static constexpr uint8 KING = 0b110;

    // BOARD MEMBERS
    // color and peice type at every square (index [0, 63] -> [a1, h8])
    uint8 peices[64];

    // Legal moves for the current position stored in the engine
    std::vector<StandardMove> currentLegalMoves;

    // List of all moves in game in algebraic notation
    std::list<std::string> gameMovesInAlgebraicNotation;

    // List of all moves in game in algebraic notation
    std::list<StandardMove> gameMoves;

    // total half moves since game start (half move is one player taking a turn)
    uint32 totalHalfmoves;

private:
    // contains the halfmove number when the kingside castling rights were lost for white or black (index 0 and 1)
    bool canKingsideCastle[2];

    // contains the halfmove number when the queenside castling rights were lost for white or black (index 0 and 1)
    bool canQueensideCastle[2];

    // Square over which a pawn double moved over in the previous turn
    uint8 eligibleEnPassantSquare;

    // Used for 50 move rule
    uint8 halfmovesSincePawnMoveOrCapture;

    // 32 bit zobrist hash of current position for checking for repitition
    uint32 positionHistory[50];

    // index of the white and black king (index 0 and 1)
    uint8 kingIndex[2];

    // zobrist hash of the current position
    uint64 zobrist;

    // number of peices on the board for either color and for every peice
    uint8 numPeices[15];

    // number of total on the board for either color 
    uint8 numTotalPeices[2];

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
    bool inCheck(uint8 c) const;

    // return true if inputted pseudo legal move is legal in the current position
    bool isLegal(const StandardMove& move);

    // @param move pseudo legal castling move (castling rights are not lost and king is not in check)
    // @return true if the castling move is legal in the current position
    bool castlingMoveIsLegal(const StandardMove& move);

    std::string getCurrentDate();
};