#include "Game.h"

#include <cstdint>
#include <iostream>
#include <iomanip>
#include <optional>
#include <vector>
#include <unordered_set>
#include <algorithm>
#include <stdexcept>
#include <string>
#include <sstream>
#include <cctype>
#include <chrono>

#include "StandardMove.h"
#include "precomputed_chess_data.h"
#include "chesshelpers.h"

typedef std::int_fast8_t int8;
typedef std::uint_fast8_t uint8;
typedef std::int_fast32_t int32;
typedef std::uint_fast32_t uint32;
typedef std::uint_fast64_t uint64;

constexpr int32 MAX_EVAL = INT32_MAX;

// TODO add some sort of protection for games with halfmove counter > 500

// PUBLIC METHODS
Game::Game(const std::string& fenString)
{
    loadFEN(fenString);
}

Game::Game()
{
    loadStartingPosition();
}

void Game::loadStartingPosition()
{
    loadFEN("rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1");
}

void Game::loadFEN(const std::string& fenString)
{
    initializeFen(fenString);
}

std::vector<StandardMove> Game::getLegalMoves() noexcept
{
    std::vector<StandardMove> moves;

    for (Move& move : enginePositionMoves) {
        moves.emplace_back(move.start(), move.target(), move.promotion());
    }

    return moves;
}

int Game::colorToMove() noexcept
{
    return 1 - 2 * (totalHalfmoves % 2);
}

void Game::inputMove(const StandardMove& move)
{
    if (gameOver().has_value()) {
        throw std::runtime_error("Game is over, cannot input move!");
    }

    for (Move& legalMove : enginePositionMoves) {
        if (legalMove == move) {

            makeMove(legalMove);


            // ALGEBRAIC NOTATION
            uint8 c = legalMove.moving() >> 3;
            uint8 color = c << 3;

            std::string algebraic = "";

            // Castling case
            if (legalMove.moving() == color + KING && move.targetSquare - move.startSquare == -2) {
                algebraic = "O-O-O";
                enginePositionMoves = legalMoves();
            }
            else if (legalMove.moving() == color + KING && move.targetSquare - move.startSquare == 2) {
                algebraic = "O-O";
                enginePositionMoves = legalMoves();
            }
            // General Case
            else {
                if (legalMove.moving() == color + PAWN) {
                    if (legalMove.captured()) {
                        algebraic += chesshelpers::boardIndexToAlgebraicNotation(move.startSquare)[0];
                    }
                }
                else {
                    char algebraicPeiceIndentifiers[] = { 'N', 'B', 'R', 'Q', 'K' };
                    algebraic += algebraicPeiceIndentifiers[(legalMove.moving() & 0b111) - 2];

                    // Disambiguation
                    std::vector<uint8> ambiguousSquares;
                    for (Move& otherMove : enginePositionMoves) {
                        if (otherMove.moving() == legalMove.moving() && otherMove.target() == legalMove.target() && otherMove.start() != legalMove.start()) {
                            ambiguousSquares.push_back(otherMove.start());
                        }
                    }

                    if (!ambiguousSquares.empty()) {
                        uint8 file = move.startSquare & 0b111;
                        uint8 rank = move.startSquare >> 3;

                        std::string startSquareAlgebraic = chesshelpers::boardIndexToAlgebraicNotation(move.startSquare);

                        if (std::find_if(ambiguousSquares.begin(), ambiguousSquares.end(), [&](uint8 otherSquare) {return (otherSquare & 0b111) == file;}) == ambiguousSquares.end()) {
                            algebraic += startSquareAlgebraic[0];
                        }
                        else if (std::find_if(ambiguousSquares.begin(), ambiguousSquares.end(), [&](uint8 otherSquare) {return (otherSquare >> 3) == rank;}) == ambiguousSquares.end()) {
                            algebraic += startSquareAlgebraic[1];
                        }
                        else {
                            algebraic += startSquareAlgebraic;
                        }
                    }
                }

                enginePositionMoves = legalMoves();

                if (legalMove.captured()) {
                    algebraic += 'x';
                }

                algebraic += chesshelpers::boardIndexToAlgebraicNotation(move.targetSquare);

                if (move.promotion) {
                    char algebraicPeiceIndentifiers[] = { 'N', 'B', 'R', 'Q' };
                    algebraic += '=';
                    algebraic += algebraicPeiceIndentifiers[move.promotion - 2];
                }

                if (inCheck()) {
                    algebraic += enginePositionMoves.empty() ? '#' : '+';
                }
            }

            gameMoves.push_back(move);
            gameMovesInAlgebraicNotation.push_back(algebraic);

            currentLegalMoves = getLegalMoves();


            // UPDATE POSITION INFO
            if (positionInfoIndex > 51 || positionInfoIndex == 0) {
                throw std::runtime_error("Position info index shouldnt be this high/low!!");
            }

            // Dont reset positionInfo array if move is not a pawn move or capture
            if (!halfMovesSincePawnMoveOrCapture()) {
                positionInfo[0] = positionInfo[positionInfoIndex];
                for (uint8 i = 1; i <= positionInfoIndex; ++i) {
                    positionInfo[i] = 0;
                }
                positionInfoIndex = 0;
            }
            return;
        }
    }

    throw std::runtime_error("inputted move is not legal in the current position!");
}

std::optional<int> Game::gameOver() noexcept
{
    if (isDraw()) {
        return 0;
    }

    if (enginePositionMoves.size() == 0) {
        return inCheck() ? -colorToMove() : 0;
    }

    return std::nullopt;
}

bool Game::inCheck() const noexcept
{
    return inCheck(totalHalfmoves % 2);
}

std::string Game::asFEN() const noexcept
{
    std::string fen = "";
    int32 c = totalHalfmoves % 2;

    // Peice placement data
    char pcs[6] = { 'P', 'N', 'B', 'R', 'Q', 'K' };
    int8 gap = 0;
    for (int8 i = 56; i >= 0; i -= 8) {
        for (int8 j = 0; j < 8; ++j) {
            if (!peices[i + j]) {
                ++gap;
                continue;
            }
            // Add gap charecter if needed
            if (gap) {
                fen += '0' + gap;
                gap = 0;
            }
            // Add peice charecter
            fen += pcs[(peices[i + j] & 0b111) - 1] + 32 * (peices[i + j] >> 3);
        }
        // Add gap charecter if needed
        if (gap) {
            fen += '0' + gap;
            gap = 0;
        }
        // Add rank seperator
        if (i != 0) {
            fen += '/';
        }
    }

    // Player to move
    fen += (c) ? " b " : " w ";

    // Castling availiability
    std::string castlingAvailability = "";
    if (!kingsideCastlingRightsLost[0]) {
        castlingAvailability += 'K';
    }
    if (!queensideCastlingRightsLost[0]) {
        castlingAvailability += 'Q';
    }
    if (!kingsideCastlingRightsLost[1]) {
        castlingAvailability += 'k';
    }
    if (!queensideCastlingRightsLost[1]) {
        castlingAvailability += 'q';
    }
    if (castlingAvailability.size() == 0) {
        fen += "- ";
    }
    else {
        fen += castlingAvailability + " ";
    }

    // En passant target
    if (eligibleEnpassantSquare()) {
        fen += chesshelpers::boardIndexToAlgebraicNotation(eligibleEnpassantSquare()) + " ";
    }
    else {
        fen += "- ";
    }

    // Halfmoves since pawn move or capture
    fen += std::to_string(halfMovesSincePawnMoveOrCapture());
    fen += ' ';

    // Total moves
    fen += std::to_string(totalHalfmoves / 2 + 1);

    return fen;
}

std::string Game::asPGN(std::map<std::string, std::string> headers) noexcept
{
    std::string pgn;

    // Add default headers
    // Event
    if (headers.find("Event") == headers.end()) {
        pgn += "[Event \"??\"]\n";
    }
    else {
        pgn += "[Event \"" + headers["Event"] + "\"]\n";
        headers.erase("Event");
    }

    // Date
    if (headers.find("Date") == headers.end()) {
        pgn += "[Date \"" + getCurrentDate() + "\"]\n";
    }
    else {
        pgn += "[Date \"" + headers["Date"] + "\"]\n";
        headers.erase("Date");
    }

    // White
    if (headers.find("White") == headers.end()) {
        pgn += "[White \"??\"]\n";
    }
    else {
        pgn += "[White \"" + headers["White"] + "\"]\n";
        headers.erase("White");
    }

    // Black
    if (headers.find("Black") == headers.end()) {
        pgn += "[Black \"??\"]\n";
    }
    else {
        pgn += "[Black \"" + headers["Black"] + "\"]\n";
        headers.erase("Black");
    }

    // Termination
    if (headers.find("Termination") == headers.end()) {
        std::string termination = gameOver().has_value() ? "Normal" : "Forfeit";
        pgn += "[Termination \"" + termination + "\"]\n";
    }
    else {
        pgn += "[Termination \"" + headers["Termination"] + "\"]\n";
        headers.erase("Termination");
    }

    // Result
    std::string resultStr;
    if (headers.find("Result") == headers.end()) {
        int result = gameOver().has_value() ? gameOver().value() : -colorToMove();
        resultStr = (result == 1) ? "1-0" : (result == -1) ? "0-1" : "1/2-1/2";
        pgn += "[Result \"" + resultStr + "\"]\n";
    }
    else {
        resultStr = headers["Result"];
        pgn += "[Result \"" + resultStr + "\"]\n";
        headers.erase("Result");
    }

    // other headers
    for (const auto& header : headers) {
        pgn += "[" + header.first + " \"" + header.second + "\"]\n";
    }

    pgn += "\n";

    int i = 0;
    for (const std::string& move : gameMovesInAlgebraicNotation) {
        if (i % 2 == 0) {
            // Add move number before White's move
            pgn += std::to_string(i / 2 + 1) + ". ";
        }
        ++i;
        pgn += move + " ";
    }
    pgn += resultStr + "\n\n";

    return pgn;
}


// MOVE STRUCT
// PUBLIC METHODS
inline uint8 Game::Move::start() const noexcept
{
    return startSquare;
}

inline uint8 Game::Move::target() const noexcept
{
    return targetSquare;
}

inline uint8 Game::Move::moving() const noexcept
{
    return movingPeice;
}

inline uint8 Game::Move::captured() const noexcept
{
    return capturedPeice;
}

inline uint8 Game::Move::color() const noexcept
{
    return (movingPeice >> 3) << 3;
}

inline uint8 Game::Move::enemy() const noexcept
{
    return !(movingPeice >> 3) << 3;
}

inline uint8 Game::Move::promotion() const noexcept
{
    return flags & PROMOTION;
}

inline bool Game::Move::isEnPassant() const noexcept
{
    return flags & EN_PASSANT;
}

inline bool Game::Move::isCastling() const noexcept
{
    return flags & CASTLE;
}

inline bool Game::Move::legalFlagSet() const noexcept
{
    return flags & LEGAL;
}

inline void Game::Move::setLegalFlag() noexcept
{
    flags |= LEGAL;
}

inline void Game::Move::unSetLegalFlag() noexcept
{
    flags |= LEGAL;
    flags ^= LEGAL;
}

inline bool Game::Move::operator==(const Game::Move& other) const
{
    return this->start() == other.start()
        && this->target() == other.target()
        && this->promotion() == other.promotion();
}

inline bool Game::Move::operator==(const StandardMove& other) const
{
    return this->start() == other.startSquare
        && this->target() == other.targetSquare
        && this->promotion() == other.promotion;
}

std::string Game::Move::toString() const
{
    std::string string;
    string += chesshelpers::boardIndexToAlgebraicNotation(start());
    string += chesshelpers::boardIndexToAlgebraicNotation(target());
    if (promotion()) {
        char values[4] = { 'n', 'b', 'r', 'q' };
        string += values[promotion() - 2];
    }
    return string;
}

// CONSTRUCTORS
Game::Move::Move(const Game* board, uint8 start, uint8 target, uint8 givenFlags) : startSquare(start), targetSquare(target), flags(givenFlags)
{
    movingPeice = board->peices[start];
    capturedPeice = board->peices[target];
    if (isEnPassant()) {
        capturedPeice = enemy() + PAWN;
    }
}

Game::Move::Move() : startSquare(0), targetSquare(0), movingPeice(0), capturedPeice(0), flags(0) {}

// END MOVE STRUCT

// BOARD METHODS
void Game::initializeFen(const std::string& fenString)
{
    // Reset current members
    zobrist = 0;
    for (int i = 0; i < 15; ++i) {
        numPeices[i] = 0;
    }
    numTotalPeices[0] = 0;
    numTotalPeices[1] = 0;

    for (uint8 i = 0; i < 32 + 50; ++i) {
        positionInfo[i] = 0;
    }

    gameMoves.clear();
    gameMovesInAlgebraicNotation.clear();

    std::istringstream fenStringStream(fenString);
    std::string peicePlacementData, activeColor, castlingAvailabilty, enPassantTarget, halfmoveClock, fullmoveNumber;

    // Get peice placement data from fen string
    if (!std::getline(fenStringStream, peicePlacementData, ' ')) {
        throw std::invalid_argument("Cannot get peice placement from FEN!");
    }

    // Update the peices[] according to fen rules
    int8 peiceIndex = 56;
    for (char peiceInfo : peicePlacementData) {
        if (std::isalpha(peiceInfo)) {
            // char contains data about a peice
            //int32 color = std::islower(c);
            uint8 c = peiceInfo > 96 && peiceInfo < 123;
            uint8 color = c << 3;

            switch (peiceInfo) {
            case 'P':
            case 'p':
                peices[peiceIndex++] = color + PAWN;
                break;
            case 'N':
            case 'n':
                peices[peiceIndex++] = color + KNIGHT;
                break;
            case 'B':
            case 'b':
                peices[peiceIndex++] = color + BISHOP;
                break;
            case 'R':
            case 'r':
                peices[peiceIndex++] = color + ROOK;
                break;
            case 'Q':
            case 'q':
                peices[peiceIndex++] = color + QUEEN;
                break;
            case 'K':
            case 'k':
                kingIndex[color >> 3] = peiceIndex;
                peices[peiceIndex++] = color + KING;
                break;
            default:
                throw std::invalid_argument("Unrecognised alpha char in FEN peice placement data!");
            }

        }
        else if (std::isdigit(peiceInfo)) {
            // char contains data about gaps between peices
            uint8 gap = peiceInfo - '0';
            for (uint8 i = 0; i < gap; ++i) {
                peices[peiceIndex++] = 0;
            }

        }
        else {
            if (peiceInfo != '/') {
                // Only "123456789pnbrqkPNBRQK/" are allowed in peice placement data
                throw std::invalid_argument("Unrecognised char in FEN peice placement data!");
            }

            if (peiceIndex % 8 != 0) {
                // Values between '/' should add up to 8
                throw std::invalid_argument("Arithmetic error in FEN peice placement data!");
            }

            // Move peice index to next rank
            peiceIndex -= 16;
        }
    }

    // Get active color data from fen string
    if (!std::getline(fenStringStream, activeColor, ' ')) {
        throw std::invalid_argument("Cannot get active color from FEN!");
    }

    if (activeColor == "w") {
        // White is to move
        totalHalfmoves = 0;

    }
    else if (activeColor == "b") {
        // Black is to move
        totalHalfmoves = 1;
        zobrist ^= ZOBRIST_TURN_KEY;

    }
    else {
        // active color can only be "wb"
        throw std::invalid_argument("Unrecognised charecter in FEN active color");
    }

    // Get castling availability data from fen string
    if (!std::getline(fenStringStream, castlingAvailabilty, ' ')) {
        throw std::invalid_argument("Cannot get castling availability from FEN!");
    }

    // Update castling availibility according to fen rules
    kingsideCastlingRightsLost[0] = -1;
    kingsideCastlingRightsLost[1] = -1;
    queensideCastlingRightsLost[0] = -1;
    queensideCastlingRightsLost[1] = -1;

    if (castlingAvailabilty != "-") {
        for (char castlingInfo : castlingAvailabilty) {
            //int32 color = std::islower(c);
            uint8 c = castlingInfo > 96 && castlingInfo < 123;
            uint8 color = c << 3;
            uint8 castlingRank = 56 * c;
            switch (castlingInfo) {
            case 'K':
            case 'k':
                if (peices[castlingRank + 4] == color + KING && peices[castlingRank + 7] == color + ROOK) {
                    kingsideCastlingRightsLost[c] = 0;
                    zobrist ^= ZOBRIST_KINGSIDE_CASTLING_KEYS[c];
                }
                break;
            case 'Q':
            case 'q':
                if (peices[castlingRank + 4] == color + KING && peices[castlingRank] == color + ROOK) {
                    queensideCastlingRightsLost[c] = 0;
                    zobrist ^= ZOBRIST_QUEENSIDE_CASTLING_KEYS[c];
                }
                break;
            default:
                throw std::invalid_argument("Unrecognised char in FEN castling availability data!");
            }
        }
    }

    // Get en passant target data from fen string
    if (!std::getline(fenStringStream, enPassantTarget, ' ')) {
        throw std::invalid_argument("Cannot get en passant target from FEN!");
    }

    // Get half move clock data from fen string
    if (!std::getline(fenStringStream, halfmoveClock, ' ')) {
        halfmoveClock = "0";
        //throw std::invalid_argument("Cannot get halfmove clock from FEN!");
    }

    try {
        positionInfoIndex = static_cast<uint8>(std::stoi(halfmoveClock));
        positionInfo[positionInfoIndex] |= static_cast<uint32>(positionInfoIndex) << 20;
    }
    catch (const std::invalid_argument& e) {
        throw std::invalid_argument(std::string("Invalid FEN half move clock! ") + e.what());
    }

    // wait until halfmove clock are defined before defining
    if (enPassantTarget != "-") {
        try {
            positionInfo[positionInfoIndex] |= static_cast<uint32>(chesshelpers::algebraicNotationToBoardIndex(enPassantTarget)) << 26;
        }
        catch (const std::invalid_argument& e) {
            throw std::invalid_argument(std::string("Invalid FEN en passant target! ") + e.what());
        }

    }

    // Get full move number data from fen string
    if (!std::getline(fenStringStream, fullmoveNumber, ' ')) {
        fullmoveNumber = "1";
        //throw std::invalid_argument("Cannot getfullmove number from FEN!");
    }

    try {
        totalHalfmoves += static_cast<uint32>(std::stoi(fullmoveNumber) * 2 - 2);
    }
    catch (const std::invalid_argument& e) {
        throw std::invalid_argument(std::string("Invalid FEN full move number! ") + e.what());
    }

    // initialize zobrist hash for all of the peices
    for (uint8 i = 0; i < 64; ++i) {
        uint8 peice = peices[i];
        if (peice) {
            zobrist ^= ZOBRIST_PEICE_KEYS[peice >> 3][(peice & 0b111) - 1][i];
            ++numPeices[peice];
            ++numTotalPeices[peice >> 3];
        }
    }

    positionInfo[positionInfoIndex] |= zobrist >> 44;

    uint8 c = totalHalfmoves % 2;

    enginePositionMoves = legalMoves();
    currentLegalMoves = getLegalMoves();
}


bool Game::generatePseudoLegalMoves(Move* stack, uint32& idx) noexcept
{
    // TODO Backwards check/pin generation for endgame
    // TODO Better pinned peice generation (detect if sliding along diagonal of pin)

    uint8 c = totalHalfmoves % 2;
    uint8 color = c << 3;
    uint8 e = !color;
    uint8 enemy = e << 3;

    // Used for optimizing the legality checking of moves
    bool isPinned[64] = { 0 };
    std::unordered_set<uint8> checkingSquares(11);

    // Check if king in check and record pinned peices and checking squares
    uint8 king = kingIndex[c];
    uint8 checks = 0;

    // Pawn checks
    if (numPeices[enemy + PAWN]) {
        uint8 kingFile = king % 8;
        uint8 kingAhead = king + 8 - 16 * c;
        if (kingFile != 0 && peices[kingAhead - 1] == enemy + PAWN) {
            checkingSquares.insert(kingAhead - 1);
            ++checks;
        }
        if (kingFile != 7 && peices[kingAhead + 1] == enemy + PAWN) {
            checkingSquares.insert(kingAhead + 1);
            ++checks;
        }
    }

    // Knight checks
    if (numPeices[enemy + KNIGHT]) {
        for (uint8 j = 1; j < KNIGHT_MOVES[king][0]; ++j) {
            if (peices[KNIGHT_MOVES[king][j]] == enemy + KNIGHT) {
                checkingSquares.insert(KNIGHT_MOVES[king][j]);
                ++checks;
            }
        }
    }

    // pins and sliding peice checks
    uint8 potentialPin = 0;
    // Dont bother checking horizontals if no bishops or queens
    if (numPeices[enemy + ROOK] | numPeices[enemy + QUEEN]) {
        for (int8 j = king - 8; j >= DIRECTION_BOUNDS[king][B]; j -= 8) {
            if (!peices[j]) {
                continue;
            }
            if (!potentialPin && peices[j] >> 3 == c) {
                potentialPin = j;
                continue;
            }
            if (peices[j] == enemy + ROOK || peices[j] == enemy + QUEEN) {
                if (potentialPin) {
                    isPinned[potentialPin] = true;
                    break;
                }
                if (checks++) { // dont need to record checking squares/pins for double checks
                    goto moveGeneration;
                }
                for (int8 k = j; k < king; k += 8) {
                    checkingSquares.insert(k);
                }
            }
            break;
        }

        potentialPin = 0;
        for (int8 j = king + 8; j <= DIRECTION_BOUNDS[king][F]; j += 8) {
            if (!peices[j]) {
                continue;
            }
            if (!potentialPin && peices[j] >> 3 == c) {
                potentialPin = j;
                continue;
            }
            if (peices[j] == enemy + ROOK || peices[j] == enemy + QUEEN) {
                if (potentialPin) {
                    isPinned[potentialPin] = true;
                    break;
                }
                if (checks++) { // dont need to record checking squares/pins for double checks
                    goto moveGeneration;
                }
                for (int8 k = j; k > king; k -= 8) {
                    checkingSquares.insert(k);
                }
            }
            break;
        }

        potentialPin = 0;
        for (int8 j = king - 1; j >= DIRECTION_BOUNDS[king][L]; j -= 1) {
            if (!peices[j]) {
                continue;
            }
            if (!potentialPin && peices[j] >> 3 == c) {
                potentialPin = j;
                continue;
            }
            if (peices[j] == enemy + ROOK || peices[j] == enemy + QUEEN) {
                if (potentialPin) {
                    isPinned[potentialPin] = true;
                    break;
                }
                if (checks++) { // dont need to record checking squares/pins for double checks
                    goto moveGeneration;
                }
                for (int8 k = j; k < king; k += 1) {
                    checkingSquares.insert(k);
                }
            }
            break;
        }

        potentialPin = 0;
        for (int8 j = king + 1; j <= DIRECTION_BOUNDS[king][R]; j += 1) {
            if (!peices[j]) {
                continue;
            }
            if (!potentialPin && peices[j] >> 3 == c) {
                potentialPin = j;
                continue;
            }
            if (peices[j] == enemy + ROOK || peices[j] == enemy + QUEEN) {
                if (potentialPin) {
                    isPinned[potentialPin] = true;
                    break;
                }
                if (checks++) { // dont need to record checking squares/pins for double checks
                    goto moveGeneration;
                }
                for (int8 k = j; k > king; k -= 1) {
                    checkingSquares.insert(k);
                }
            }
            break;
        }
    }

    // Dont bother checking diagonals if no bishops or queens
    if (numPeices[enemy + BISHOP] | numPeices[enemy + QUEEN]) {
        potentialPin = 0;
        for (int8 j = king - 9; j >= DIRECTION_BOUNDS[king][BL]; j -= 9) {
            if (!peices[j]) {
                continue;
            }
            if (!potentialPin && peices[j] >> 3 == c) {
                potentialPin = j;
                continue;
            }
            if (peices[j] == enemy + BISHOP || peices[j] == enemy + QUEEN) {
                if (potentialPin) {
                    isPinned[potentialPin] = true;
                    break;
                }
                if (checks++) { // dont need to record checking squares/pins for double checks
                    goto moveGeneration;
                }
                for (int8 k = j; k < king; k += 9) {
                    checkingSquares.insert(k);
                }
            }
            break;
        }

        potentialPin = 0;
        for (int8 j = king + 9; j <= DIRECTION_BOUNDS[king][FR]; j += 9) {
            if (!peices[j]) {
                continue;
            }
            if (!potentialPin && peices[j] >> 3 == c) {
                potentialPin = j;
                continue;
            }
            if (peices[j] == enemy + BISHOP || peices[j] == enemy + QUEEN) {
                if (potentialPin) {
                    isPinned[potentialPin] = true;
                    break;
                }
                if (checks++) { // dont need to record checking squares/pins for double checks
                    goto moveGeneration;
                }
                for (int8 k = j; k > king; k -= 9) {
                    checkingSquares.insert(k);
                }
            }
            break;
        }

        potentialPin = 0;
        for (int8 j = king - 7; j >= DIRECTION_BOUNDS[king][BR]; j -= 7) {
            if (!peices[j]) {
                continue;
            }
            if (!potentialPin && peices[j] >> 3 == c) {
                potentialPin = j;
                continue;
            }
            if (peices[j] == enemy + BISHOP || peices[j] == enemy + QUEEN) {
                if (potentialPin) {
                    isPinned[potentialPin] = true;
                    break;
                }
                if (checks++) { // dont need to record checking squares/pins for double checks
                    goto moveGeneration;
                }
                for (int8 k = j; k < king; k += 7) {
                    checkingSquares.insert(k);
                }
            }
            break;
        }

        potentialPin = 0;
        for (int8 j = king + 7; j <= DIRECTION_BOUNDS[king][FL]; j += 7) {
            if (!peices[j]) {
                continue;
            }
            if (!potentialPin && peices[j] >> 3 == c) {
                potentialPin = j;
                continue;
            }
            if (peices[j] == enemy + BISHOP || peices[j] == enemy + QUEEN) {
                if (potentialPin) {
                    isPinned[potentialPin] = true;
                    break;
                }
                if (checks++) { // dont need to record checking squares/pins for double checks
                    goto moveGeneration;
                }
                for (int8 k = j; k > king; k -= 7) {
                    checkingSquares.insert(k);
                }
            }
            break;
        }
    }

moveGeneration:
    // Double check; Only king moves are legal
    if (checks > 1) {
        // Generate king moves
        for (uint8 j = 1; j < KING_MOVES[king][0]; ++j) {
            if ((!peices[KING_MOVES[king][j]] && !checkingSquares.count(KING_MOVES[king][j])) || peices[KING_MOVES[king][j]] >> 3 == e) {
                stack[idx++] = Move(this, king, KING_MOVES[king][j], Move::NONE);
            }
        }
        return true;
    }

    // En passant moves
    uint8 epSquare = eligibleEnpassantSquare();
    if (epSquare) {
        uint8 epfile = epSquare % 8;
        if (color == WHITE) {
            if (epfile != 0 && peices[epSquare - 9] == color + PAWN && (!checks || checkingSquares.count(epSquare - 8))) {
                stack[idx++] = Move(this, epSquare - 9, epSquare, Move::EN_PASSANT);
            }
            if (epfile != 7 && peices[epSquare - 7] == color + PAWN && (!checks || checkingSquares.count(epSquare - 8))) {
                stack[idx++] = Move(this, epSquare - 7, epSquare, Move::EN_PASSANT);
            }
        }
        else {
            if (epfile != 0 && peices[epSquare + 7] == color + PAWN && (!checks || checkingSquares.count(epSquare + 8))) {
                stack[idx++] = Move(this, epSquare + 7, epSquare, Move::EN_PASSANT);
            }
            if (epfile != 7 && peices[epSquare + 9] == color + PAWN && (!checks || checkingSquares.count(epSquare + 8))) {
                stack[idx++] = Move(this, epSquare + 9, epSquare, Move::EN_PASSANT);
            }
        }
    }

    // Special move generation for when few number of checking squares
    if (checks && checkingSquares.size() < 4) {
        // Generate king moves
        for (uint8 j = 1; j < KING_MOVES[king][0]; ++j) {
            if ((!peices[KING_MOVES[king][j]] && !checkingSquares.count(KING_MOVES[king][j])) || peices[KING_MOVES[king][j]] >> 3 == e) {
                stack[idx++] = Move(this, king, KING_MOVES[king][j], Move::NONE);
            }
        }

        // Backwards search from checking squares to see if peices can move to them
        for (uint8 t : checkingSquares) {

            // Pawn can block/take
            if (numPeices[color + PAWN]) {
                if ((color == WHITE && t >> 3 >= 2) || (color == BLACK && t >> 3 <= 5)) {
                    uint8 file = t % 8;
                    uint8 ahead = t - 8 + 16 * c;
                    bool promotion = t >> 3 == 0 || t >> 3 == 7;
                    // Pawn capture
                    if (peices[t] && peices[t] >> 3 == e) {
                        if (file != 0 && peices[ahead - 1] == color + PAWN && !isPinned[ahead - 1]) {
                            if (promotion) {
                                stack[idx++] = Move(this, ahead - 1, t, KNIGHT | Move::LEGAL);
                                stack[idx++] = Move(this, ahead - 1, t, BISHOP | Move::LEGAL);
                                stack[idx++] = Move(this, ahead - 1, t, ROOK | Move::LEGAL);
                                stack[idx++] = Move(this, ahead - 1, t, QUEEN | Move::LEGAL);
                            }
                            else {
                                stack[idx++] = Move(this, ahead - 1, t, Move::LEGAL);
                            }
                        }
                        if (file != 7 && peices[ahead + 1] == color + PAWN && !isPinned[ahead + 1]) {
                            if (promotion) {
                                stack[idx++] = Move(this, ahead + 1, t, KNIGHT | Move::LEGAL);
                                stack[idx++] = Move(this, ahead + 1, t, BISHOP | Move::LEGAL);
                                stack[idx++] = Move(this, ahead + 1, t, ROOK | Move::LEGAL);
                                stack[idx++] = Move(this, ahead + 1, t, QUEEN | Move::LEGAL);
                            }
                            else {
                                stack[idx++] = Move(this, ahead + 1, t, Move::LEGAL);
                            }
                        }
                        // Pawn move
                    }
                    else if (!peices[t]) {
                        int8 doubleAhead = ahead - 8 + 16 * c;
                        if (peices[ahead] == color + PAWN && !isPinned[ahead]) {
                            if (promotion) {
                                stack[idx++] = Move(this, ahead, t, KNIGHT | Move::LEGAL);
                                stack[idx++] = Move(this, ahead, t, BISHOP | Move::LEGAL);
                                stack[idx++] = Move(this, ahead, t, ROOK | Move::LEGAL);
                                stack[idx++] = Move(this, ahead, t, QUEEN | Move::LEGAL);
                            }
                            else {
                                stack[idx++] = Move(this, ahead, t, Move::LEGAL);
                            }

                        }
                        else if ((doubleAhead >> 3 == 1 || doubleAhead >> 3 == 6) && !peices[ahead] && peices[doubleAhead] == color + PAWN && !isPinned[doubleAhead]) {
                            stack[idx++] = Move(this, doubleAhead, t, Move::LEGAL);
                        }
                    }
                }
            }

            // Knight can block/take
            if (numPeices[color + KNIGHT]) {
                for (uint8 j = 1; j < KNIGHT_MOVES[t][0]; ++j) {
                    if (peices[KNIGHT_MOVES[t][j]] == color + KNIGHT && !isPinned[KNIGHT_MOVES[t][j]]) {
                        stack[idx++] = Move(this, KNIGHT_MOVES[t][j], t, Move::LEGAL);
                    }
                }
            }

            // Sliding peices can block/take
            if (numPeices[color + ROOK] | numPeices[color + QUEEN]) {
                for (int8 s = t - 8; s >= DIRECTION_BOUNDS[t][B]; s -= 8) {
                    if (peices[s]) {
                        if ((peices[s] == color + ROOK || peices[s] == color + QUEEN) && !isPinned[s]) {
                            stack[idx++] = Move(this, s, t, Move::LEGAL);
                        }
                        break;
                    }
                }

                for (int8 s = t + 8; s <= DIRECTION_BOUNDS[t][F]; s += 8) {
                    if (peices[s]) {
                        if ((peices[s] == color + ROOK || peices[s] == color + QUEEN) && !isPinned[s]) {
                            stack[idx++] = Move(this, s, t, Move::LEGAL);
                        }
                        break;
                    }
                }

                for (int8 s = t - 1; s >= DIRECTION_BOUNDS[t][L]; s -= 1) {
                    if (peices[s]) {
                        if ((peices[s] == color + ROOK || peices[s] == color + QUEEN) && !isPinned[s]) {
                            stack[idx++] = Move(this, s, t, Move::LEGAL);
                        }
                        break;
                    }
                }

                for (int8 s = t + 1; s <= DIRECTION_BOUNDS[t][R]; s += 1) {
                    if (peices[s]) {
                        if ((peices[s] == color + ROOK || peices[s] == color + QUEEN) && !isPinned[s]) {
                            stack[idx++] = Move(this, s, t, Move::LEGAL);
                        }
                        break;
                    }
                }
            }

            if (numPeices[color + BISHOP] | numPeices[color + QUEEN]) {
                for (int8 s = t - 9; s >= DIRECTION_BOUNDS[t][BL]; s -= 9) {
                    if (peices[s]) {
                        if ((peices[s] == color + BISHOP || peices[s] == color + QUEEN) && !isPinned[s]) {
                            stack[idx++] = Move(this, s, t, Move::LEGAL);
                        }
                        break;
                    }
                }

                for (int8 s = t + 9; s <= DIRECTION_BOUNDS[t][FR]; s += 9) {
                    if (peices[s]) {
                        if ((peices[s] == color + BISHOP || peices[s] == color + QUEEN) && !isPinned[s]) {
                            stack[idx++] = Move(this, s, t, Move::LEGAL);
                        }
                        break;
                    }
                }

                for (int8 s = t - 7; s >= DIRECTION_BOUNDS[t][BR]; s -= 7) {
                    if (peices[s]) {
                        if ((peices[s] == color + BISHOP || peices[s] == color + QUEEN) && !isPinned[s]) {
                            stack[idx++] = Move(this, s, t, Move::LEGAL);
                        }
                        break;
                    }
                }

                for (int8 s = t + 7; s <= DIRECTION_BOUNDS[t][FL]; s += 7) {
                    if (peices[s]) {
                        if ((peices[s] == color + BISHOP || peices[s] == color + QUEEN) && !isPinned[s]) {
                            stack[idx++] = Move(this, s, t, Move::LEGAL);
                        }
                        break;
                    }
                }
            }
        }
        return true;
    }

    // Castling
    if (!kingsideCastlingRightsLost[c] && !checks) {
        uint8 castlingRank = 56 * c;
        bool roomToCastle = true;
        for (uint8 j = castlingRank + 5; j < castlingRank + 7; ++j) {
            if (peices[j]) {
                roomToCastle = false;
                break;
            }
        }
        if (roomToCastle) {
            stack[idx++] = Move(this, castlingRank + 4, castlingRank + 6, Move::CASTLE);
        }
    }
    if (!queensideCastlingRightsLost[c] && !checks) {
        uint8 castlingRank = 56 * c;
        bool roomToCastle = true;
        for (uint8 j = castlingRank + 3; j > castlingRank; --j) {
            if (peices[j]) {
                roomToCastle = false;
                break;
            }
        }
        if (roomToCastle) {
            stack[idx++] = Move(this, castlingRank + 4, castlingRank + 2, Move::CASTLE);
        }
    }

    // General case
    for (uint8 s = 0; s < 64; ++s) {
        if (peices[s] && peices[s] >> 3 == c) {
            uint8 legalFlag = isPinned[s] ? Move::NONE : Move::LEGAL;

            switch (peices[s] & 0b111) {
            case PAWN: {
                uint8 file = s % 8;
                uint8 ahead = s + 8 - 16 * c;
                bool promotion = color == WHITE ? (s >> 3 == 6) : (s >> 3 == 1);

                // Pawn foward moves
                if (!peices[ahead]) {
                    if (!checks || checkingSquares.count(ahead)) {
                        if (promotion) {
                            stack[idx++] = Move(this, s, ahead, legalFlag | KNIGHT);
                            stack[idx++] = Move(this, s, ahead, legalFlag | BISHOP);
                            stack[idx++] = Move(this, s, ahead, legalFlag | ROOK);
                            stack[idx++] = Move(this, s, ahead, legalFlag | QUEEN);
                        }
                        else {
                            stack[idx++] = Move(this, s, ahead, legalFlag);
                        }
                    }

                    bool doubleJumpAllowed = color == WHITE ? (s >> 3 == 1) : (s >> 3 == 6);
                    int8 doubleAhead = ahead + 8 - 16 * c;
                    if (doubleJumpAllowed && !peices[doubleAhead] && (!checks || checkingSquares.count(doubleAhead))) {
                        stack[idx++] = Move(this, s, doubleAhead, legalFlag);
                    }
                }

                // Pawn captures
                if (file != 0 && peices[ahead - 1] && peices[ahead - 1] >> 3 == e && (!checks || checkingSquares.count(ahead - 1))) {
                    if (promotion) {
                        stack[idx++] = Move(this, s, ahead - 1, legalFlag | KNIGHT);
                        stack[idx++] = Move(this, s, ahead - 1, legalFlag | BISHOP);
                        stack[idx++] = Move(this, s, ahead - 1, legalFlag | ROOK);
                        stack[idx++] = Move(this, s, ahead - 1, legalFlag | QUEEN);
                    }
                    else {
                        stack[idx++] = Move(this, s, ahead - 1, legalFlag);
                    }
                }
                if (file != 7 && peices[ahead + 1] && peices[ahead + 1] >> 3 == e && (!checks || checkingSquares.count(ahead + 1))) {
                    if (promotion) {
                        stack[idx++] = Move(this, s, ahead + 1, legalFlag | KNIGHT);
                        stack[idx++] = Move(this, s, ahead + 1, legalFlag | BISHOP);
                        stack[idx++] = Move(this, s, ahead + 1, legalFlag | ROOK);
                        stack[idx++] = Move(this, s, ahead + 1, legalFlag | QUEEN);
                    }
                    else {
                        stack[idx++] = Move(this, s, ahead + 1, legalFlag);
                    }
                }
                break;
            }
            case KNIGHT: {
                uint8 t;
                for (uint8 j = 1; j < KNIGHT_MOVES[s][0]; ++j) {
                    t = KNIGHT_MOVES[s][j];
                    if ((!peices[t] || peices[t] >> 3 == e) && (!checks || checkingSquares.count(t))) {
                        stack[idx++] = Move(this, s, t, legalFlag);
                    }
                }
                break;
            }
            case BISHOP:
            case ROOK:
            case QUEEN: {
                if ((peices[s] & 0b111) == BISHOP) {
                    goto bishopMoves;
                }

                for (int8 t = s - 8; t >= DIRECTION_BOUNDS[s][B]; t -= 8) {
                    if ((!peices[t] || peices[t] >> 3 == e) && (!checks || checkingSquares.count(t))) {
                        stack[idx++] = Move(this, s, t, legalFlag);
                    }
                    if (peices[t]) {
                        break;
                    }
                }

                for (int8 t = s + 8; t <= DIRECTION_BOUNDS[s][F]; t += 8) {
                    if ((!peices[t] || peices[t] >> 3 == e) && (!checks || checkingSquares.count(t))) {
                        stack[idx++] = Move(this, s, t, legalFlag);
                    }
                    if (peices[t]) {
                        break;
                    }
                }

                for (int8 t = s - 1; t >= DIRECTION_BOUNDS[s][L]; t -= 1) {
                    if ((!peices[t] || peices[t] >> 3 == e) && (!checks || checkingSquares.count(t))) {
                        stack[idx++] = Move(this, s, t, legalFlag);
                    }
                    if (peices[t]) {
                        break;
                    }
                }

                for (int8 t = s + 1; t <= DIRECTION_BOUNDS[s][R]; t += 1) {
                    if ((!peices[t] || peices[t] >> 3 == e) && (!checks || checkingSquares.count(t))) {
                        stack[idx++] = Move(this, s, t, legalFlag);
                    }
                    if (peices[t]) {
                        break;
                    }
                }

                if ((peices[s] & 0b111) == ROOK) {
                    break;
                }
            bishopMoves:

                for (int8 t = s - 9; t >= DIRECTION_BOUNDS[s][BL]; t -= 9) {
                    if ((!peices[t] || peices[t] >> 3 == e) && (!checks || checkingSquares.count(t))) {
                        stack[idx++] = Move(this, s, t, legalFlag);
                    }
                    if (peices[t]) {
                        break;
                    }
                }

                for (int8 t = s + 9; t <= DIRECTION_BOUNDS[s][FR]; t += 9) {
                    if ((!peices[t] || peices[t] >> 3 == e) && (!checks || checkingSquares.count(t))) {
                        stack[idx++] = Move(this, s, t, legalFlag);
                    }
                    if (peices[t]) {
                        break;
                    }
                }

                for (int8 t = s - 7; t >= DIRECTION_BOUNDS[s][BR]; t -= 7) {
                    if ((!peices[t] || peices[t] >> 3 == e) && (!checks || checkingSquares.count(t))) {
                        stack[idx++] = Move(this, s, t, legalFlag);
                    }
                    if (peices[t]) {
                        break;
                    }
                }

                for (int8 t = s + 7; t <= DIRECTION_BOUNDS[s][FL]; t += 7) {
                    if ((!peices[t] || peices[t] >> 3 == e) && (!checks || checkingSquares.count(t))) {
                        stack[idx++] = Move(this, s, t, legalFlag);
                    }
                    if (peices[t]) {
                        break;
                    }
                }

                break;
            }
            case KING: {
                uint8 t;
                for (uint8 j = 1; j < KING_MOVES[s][0]; ++j) {
                    t = KING_MOVES[s][j];
                    if (!peices[t] || peices[t] >> 3 == e) {
                        stack[idx++] = Move(this, s, t, Move::NONE);
                    }
                }
            }
            }
        }
    }
    return checks;
}

std::vector<Game::Move> Game::legalMoves()
{
    Move moves[225];
    uint32 end = 0;
    generatePseudoLegalMoves(moves, end);

    std::vector<Move> legalMoves;

    for (uint32 i = 0; i < end; ++i) {
        if (isLegal(moves[i])) {
            legalMoves.push_back(moves[i]);
        }
    }

    return legalMoves;
}

bool Game::makeMove(Game::Move& move)
{
    uint8 c = move.moving() >> 3;
    uint8 color = c << 3;
    uint8 e = !color;
    uint8 enemy = e << 3;

    // CHECK LEGALITY
    // Seperately check legality of castling moves
    if (move.isCastling() && !move.legalFlagSet()) {
        if (!castlingMoveIsLegal(move)) {
            return false;
        }
    }

    // Update peices array first to check legality
    peices[move.start()] = 0;
    peices[move.target()] = move.promotion() ? color + move.promotion() : move.moving();
    if (move.isEnPassant()) {
        peices[move.target() - 8 + 16 * c] = 0;
    }

    // Update king index
    if ((move.moving() & 0b111) == KING) {
        kingIndex[c] = move.target();
    }

    // Check if move was illegal
    if (!move.legalFlagSet() && inCheck(c)) {
        // Undo change
        peices[move.start()] = move.moving();
        peices[move.target()] = move.captured();
        if (move.isEnPassant()) {
            peices[move.target()] = 0;
            peices[move.target() - 8 + 16 * c] = move.captured();
        }
        // Undo king index
        if ((move.moving() & 0b111) == KING) {
            kingIndex[c] = move.start();
        }
        return false;
    }

    move.setLegalFlag();

    // UPDATE PEICE DATA / ZOBRIST HASH
    // Update zobrist hash for turn change
    zobrist ^= ZOBRIST_TURN_KEY;

    // Update zobrist hash, numpieces and positonal imbalance for moving peice
    zobrist ^= ZOBRIST_PEICE_KEYS[c][(move.moving() & 0b111) - 1][move.start()];
    if (move.promotion()) {
        zobrist ^= ZOBRIST_PEICE_KEYS[c][move.promotion() - 1][move.target()];
        --numPeices[move.moving()];
        ++numPeices[color + move.promotion()];
    }
    else {
        zobrist ^= ZOBRIST_PEICE_KEYS[c][(move.moving() & 0b111) - 1][move.target()];
    }


    // Update zobrist hash and peice indices set for capture
    if (move.captured()) {
        uint8 captureSquare = move.isEnPassant() ? move.target() - 8 + 16 * c : move.target();
        zobrist ^= ZOBRIST_PEICE_KEYS[e][(move.captured() & 0b111) - 1][captureSquare];
        --numPeices[move.captured()];
        --numTotalPeices[e];
    }

    // Update rooks for castling
    if (move.isCastling()) {
        uint8 rookStart;
        uint8 rookEnd;

        uint8 castlingRank = move.target() & 0b11111000;
        if (move.target() % 8 < 4) {
            // Queenside castling
            rookStart = castlingRank;
            rookEnd = castlingRank + 3;
        }
        else {
            // Kingside castling
            rookStart = castlingRank + 7;
            rookEnd = castlingRank + 5;
        }

        peices[rookEnd] = peices[rookStart];
        peices[rookStart] = 0;
        zobrist ^= ZOBRIST_PEICE_KEYS[c][ROOK - 1][rookStart];
        zobrist ^= ZOBRIST_PEICE_KEYS[c][ROOK - 1][rookEnd];
    }

    // UPDATE BOARD FLAGS
    // increment counters / Update position history
    ++totalHalfmoves;

    if (!move.captured() && move.moving() != color + PAWN) {
        positionInfo[positionInfoIndex + 1] |= static_cast<uint32>(halfMovesSincePawnMoveOrCapture() + 1) << 20;
    }
    ++positionInfoIndex;

    // En passant file
    if ((move.moving() & 0b111) == PAWN && std::abs(move.target() - move.start()) == 16) {
        positionInfo[positionInfoIndex] |= static_cast<uint32>((move.start() + move.target()) / 2) << 26;
    }

    // update castling rights
    if (!kingsideCastlingRightsLost[c]) {
        if (move.moving() == color + KING || (move.moving() == color + ROOK && (color == WHITE ? move.start() == 7 : move.start() == 63))) {
            kingsideCastlingRightsLost[c] = totalHalfmoves;
            zobrist ^= ZOBRIST_KINGSIDE_CASTLING_KEYS[c];
        }
    }
    if (!queensideCastlingRightsLost[c]) {
        if (move.moving() == color + KING || (move.moving() == color + ROOK && (color == WHITE ? move.start() == 0 : move.start() == 56))) {
            queensideCastlingRightsLost[c] = totalHalfmoves;
            zobrist ^= ZOBRIST_QUEENSIDE_CASTLING_KEYS[c];
        }
    }
    if (!kingsideCastlingRightsLost[e]) {
        if (color == BLACK ? move.target() == 7 : move.target() == 63) {
            kingsideCastlingRightsLost[e] = totalHalfmoves;
            zobrist ^= ZOBRIST_KINGSIDE_CASTLING_KEYS[e];
        }
    }
    if (!queensideCastlingRightsLost[e]) {
        if (color == BLACK ? move.target() == 0 : move.target() == 56) {
            queensideCastlingRightsLost[e] = totalHalfmoves;
            zobrist ^= ZOBRIST_QUEENSIDE_CASTLING_KEYS[e];
        }
    }

    positionInfo[positionInfoIndex] |= zobrist >> 44;

    return true;
}

void Game::unmakeMove(Game::Move& move)
{
    uint8 c = move.moving() >> 3;
    uint8 color = c << 3;
    uint8 e = !color;
    uint8 enemy = e << 3;

    // UNDO PEICE DATA / ZOBRIST HASH
    // Undo zobrist hash for turn change
    zobrist ^= ZOBRIST_TURN_KEY;

    // Undo peice array, zobrist hash, and peice indices set for moving peice
    peices[move.start()] = move.moving();
    peices[move.target()] = move.captured();
    if (move.isEnPassant()) {
        peices[move.target()] = 0;
        peices[move.target() - 8 + 16 * c] = move.captured();
    }
    if (move.promotion()) {
        ++numPeices[move.moving()];
        --numPeices[color + move.promotion()];
        zobrist ^= ZOBRIST_PEICE_KEYS[c][move.promotion() - 1][move.target()];

    }
    else {
        zobrist ^= ZOBRIST_PEICE_KEYS[c][(move.moving() & 0b111) - 1][move.target()];
    }
    zobrist ^= ZOBRIST_PEICE_KEYS[c][(move.moving() & 0b111) - 1][move.start()];

    // Undo zobrist hash and peice indices set for capture
    if (move.captured()) {
        uint8 captureSquare = move.isEnPassant() ? move.target() - 8 + 16 * c : move.target();
        zobrist ^= ZOBRIST_PEICE_KEYS[e][(move.captured() & 0b111) - 1][captureSquare];
        ++numPeices[move.captured()];
        ++numTotalPeices[e];
    }

    // Undo rooks for castling
    if (move.isCastling()) {
        uint8 rookStart;
        uint8 rookEnd;

        uint8 castlingRank = move.target() & 0b11111000;
        if (move.target() % 8 < 4) {
            // Queenside castling
            rookStart = castlingRank;
            rookEnd = castlingRank + 3;
        }
        else {
            // Kingside castling
            rookStart = castlingRank + 7;
            rookEnd = castlingRank + 5;
        }

        peices[rookStart] = peices[rookEnd];
        peices[rookEnd] = 0;
        zobrist ^= ZOBRIST_PEICE_KEYS[c][ROOK - 1][rookStart];
        zobrist ^= ZOBRIST_PEICE_KEYS[c][ROOK - 1][rookEnd];
    }

    // UBDO BOARD FLAGS
    // Undo king index
    if ((move.moving() & 0b111) == KING) {
        kingIndex[c] = move.start();
    }

    // undo castling rights
    if (kingsideCastlingRightsLost[c] == totalHalfmoves) {
        kingsideCastlingRightsLost[c] = 0;
        zobrist ^= ZOBRIST_KINGSIDE_CASTLING_KEYS[c];
    }
    if (queensideCastlingRightsLost[c] == totalHalfmoves) {
        queensideCastlingRightsLost[c] = 0;
        zobrist ^= ZOBRIST_QUEENSIDE_CASTLING_KEYS[c];

    }
    if (kingsideCastlingRightsLost[e] == totalHalfmoves) {
        kingsideCastlingRightsLost[e] = 0;
        zobrist ^= ZOBRIST_KINGSIDE_CASTLING_KEYS[e];
    }
    if (queensideCastlingRightsLost[e] == totalHalfmoves) {
        queensideCastlingRightsLost[e] = 0;
        zobrist ^= ZOBRIST_QUEENSIDE_CASTLING_KEYS[e];
    }

    // decrement counters
    --totalHalfmoves;
    positionInfo[positionInfoIndex--] = 0;
}

inline bool Game::isDraw() const
{
    return isDrawByFiftyMoveRule() || isDrawByInsufficientMaterial() || isDrawByThreefoldRepitition();
}

inline uint8 Game::halfMovesSincePawnMoveOrCapture() const noexcept
{
    return static_cast<uint8>((positionInfo[positionInfoIndex] >> 20) & 0b111111);
}

inline uint8 Game::eligibleEnpassantSquare() const noexcept
{
    return static_cast<uint8>(positionInfo[positionInfoIndex] >> 26);
}

bool Game::isDrawByThreefoldRepitition() const noexcept
{
    if (halfMovesSincePawnMoveOrCapture() < 8) {
        return false;
    }

    uint8 index = positionInfoIndex - 2;
    uint8 numPossibleRepitions = halfMovesSincePawnMoveOrCapture() / 2 - 1;

    uint32 currentHash = static_cast<uint32>(zobrist >> 44);
    bool repititionFound = false;

    for (uint8 i = 0; i < numPossibleRepitions; ++i) {
        index -= 2;
        if ((positionInfo[index] & ((1 << 20) - 1)) == currentHash) {
            if (repititionFound) {
                return true;
            }
            repititionFound = true;
        }
    }
    return false;
}

inline bool Game::isDrawByFiftyMoveRule() const noexcept
{
    return halfMovesSincePawnMoveOrCapture() >= 50;
}

bool Game::isDrawByInsufficientMaterial() const noexcept
{
    if (numTotalPeices[0] > 3 || numTotalPeices[1] > 3) {
        return false;
    }
    if (numTotalPeices[0] == 3 || numTotalPeices[1] == 3) {
        return (numPeices[WHITE + KNIGHT] == 2 || numPeices[BLACK + KNIGHT] == 2) && (numTotalPeices[0] == 1 || numTotalPeices[1] == 1);
    }
    return !(numPeices[WHITE + PAWN] || numPeices[BLACK + PAWN] || numPeices[WHITE + ROOK] || numPeices[BLACK + ROOK] || numPeices[WHITE + QUEEN] || numPeices[BLACK + QUEEN]);
}

bool Game::inCheck(uint8 c) const
{
    // TODO Backwards check searching during endgame

    uint8 color = c << 3;
    uint8 e = !c;
    uint8 enemy = e << 3;
    uint8 king = kingIndex[c];

    // Pawn checks
    uint8 kingFile = king % 8;
    uint8 ahead = king + 8 - 16 * c;
    if (kingFile != 0 && peices[ahead - 1] == enemy + PAWN) {
        return true;
    }
    if (kingFile != 7 && peices[ahead + 1] == enemy + PAWN) {
        return true;
    }

    // Knight checks
    for (uint8 j = 1; j < KNIGHT_MOVES[king][0]; ++j) {
        if (peices[KNIGHT_MOVES[king][j]] == enemy + KNIGHT) {
            return true;
        }
    }

    // sliding peice checks
    for (int8 j = king - 8; j >= DIRECTION_BOUNDS[king][B]; j -= 8) {
        if (peices[j]) {
            if (peices[j] == enemy + ROOK || peices[j] == enemy + QUEEN) {
                return true;
            }
            break;
        }
    }

    for (int8 j = king + 8; j <= DIRECTION_BOUNDS[king][F]; j += 8) {
        if (peices[j]) {
            if (peices[j] == enemy + ROOK || peices[j] == enemy + QUEEN) {
                return true;
            }
            break;
        }
    }

    for (int8 j = king - 1; j >= DIRECTION_BOUNDS[king][L]; j -= 1) {
        if (peices[j]) {
            if (peices[j] == enemy + ROOK || peices[j] == enemy + QUEEN) {
                return true;
            }
            break;
        }
    }

    for (int8 j = king + 1; j <= DIRECTION_BOUNDS[king][R]; j += 1) {
        if (peices[j]) {
            if (peices[j] == enemy + ROOK || peices[j] == enemy + QUEEN) {
                return true;
            }
            break;
        }
    }

    for (int8 j = king - 9; j >= DIRECTION_BOUNDS[king][BL]; j -= 9) {
        if (peices[j]) {
            if (peices[j] == enemy + BISHOP || peices[j] == enemy + QUEEN) {
                return true;
            }
            break;
        }
    }

    for (int8 j = king + 9; j <= DIRECTION_BOUNDS[king][FR]; j += 9) {
        if (peices[j]) {
            if (peices[j] == enemy + BISHOP || peices[j] == enemy + QUEEN) {
                return true;
            }
            break;
        }
    }

    for (int8 j = king - 7; j >= DIRECTION_BOUNDS[king][BR]; j -= 7) {
        if (peices[j]) {
            if (peices[j] == enemy + BISHOP || peices[j] == enemy + QUEEN) {
                return true;
            }
            break;
        }
    }

    for (int8 j = king + 7; j <= DIRECTION_BOUNDS[king][FL]; j += 7) {
        if (peices[j]) {
            if (peices[j] == enemy + BISHOP || peices[j] == enemy + QUEEN) {
                return true;
            }
            break;
        }
    }

    // King checks (seems wierd, but needed for detecting illegal moves)
    for (uint8 j = 1; j < KING_MOVES[king][0]; ++j) {
        if (peices[KING_MOVES[king][j]] == enemy + KING) {
            return true;
        }
    }

    return false;
}

bool Game::isLegal(Move& move)
{
    if (move.legalFlagSet()) {
        return true;
    }

    if (move.isCastling()) {
        // Check if king is in check
        if (inCheck()) {
            return false;
        }

        return castlingMoveIsLegal(move);
    }

    if (makeMove(move)) {
        unmakeMove(move);
        move.setLegalFlag();
        return true;
    }
    return false;
}

bool Game::castlingMoveIsLegal(Move& move) {
    if (move.legalFlagSet()) {
        return true;
    }

    uint8 c = totalHalfmoves % 2;
    uint8 color = c << 3;
    uint8 e = !color;
    uint8 enemy = e << 3;
    uint8 castlingRank = move.start() & 0b11111000;

    // Check if anything is attacking squares on king's path
    uint8 s;
    uint8 end;
    if (move.target() - castlingRank < 4) {
        s = castlingRank + 2;
        end = castlingRank + 3;
    }
    else {
        s = castlingRank + 5;
        end = castlingRank + 6;
    }
    for (; s <= end; ++s) {
        // Pawn attacks
        uint8 file = s % 8;
        uint8 ahead = s + 8 - 16 * c;
        if (file != 0 && peices[ahead - 1] == enemy + PAWN) {
            return false;
        }
        if (file != 7 && peices[ahead + 1] == enemy + PAWN) {
            return false;
        }

        // Knight attacks
        for (uint8 j = 1; j < KNIGHT_MOVES[s][0]; ++j) {
            if (peices[KNIGHT_MOVES[s][j]] == enemy + KNIGHT) {
                return false;
            }
        }

        // sliding peice attacks
        if (color == BLACK) {
            for (int8 j = s - 8; j >= DIRECTION_BOUNDS[s][B]; j -= 8) {
                if (peices[j]) {
                    if (peices[j] == enemy + ROOK || peices[j] == enemy + QUEEN) {
                        return false;
                    }
                    break;
                }
            }

            for (int8 j = s - 9; j >= DIRECTION_BOUNDS[s][BL]; j -= 9) {
                if (peices[j]) {
                    if (peices[j] == enemy + BISHOP || peices[j] == enemy + QUEEN) {
                        return false;
                    }
                    break;
                }
            }

            for (int8 j = s - 7; j >= DIRECTION_BOUNDS[s][BR]; j -= 7) {
                if (peices[j]) {
                    if (peices[j] == enemy + BISHOP || peices[j] == enemy + QUEEN) {
                        return false;
                    }
                    break;
                }
            }

        }
        else {
            for (int8 j = s + 8; j <= DIRECTION_BOUNDS[s][F]; j += 8) {
                if (peices[j]) {
                    if (peices[j] == enemy + ROOK || peices[j] == enemy + QUEEN) {
                        return false;
                    }
                    break;
                }
            }

            for (int8 j = s + 9; j <= DIRECTION_BOUNDS[s][FR]; j += 9) {
                if (peices[j]) {
                    if (peices[j] == enemy + BISHOP || peices[j] == enemy + QUEEN) {
                        return false;
                    }
                    break;
                }
            }

            for (int8 j = s + 7; j <= DIRECTION_BOUNDS[s][FL]; j += 7) {
                if (peices[j]) {
                    if (peices[j] == enemy + BISHOP || peices[j] == enemy + QUEEN) {
                        return false;
                    }
                    break;
                }
            }
        }

        // King attacks
        for (uint8 j = 1; j < KING_MOVES[s][0]; ++j) {
            if (peices[KING_MOVES[s][j]] == enemy + KING) {
                return false;
            }
        }
    }

    move.setLegalFlag();
    return true;
}

std::string Game::getCurrentDate()
{
    auto now = std::chrono::system_clock::now();
    auto now_c = std::chrono::system_clock::to_time_t(now);

    std::tm now_tm;
    localtime_s(&now_tm, &now_c); // Use localtime_s instead of localtime

    std::stringstream ss;
    ss << std::put_time(&now_tm, "%Y.%m.%d");
    return ss.str();
}

// END GAME
