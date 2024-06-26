#include "EngineV1_0.h"

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


#define MAX_GAME_LENGTH 500
#define MAX_DEPTH 32
#define MOVE_STACK_SIZE 1500
#define MAX_EVAL INT16_MAX

// TODO add some sort of protection for games with halfmove counter > 500

// PUBLIC METHODS
EngineV1_0::EngineV1_0(std::string& fenString)
{
    loadFEN(fenString);
}

EngineV1_0::EngineV1_0()
{
    loadStartingPosition();
}

void EngineV1_0::loadStartingPosition()
{
    std::string startingFen = "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1";
    loadFEN(startingFen);
}

void EngineV1_0::loadFEN(const std::string& fenString)
{
    initializeFen(fenString);
}

std::vector<StandardMove> EngineV1_0::getLegalMoves() noexcept
{
    std::vector<StandardMove> moves;

    for (Move& move : enginePositionMoves) {
        moves.emplace_back(move.start(), move.target(), move.promotion());
    }

    return moves;
}

int EngineV1_0::colorToMove() noexcept
{
    return 1 - 2 * (totalHalfmoves % 2);
}

StandardMove EngineV1_0::computerMove(std::chrono::milliseconds thinkTime)
{
    if (gameOver().has_value()) {
        throw std::runtime_error("Game is over, cannot get computer move!");
    }

    // Time management variables
    auto endSearch = std::chrono::high_resolution_clock::now() + thinkTime;
    std::chrono::nanoseconds lastSearchDuration(0);
    std::chrono::milliseconds totalSearchTime(0);

    Move moveStack[1500];

    std::cout << "SEARCH " << asFEN() << std::endl;

    // Incrementally increase depth until time is up
    for (int16 depth = 0;; ++depth) {
        // Calculate the cutoff time to halt search based on last search
        auto searchCutoff = endSearch - lastSearchDuration * 1.25;
        auto start = std::chrono::high_resolution_clock::now();

        std::cout << "depth " << depth;

        // Run search for each move
        for (Move& move : enginePositionMoves) {
            if (std::chrono::high_resolution_clock::now() > searchCutoff) {
                std::cout << " timeout";
                break;
            }

            makeMove(move);
            move.strengthGuess = -search_std(1, depth, moveStack, 0);
            unmakeMove(move);
        }

        // Sort moves in order by score
        std::sort(enginePositionMoves.begin(), enginePositionMoves.end(), [](Move& l, Move& r) { return l.strengthGuess > r.strengthGuess; });
        std::cout << " bestmove " << enginePositionMoves[0].toString();

        auto end = std::chrono::high_resolution_clock::now();
        lastSearchDuration = end - start;
        totalSearchTime += std::chrono::duration_cast<std::chrono::milliseconds>(lastSearchDuration);
        std::cout << " time " << std::chrono::duration_cast<std::chrono::milliseconds>(lastSearchDuration).count() << "millis" << std::endl;

        if (std::chrono::high_resolution_clock::now() > searchCutoff) {
            break;
        }
    }


    // Return the best move
    Move bestMove = enginePositionMoves[0];
    std::cout << "totaltime " << totalSearchTime.count() << "millis" << std::endl;
    std::cout << bestMove.toString() << std::endl;
    return StandardMove(bestMove.start(), bestMove.target(), bestMove.promotion());
}

void EngineV1_0::inputMove(const StandardMove& move)
{
    if (gameOver().has_value()) {
        throw std::runtime_error("Game is over, cannot input move!");
    }

    for (Move& legalMove : enginePositionMoves) {
        if (legalMove == move) {
            makeMove(legalMove);
            enginePositionMoves = legalMoves();
            return;
        }
    }

    throw std::runtime_error("inputted move is not legal in the current position!");
}

std::optional<int> EngineV1_0::gameOver() noexcept
{
    if (isDraw()) {
        return 0;
    }

    if (enginePositionMoves.size() == 0) {
        return inCheck() ? -colorToMove() : 0;
    }

    return std::nullopt;
}

bool EngineV1_0::inCheck() const noexcept
{
    return inCheck(totalHalfmoves % 2);
}

std::string EngineV1_0::asFEN() const noexcept
{
    std::string fen = "";
    int16 c = totalHalfmoves % 2;

    // Peice placement data
    char pcs[6] = { 'P', 'N', 'B', 'R', 'Q', 'K' };
    int16 gap = 0;
    for (int16 i = 56; i >= 0; i -= 8) {
        for (int16 j = 0; j < 8; ++j) {
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
            fen += pcs[(peices[i + j] % (1 << 3)) - 1] + 32 * (peices[i + j] >> 3);
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
    if (eligibleEnPassantSquare[totalHalfmoves] >= 0) {
        fen += chesshelpers::boardIndexToAlgebraicNotation(eligibleEnPassantSquare[totalHalfmoves]) + " ";
    }
    else {
        fen += "- ";
    }

    // Halfmoves since pawn move or capture
    fen += std::to_string(halfmovesSincePawnMoveOrCapture[hmspmocIndex - 1]);
    fen += ' ';

    // Total moves
    fen += std::to_string(totalHalfmoves / 2);

    return fen;
}

std::uint64_t EngineV1_0::perft(int depth, bool printOut = false) noexcept
{
    Move moveStack[1500];

    if (!printOut) {
        return perft_h(depth, moveStack, 0);
    }

    if (depth == 0) {
        return 1ULL;
    }

    std::cout << "PERFT TEST\nFEN: " << asFEN() << std::endl;

    std::uint64_t nodes = 0;

    for (size_t i = 0; i < enginePositionMoves.size(); ++i) {
        std::uint64_t subnodes = 0;

        std::cout << std::setw(2) << i << " *** " << enginePositionMoves[i].toString() << ": ";
        std::cout.flush();

        if (makeMove(enginePositionMoves[i])) {
            subnodes = perft_h(depth - 1, moveStack, 0);
            nodes += subnodes;
            unmakeMove(enginePositionMoves[i]);
        }

        std::cout << subnodes << std::endl;
    }

    std::cout << "TOTAL: " << nodes << std::endl;
    return nodes;
}

// MOVE STRUCT
// PUBLIC METHODS
inline int16 EngineV1_0::Move::start() const noexcept
{
    return startSquare;
}

inline int16 EngineV1_0::Move::target() const noexcept
{
    return targetSquare;
}

inline int16 EngineV1_0::Move::moving() const noexcept
{
    return movingPeice;
}

inline int16 EngineV1_0::Move::captured() const noexcept
{
    return capturedPeice;
}

inline int16 EngineV1_0::Move::color() const noexcept
{
    return (movingPeice >> 3) << 3;
}

inline int16 EngineV1_0::Move::enemy() const noexcept
{
    return !(movingPeice >> 3) << 3;
}

inline int16 EngineV1_0::Move::promotion() const noexcept
{
    return flags & PROMOTION;
}

inline bool EngineV1_0::Move::isEnPassant() const noexcept
{
    return flags & EN_PASSANT;
}

inline bool EngineV1_0::Move::isCastling() const noexcept
{
    return flags & CASTLE;
}

inline bool EngineV1_0::Move::legalFlagSet() const noexcept
{
    return flags & LEGAL;
}

inline void EngineV1_0::Move::setLegalFlag() noexcept
{
    flags |= LEGAL;
}

inline int16 EngineV1_0::Move::earlygamePositionalMaterialChange() noexcept
{
    if (!posmatInit) {
        initializePosmat();
    }
    return earlyPosmat;
}

inline int16 EngineV1_0::Move::endgamePositionalMaterialChange() noexcept
{
    if (!posmatInit) {
        initializePosmat();
    }
    return endPosmat;
}

inline bool EngineV1_0::Move::operator==(const EngineV1_0::Move& other) const
{
    return this->start() == other.start()
        && this->target() == other.target()
        && this->promotion() == other.promotion();
}

inline bool EngineV1_0::Move::operator==(const StandardMove& other) const
{
    return this->start() == other.startSquare
        && this->target() == other.targetSquare
        && this->promotion() == other.promotion;
}

std::string EngineV1_0::Move::toString()
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
EngineV1_0::Move::Move(const EngineV1_0* board, int16 start, int16 target, int16 givenFlags) : startSquare(start), targetSquare(target), flags(givenFlags), strengthGuess(0), posmatInit(false), earlyPosmat(0), endPosmat(0)
{
    movingPeice = board->peices[start];
    capturedPeice = board->peices[target];
    if (isEnPassant()) {
        capturedPeice = enemy() + PAWN;
    }
}

EngineV1_0::Move::Move() : startSquare(0), targetSquare(0), movingPeice(0), capturedPeice(0), flags(0), strengthGuess(0), posmatInit(false), earlyPosmat(0), endPosmat(0) {}

// PRIVATE METHODS
void EngineV1_0::Move::initializePosmat() noexcept
{
    // Update zobrist hash, numpieces and positonal imbalance for moving peice
    earlyPosmat -= EARLYGAME_PEICE_VALUE[moving()][start()];
    endPosmat -= ENDGAME_PEICE_VALUE[moving()][start()];
    if (promotion()) {
        earlyPosmat += EARLYGAME_PEICE_VALUE[color() + promotion()][target()];
        endPosmat += ENDGAME_PEICE_VALUE[color() + promotion()][target()];
    }
    else {
        earlyPosmat += EARLYGAME_PEICE_VALUE[moving()][target()];
        endPosmat += ENDGAME_PEICE_VALUE[moving()][target()];
    }


    // Update zobrist hash and peice indices set for capture
    if (captured()) {
        int16 captureSquare = isEnPassant() ? target() - 8 + 16 * (color() >> 3) : target();
        earlyPosmat -= EARLYGAME_PEICE_VALUE[captured()][captureSquare];
        endPosmat -= ENDGAME_PEICE_VALUE[captured()][captureSquare];
    }

    // Update rooks for castling
    if (isCastling()) {
        int16 rookStart;
        int16 rookEnd;

        int16 castlingRank = target() & 0b11111000;
        if (target() % 8 < 4) {
            // Queenside castling
            rookStart = castlingRank;
            rookEnd = castlingRank + 3;
        }
        else {
            // Kingside castling
            rookStart = castlingRank + 7;
            rookEnd = castlingRank + 5;
        }

        earlyPosmat -= EARLYGAME_PEICE_VALUE[color() + ROOK][rookStart];
        endPosmat -= ENDGAME_PEICE_VALUE[color() + ROOK][rookStart];
        earlyPosmat += EARLYGAME_PEICE_VALUE[color() + ROOK][rookEnd];
        endPosmat += ENDGAME_PEICE_VALUE[color() + ROOK][rookEnd];
    }
    posmatInit = true;
}
// END MOVE STRUCT

// BOARD METHODS
void EngineV1_0::initializeFen(const std::string& fenString)
{
    // Reset current members
    zobrist = 0;
    for (int i = 0; i < 15; ++i) {
        numPeices[i] = 0;
    }
    numTotalPeices[0] = 0;
    numTotalPeices[1] = 0;

    material_stage_weight = 0;
    earlygamePositionalMaterialInbalance = 0;
    endgamePositionalMaterialInbalance = 0;



    std::istringstream fenStringStream(fenString);
    std::string peicePlacementData, activeColor, castlingAvailabilty, enPassantTarget, halfmoveClock, fullmoveNumber;

    // Get peice placement data from fen string
    if (!std::getline(fenStringStream, peicePlacementData, ' ')) {
        throw std::invalid_argument("Cannot get peice placement from FEN!");
    }

    // Update the peices[] according to fen rules
    int16 peiceIndex = 56;
    for (char peiceInfo : peicePlacementData) {
        if (std::isalpha(peiceInfo)) {
            // char contains data about a peice
            //int16 color = std::islower(c);
            int16 c = peiceInfo > 96 && peiceInfo < 123;
            int16 color = c << 3;

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
            int16 gap = peiceInfo - '0';
            for (int16 i = 0; i < gap; ++i) {
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
            //int16 color = std::islower(c);
            int16 c = castlingInfo > 96 && castlingInfo < 123;
            int16 color = c << 3;
            int16 castlingRank = 56 * c;
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
        hmspmocIndex = 0;
        halfmovesSincePawnMoveOrCapture[hmspmocIndex++] = static_cast<int16>(std::stoi(halfmoveClock));
    }
    catch (const std::invalid_argument& e) {
        throw std::invalid_argument(std::string("Invalid FEN half move clock! ") + e.what());
    }

    // Get full move number data from fen string
    if (!std::getline(fenStringStream, fullmoveNumber, ' ')) {
        fullmoveNumber = "1";
        //throw std::invalid_argument("Cannot getfullmove number from FEN!");
    }

    try {
        totalHalfmoves += static_cast<int16>(std::stoi(fullmoveNumber) * 2 - 2);
    }
    catch (const std::invalid_argument& e) {
        throw std::invalid_argument(std::string("Invalid FEN full move number! ") + e.what());
    }

    // wait until total halfmoves are defined before defining
    for (int i = 0; i < MAX_GAME_LENGTH; ++i) {
        eligibleEnPassantSquare[i] = -1;
    }
    if (enPassantTarget != "-") {
        try {
            eligibleEnPassantSquare[totalHalfmoves] = chesshelpers::algebraicNotationToBoardIndex(enPassantTarget);
        }
        catch (const std::invalid_argument& e) {
            throw std::invalid_argument(std::string("Invalid FEN en passant target! ") + e.what());
        }

    }
    else {
        eligibleEnPassantSquare[totalHalfmoves] = -1;
    }

    // initialize zobrist hash for all of the peices
    for (int16 i = 0; i < 64; ++i) {
        int16 peice = peices[i];
        if (peice) {
            zobrist ^= ZOBRIST_PEICE_KEYS[peice >> 3][peice % (1 << 3) - 1][i];
            ++numPeices[peice];
            ++numTotalPeices[peice >> 3];
            material_stage_weight += PEICE_STAGE_WEIGHTS[peice];
            earlygamePositionalMaterialInbalance += EARLYGAME_PEICE_VALUE[peice][i];
            endgamePositionalMaterialInbalance += ENDGAME_PEICE_VALUE[peice][i];
        }
    }

    enginePositionMoves = legalMoves();
}

void EngineV1_0::generatePseudoLegalMoves(Move* stack, int16& idx) const
{
    // TODO Backwards check/pin generation for endgame
    // TODO Better pinned peice generation (detect if sliding along diagonal of pin)

    int16 c = totalHalfmoves % 2;
    int16 color = c << 3;
    int16 e = !color;
    int16 enemy = e << 3;

    // Used for optimizing the legality checking of moves
    bool isPinned[64] = { 0 };
    std::unordered_set<int16> checkingSquares(11);

    // Check if king in check and record pinned peices and checking squares
    int16 king = kingIndex[c];
    int16 checks = 0;

    // Pawn checks
    if (numPeices[enemy + PAWN]) {
        int16 kingFile = king % 8;
        int16 kingAhead = king + 8 - 16 * c;
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
        for (int16 j = 1; j < KNIGHT_MOVES[king][0]; ++j) {
            if (peices[KNIGHT_MOVES[king][j]] == enemy + KNIGHT) {
                checkingSquares.insert(KNIGHT_MOVES[king][j]);
                ++checks;
            }
        }
    }

    // pins and sliding peice checks
    int16 potentialPin = 0;
    // Dont bother checking horizontals if no bishops or queens
    if (numPeices[enemy + ROOK] | numPeices[enemy + QUEEN]) {
        for (int16 j = king - 8; j >= DIRECTION_BOUNDS[king][B]; j -= 8) {
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
                for (int16 k = j; k < king; k += 8) {
                    checkingSquares.insert(k);
                }
            }
            break;
        }

        potentialPin = 0;
        for (int16 j = king + 8; j <= DIRECTION_BOUNDS[king][F]; j += 8) {
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
                for (int16 k = j; k > king; k -= 8) {
                    checkingSquares.insert(k);
                }
            }
            break;
        }

        potentialPin = 0;
        for (int16 j = king - 1; j >= DIRECTION_BOUNDS[king][L]; j -= 1) {
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
                for (int16 k = j; k < king; k += 1) {
                    checkingSquares.insert(k);
                }
            }
            break;
        }

        potentialPin = 0;
        for (int16 j = king + 1; j <= DIRECTION_BOUNDS[king][R]; j += 1) {
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
                for (int16 k = j; k > king; k -= 1) {
                    checkingSquares.insert(k);
                }
            }
            break;
        }
    }

    // Dont bother checking diagonals if no bishops or queens
    if (numPeices[enemy + BISHOP] | numPeices[enemy + QUEEN]) {
        potentialPin = 0;
        for (int16 j = king - 9; j >= DIRECTION_BOUNDS[king][BL]; j -= 9) {
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
                for (int16 k = j; k < king; k += 9) {
                    checkingSquares.insert(k);
                }
            }
            break;
        }

        potentialPin = 0;
        for (int16 j = king + 9; j <= DIRECTION_BOUNDS[king][FR]; j += 9) {
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
                for (int16 k = j; k > king; k -= 9) {
                    checkingSquares.insert(k);
                }
            }
            break;
        }

        potentialPin = 0;
        for (int16 j = king - 7; j >= DIRECTION_BOUNDS[king][BR]; j -= 7) {
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
                for (int16 k = j; k < king; k += 7) {
                    checkingSquares.insert(k);
                }
            }
            break;
        }

        potentialPin = 0;
        for (int16 j = king + 7; j <= DIRECTION_BOUNDS[king][FL]; j += 7) {
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
                for (int16 k = j; k > king; k -= 7) {
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
        for (int16 j = 1; j < KING_MOVES[king][0]; ++j) {
            if ((!peices[KING_MOVES[king][j]] && !checkingSquares.count(KING_MOVES[king][j])) || peices[KING_MOVES[king][j]] >> 3 == e) {
                stack[idx++] = Move(this, king, KING_MOVES[king][j], Move::NONE);
            }
        }
        return;
    }

    // En passant moves
    int16 epSquare = eligibleEnPassantSquare[totalHalfmoves];
    if (epSquare >= 0) {
        int16 epfile = epSquare % 8;
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
    if (checks && checkingSquares.size() < 3) {
        // Generate king moves
        for (int16 j = 1; j < KING_MOVES[king][0]; ++j) {
            if ((!peices[KING_MOVES[king][j]] && !checkingSquares.count(KING_MOVES[king][j])) || peices[KING_MOVES[king][j]] >> 3 == e) {
                stack[idx++] = Move(this, king, KING_MOVES[king][j], Move::NONE);
            }
        }

        // Backwards search from checking squares to see if peices can move to them
        for (int16 t : checkingSquares) {

            // Pawn can block/take
            if (numPeices[color + PAWN]) {
                if ((color == WHITE && t >> 3 >= 2) || (color == BLACK && t >> 3 <= 5)) {
                    int16 file = t % 8;
                    int16 ahead = t - 8 + 16 * c;
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
                        int16 doubleAhead = ahead - 8 + 16 * c;
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
                for (int16 j = 1; j < KNIGHT_MOVES[t][0]; ++j) {
                    if (peices[KNIGHT_MOVES[t][j]] == color + KNIGHT && !isPinned[KNIGHT_MOVES[t][j]]) {
                        stack[idx++] = Move(this, KNIGHT_MOVES[t][j], t, Move::LEGAL);
                    }
                }
            }

            // Sliding peices can block/take
            if (numPeices[color + ROOK] | numPeices[color + QUEEN]) {
                for (int16 s = t - 8; s >= DIRECTION_BOUNDS[t][B]; s -= 8) {
                    if (peices[s]) {
                        if ((peices[s] == color + ROOK || peices[s] == color + QUEEN) && !isPinned[s]) {
                            stack[idx++] = Move(this, s, t, Move::LEGAL);
                        }
                        break;
                    }
                }

                for (int16 s = t + 8; s <= DIRECTION_BOUNDS[t][F]; s += 8) {
                    if (peices[s]) {
                        if ((peices[s] == color + ROOK || peices[s] == color + QUEEN) && !isPinned[s]) {
                            stack[idx++] = Move(this, s, t, Move::LEGAL);
                        }
                        break;
                    }
                }

                for (int16 s = t - 1; s >= DIRECTION_BOUNDS[t][L]; s -= 1) {
                    if (peices[s]) {
                        if ((peices[s] == color + ROOK || peices[s] == color + QUEEN) && !isPinned[s]) {
                            stack[idx++] = Move(this, s, t, Move::LEGAL);
                        }
                        break;
                    }
                }

                for (int16 s = t + 1; s <= DIRECTION_BOUNDS[t][R]; s += 1) {
                    if (peices[s]) {
                        if ((peices[s] == color + ROOK || peices[s] == color + QUEEN) && !isPinned[s]) {
                            stack[idx++] = Move(this, s, t, Move::LEGAL);
                        }
                        break;
                    }
                }
            }

            if (numPeices[color + BISHOP] | numPeices[color + QUEEN]) {
                for (int16 s = t - 9; s >= DIRECTION_BOUNDS[t][BL]; s -= 9) {
                    if (peices[s]) {
                        if ((peices[s] == color + BISHOP || peices[s] == color + QUEEN) && !isPinned[s]) {
                            stack[idx++] = Move(this, s, t, Move::LEGAL);
                        }
                        break;
                    }
                }

                for (int16 s = t + 9; s <= DIRECTION_BOUNDS[t][FR]; s += 9) {
                    if (peices[s]) {
                        if ((peices[s] == color + BISHOP || peices[s] == color + QUEEN) && !isPinned[s]) {
                            stack[idx++] = Move(this, s, t, Move::LEGAL);
                        }
                        break;
                    }
                }

                for (int16 s = t - 7; s >= DIRECTION_BOUNDS[t][BR]; s -= 7) {
                    if (peices[s]) {
                        if ((peices[s] == color + BISHOP || peices[s] == color + QUEEN) && !isPinned[s]) {
                            stack[idx++] = Move(this, s, t, Move::LEGAL);
                        }
                        break;
                    }
                }

                for (int16 s = t + 7; s <= DIRECTION_BOUNDS[t][FL]; s += 7) {
                    if (peices[s]) {
                        if ((peices[s] == color + BISHOP || peices[s] == color + QUEEN) && !isPinned[s]) {
                            stack[idx++] = Move(this, s, t, Move::LEGAL);
                        }
                        break;
                    }
                }
            }
        }
        return;
    }

    // Castling
    if (!kingsideCastlingRightsLost[c] && !checks) {
        int16 castlingRank = 56 * c;
        bool roomToCastle = true;
        for (int16 j = castlingRank + 5; j < castlingRank + 7; ++j) {
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
        int16 castlingRank = 56 * c;
        bool roomToCastle = true;
        for (int16 j = castlingRank + 3; j > castlingRank; --j) {
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
    for (int16 s = 0; s < 64; ++s) {
        if (peices[s] && peices[s] >> 3 == c) {
            int16 legalFlag = isPinned[s] ? Move::NONE : Move::LEGAL;

            switch (peices[s] % (1 << 3)) {
            case PAWN: {
                int16 file = s % 8;
                int16 ahead = s + 8 - 16 * c;
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
                    int16 doubleAhead = ahead + 8 - 16 * c;
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
                int16 t;
                for (int16 j = 1; j < KNIGHT_MOVES[s][0]; ++j) {
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
                if (peices[s] % (1 << 3) == BISHOP) {
                    goto bishopMoves;
                }

                for (int16 t = s - 8; t >= DIRECTION_BOUNDS[s][B]; t -= 8) {
                    if ((!peices[t] || peices[t] >> 3 == e) && (!checks || checkingSquares.count(t))) {
                        stack[idx++] = Move(this, s, t, legalFlag);
                    }
                    if (peices[t]) {
                        break;
                    }
                }

                for (int16 t = s + 8; t <= DIRECTION_BOUNDS[s][F]; t += 8) {
                    if ((!peices[t] || peices[t] >> 3 == e) && (!checks || checkingSquares.count(t))) {
                        stack[idx++] = Move(this, s, t, legalFlag);
                    }
                    if (peices[t]) {
                        break;
                    }
                }

                for (int16 t = s - 1; t >= DIRECTION_BOUNDS[s][L]; t -= 1) {
                    if ((!peices[t] || peices[t] >> 3 == e) && (!checks || checkingSquares.count(t))) {
                        stack[idx++] = Move(this, s, t, legalFlag);
                    }
                    if (peices[t]) {
                        break;
                    }
                }

                for (int16 t = s + 1; t <= DIRECTION_BOUNDS[s][R]; t += 1) {
                    if ((!peices[t] || peices[t] >> 3 == e) && (!checks || checkingSquares.count(t))) {
                        stack[idx++] = Move(this, s, t, legalFlag);
                    }
                    if (peices[t]) {
                        break;
                    }
                }

                if (peices[s] % (1 << 3) == ROOK) {
                    break;
                }
            bishopMoves:

                for (int16 t = s - 9; t >= DIRECTION_BOUNDS[s][BL]; t -= 9) {
                    if ((!peices[t] || peices[t] >> 3 == e) && (!checks || checkingSquares.count(t))) {
                        stack[idx++] = Move(this, s, t, legalFlag);
                    }
                    if (peices[t]) {
                        break;
                    }
                }

                for (int16 t = s + 9; t <= DIRECTION_BOUNDS[s][FR]; t += 9) {
                    if ((!peices[t] || peices[t] >> 3 == e) && (!checks || checkingSquares.count(t))) {
                        stack[idx++] = Move(this, s, t, legalFlag);
                    }
                    if (peices[t]) {
                        break;
                    }
                }

                for (int16 t = s - 7; t >= DIRECTION_BOUNDS[s][BR]; t -= 7) {
                    if ((!peices[t] || peices[t] >> 3 == e) && (!checks || checkingSquares.count(t))) {
                        stack[idx++] = Move(this, s, t, legalFlag);
                    }
                    if (peices[t]) {
                        break;
                    }
                }

                for (int16 t = s + 7; t <= DIRECTION_BOUNDS[s][FL]; t += 7) {
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
                int16 t;
                for (int16 j = 1; j < KING_MOVES[s][0]; ++j) {
                    t = KING_MOVES[s][j];
                    if (!peices[t] || peices[t] >> 3 == e) {
                        stack[idx++] = Move(this, s, t, Move::NONE);
                    }
                }
            }
            }
        }
    }
}

std::vector<EngineV1_0::Move> EngineV1_0::legalMoves()
{
    Move moves[225];
    int16 end = 0;
    generatePseudoLegalMoves(moves, end);

    std::vector<Move> legalMoves;

    for (int i = 0; i < end; ++i) {
        if (isLegal(moves[i])) {
            legalMoves.push_back(moves[i]);
        }
    }

    return legalMoves;
}

bool EngineV1_0::makeMove(EngineV1_0::Move& move)
{
    positionHistory[totalHalfmoves] = static_cast<uint32>(zobrist);

    int16 c = move.moving() >> 3;
    int16 color = c << 3;
    int16 e = !color;
    int16 enemy = e << 3;

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
    if (move.moving() % (1 << 3) == KING) {
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
        if (move.moving() % (1 << 3) == KING) {
            kingIndex[c] = move.start();
        }
        return false;
    }

    move.setLegalFlag();

    // UPDATE PEICE DATA / ZOBRIST HASH
    // Update zobrist hash for turn change
    zobrist ^= ZOBRIST_TURN_KEY;

    earlygamePositionalMaterialInbalance += move.earlygamePositionalMaterialChange();
    endgamePositionalMaterialInbalance += move.endgamePositionalMaterialChange();

    // Update zobrist hash, numpieces and positonal imbalance for moving peice
    zobrist ^= ZOBRIST_PEICE_KEYS[c][move.moving() % (1 << 3) - 1][move.start()];
    if (move.promotion()) {
        zobrist ^= ZOBRIST_PEICE_KEYS[c][move.promotion() - 1][move.target()];
        --numPeices[move.moving()];
        ++numPeices[color + move.promotion()];
        material_stage_weight -= PEICE_STAGE_WEIGHTS[move.moving()];
        material_stage_weight += PEICE_STAGE_WEIGHTS[color + move.promotion()];
    }
    else {
        zobrist ^= ZOBRIST_PEICE_KEYS[c][move.moving() % (1 << 3) - 1][move.target()];
    }


    // Update zobrist hash and peice indices set for capture
    if (move.captured()) {
        int16 captureSquare = move.isEnPassant() ? move.target() - 8 + 16 * c : move.target();
        zobrist ^= ZOBRIST_PEICE_KEYS[e][move.captured() % (1 << 3) - 1][captureSquare];
        --numPeices[move.captured()];
        --numTotalPeices[e];
        material_stage_weight -= PEICE_STAGE_WEIGHTS[move.captured()];
    }

    // Update rooks for castling
    if (move.isCastling()) {
        int16 rookStart;
        int16 rookEnd;

        int16 castlingRank = move.target() & 0b11111000;
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
    if (move.captured() || move.moving() == color + PAWN) {
        halfmovesSincePawnMoveOrCapture[hmspmocIndex++] = 0;
    }
    else {
        ++halfmovesSincePawnMoveOrCapture[hmspmocIndex - 1];
    }

    // En passant file
    if (move.moving() % (1 << 3) == PAWN && std::abs(move.target() - move.start()) == 16) {
        eligibleEnPassantSquare[totalHalfmoves] = (move.start() + move.target()) / 2;
    }
    else {
        eligibleEnPassantSquare[totalHalfmoves] = -1;
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

    return true;
}

void EngineV1_0::unmakeMove(EngineV1_0::Move& move)
{
    int16 c = move.moving() >> 3;
    int16 color = c << 3;
    int16 e = !color;
    int16 enemy = e << 3;

    // UNDO PEICE DATA / ZOBRIST HASH
    // Undo zobrist hash for turn change
    zobrist ^= ZOBRIST_TURN_KEY;

    earlygamePositionalMaterialInbalance -= move.earlygamePositionalMaterialChange();
    endgamePositionalMaterialInbalance -= move.endgamePositionalMaterialChange();

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
        material_stage_weight += PEICE_STAGE_WEIGHTS[move.moving()];
        material_stage_weight -= PEICE_STAGE_WEIGHTS[color + move.promotion()];
        zobrist ^= ZOBRIST_PEICE_KEYS[c][move.promotion() - 1][move.target()];

    }
    else {
        zobrist ^= ZOBRIST_PEICE_KEYS[c][move.moving() % (1 << 3) - 1][move.target()];
    }
    zobrist ^= ZOBRIST_PEICE_KEYS[c][move.moving() % (1 << 3) - 1][move.start()];

    // Undo zobrist hash and peice indices set for capture
    if (move.captured()) {
        int16 captureSquare = move.isEnPassant() ? move.target() - 8 + 16 * c : move.target();
        zobrist ^= ZOBRIST_PEICE_KEYS[e][move.captured() % (1 << 3) - 1][captureSquare];
        material_stage_weight += PEICE_STAGE_WEIGHTS[move.captured()];
        ++numPeices[move.captured()];
        ++numTotalPeices[e];
    }

    // Undo rooks for castling
    if (move.isCastling()) {
        int16 rookStart;
        int16 rookEnd;

        int16 castlingRank = move.target() & 0b11111000;
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
    if (move.moving() % (1 << 3) == KING) {
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
    if (move.captured() || move.moving() == color + PAWN) {
        --hmspmocIndex;
    }
    else {
        --halfmovesSincePawnMoveOrCapture[hmspmocIndex - 1];
    }
}

bool EngineV1_0::isDraw() const
{
    return isDrawByFiftyMoveRule() || isDrawByInsufficientMaterial() || isDrawByThreefoldRepitition();
}

bool EngineV1_0::isDrawByThreefoldRepitition() const noexcept
{
    if (halfmovesSincePawnMoveOrCapture[hmspmocIndex - 1] < 8) {
        return false;
    }

    int16 index = totalHalfmoves - 4;
    int16 numPossibleRepitions = halfmovesSincePawnMoveOrCapture[hmspmocIndex - 1] / 2 - 1;
    uint32 currentHash = static_cast<uint32>(zobrist);
    bool repititionFound = false;

    for (int16 i = 0; i < numPossibleRepitions; ++i) {
        if (positionHistory[index] == currentHash) {
            if (repititionFound) {
                return true;
            }
            repititionFound = true;
        }
        index -= 2;
    }
    return false;
}

inline bool EngineV1_0::isDrawByFiftyMoveRule() const noexcept
{
    return halfmovesSincePawnMoveOrCapture[hmspmocIndex - 1] >= 50;
}

bool EngineV1_0::isDrawByInsufficientMaterial() const noexcept
{
    if (numTotalPeices[0] > 3 || numTotalPeices[1] > 3) {
        return false;
    }
    if (numTotalPeices[0] == 3 || numTotalPeices[1] == 3) {
        return (numPeices[WHITE + KNIGHT] == 2 || numPeices[BLACK + KNIGHT] == 2) && (numTotalPeices[0] == 1 || numTotalPeices[1] == 1);
    }
    return !(numPeices[WHITE + PAWN] || numPeices[BLACK + PAWN] || numPeices[WHITE + ROOK] || numPeices[BLACK + ROOK] || numPeices[WHITE + QUEEN] || numPeices[BLACK + QUEEN]);
}

bool EngineV1_0::repititionOcurred() const noexcept
{
    if (halfmovesSincePawnMoveOrCapture[hmspmocIndex - 1] < 4) {
        return false;
    }

    int16 index = totalHalfmoves - 4;
    int16 numPossibleRepitions = halfmovesSincePawnMoveOrCapture[hmspmocIndex - 1] / 2 - 1;
    uint32 currentHash = static_cast<uint32>(zobrist);

    for (int16 i = 0; i < numPossibleRepitions; ++i) {
        if (positionHistory[index] == currentHash) {
            return true;
        }
        index -= 2;
    }
    return false;
}

bool EngineV1_0::inCheck(int16 c) const
{
    // TODO Backwards check searching during endgame

    int16 color = c << 3;
    int16 e = !c;
    int16 enemy = e << 3;
    int16 king = kingIndex[c];

    // Pawn checks
    int16 kingFile = king % 8;
    int16 ahead = king + 8 - 16 * c;
    if (kingFile != 0 && peices[ahead - 1] == enemy + PAWN) {
        return true;
    }
    if (kingFile != 7 && peices[ahead + 1] == enemy + PAWN) {
        return true;
    }

    // Knight checks
    for (int16 j = 1; j < KNIGHT_MOVES[king][0]; ++j) {
        if (peices[KNIGHT_MOVES[king][j]] == enemy + KNIGHT) {
            return true;
        }
    }

    // sliding peice checks
    for (int16 j = king - 8; j >= DIRECTION_BOUNDS[king][B]; j -= 8) {
        if (peices[j]) {
            if (peices[j] == enemy + ROOK || peices[j] == enemy + QUEEN) {
                return true;
            }
            break;
        }
    }

    for (int16 j = king + 8; j <= DIRECTION_BOUNDS[king][F]; j += 8) {
        if (peices[j]) {
            if (peices[j] == enemy + ROOK || peices[j] == enemy + QUEEN) {
                return true;
            }
            break;
        }
    }

    for (int16 j = king - 1; j >= DIRECTION_BOUNDS[king][L]; j -= 1) {
        if (peices[j]) {
            if (peices[j] == enemy + ROOK || peices[j] == enemy + QUEEN) {
                return true;
            }
            break;
        }
    }

    for (int16 j = king + 1; j <= DIRECTION_BOUNDS[king][R]; j += 1) {
        if (peices[j]) {
            if (peices[j] == enemy + ROOK || peices[j] == enemy + QUEEN) {
                return true;
            }
            break;
        }
    }

    for (int16 j = king - 9; j >= DIRECTION_BOUNDS[king][BL]; j -= 9) {
        if (peices[j]) {
            if (peices[j] == enemy + BISHOP || peices[j] == enemy + QUEEN) {
                return true;
            }
            break;
        }
    }

    for (int16 j = king + 9; j <= DIRECTION_BOUNDS[king][FR]; j += 9) {
        if (peices[j]) {
            if (peices[j] == enemy + BISHOP || peices[j] == enemy + QUEEN) {
                return true;
            }
            break;
        }
    }

    for (int16 j = king - 7; j >= DIRECTION_BOUNDS[king][BR]; j -= 7) {
        if (peices[j]) {
            if (peices[j] == enemy + BISHOP || peices[j] == enemy + QUEEN) {
                return true;
            }
            break;
        }
    }

    for (int16 j = king + 7; j <= DIRECTION_BOUNDS[king][FL]; j += 7) {
        if (peices[j]) {
            if (peices[j] == enemy + BISHOP || peices[j] == enemy + QUEEN) {
                return true;
            }
            break;
        }
    }

    // King checks (seems wierd, but needed for detecting illegal moves)
    for (int16 j = 1; j < KING_MOVES[king][0]; ++j) {
        if (peices[KING_MOVES[king][j]] == enemy + KING) {
            return true;
        }
    }

    return false;
}

bool EngineV1_0::isLegal(Move& move)
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

bool EngineV1_0::castlingMoveIsLegal(Move& move) {
    if (move.legalFlagSet()) {
        return true;
    }

    int16 c = totalHalfmoves % 2;
    int16 color = c << 3;
    int16 e = !color;
    int16 enemy = e << 3;
    int16 castlingRank = move.start() & 0b11111000;

    // Check if anything is attacking squares on king's path
    int16 s;
    int16 end;
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
        int16 file = s % 8;
        int16 ahead = s + 8 - 16 * c;
        if (file != 0 && peices[ahead - 1] == enemy + PAWN) {
            return false;
        }
        if (file != 7 && peices[ahead + 1] == enemy + PAWN) {
            return false;
        }

        // Knight attacks
        for (int16 j = 1; j < KNIGHT_MOVES[s][0]; ++j) {
            if (peices[KNIGHT_MOVES[s][j]] == enemy + KNIGHT) {
                return false;
            }
        }

        // sliding peice attacks
        if (color == BLACK) {
            for (int16 j = s - 8; j >= DIRECTION_BOUNDS[s][B]; j -= 8) {
                if (peices[j]) {
                    if (peices[j] == enemy + ROOK || peices[j] == enemy + QUEEN) {
                        return false;
                    }
                    break;
                }
            }

            for (int16 j = s - 9; j >= DIRECTION_BOUNDS[s][BL]; j -= 9) {
                if (peices[j]) {
                    if (peices[j] == enemy + BISHOP || peices[j] == enemy + QUEEN) {
                        return false;
                    }
                    break;
                }
            }

            for (int16 j = s - 7; j >= DIRECTION_BOUNDS[s][BR]; j -= 7) {
                if (peices[j]) {
                    if (peices[j] == enemy + BISHOP || peices[j] == enemy + QUEEN) {
                        return false;
                    }
                    break;
                }
            }

        }
        else {
            for (int16 j = s + 8; j <= DIRECTION_BOUNDS[s][F]; j += 8) {
                if (peices[j]) {
                    if (peices[j] == enemy + ROOK || peices[j] == enemy + QUEEN) {
                        return false;
                    }
                    break;
                }
            }

            for (int16 j = s + 9; j <= DIRECTION_BOUNDS[s][FR]; j += 9) {
                if (peices[j]) {
                    if (peices[j] == enemy + BISHOP || peices[j] == enemy + QUEEN) {
                        return false;
                    }
                    break;
                }
            }

            for (int16 j = s + 7; j <= DIRECTION_BOUNDS[s][FL]; j += 7) {
                if (peices[j]) {
                    if (peices[j] == enemy + BISHOP || peices[j] == enemy + QUEEN) {
                        return false;
                    }
                    break;
                }
            }
        }

        // King attacks
        for (int16 j = 1; j < KING_MOVES[s][0]; ++j) {
            if (peices[KING_MOVES[s][j]] == enemy + KING) {
                return false;
            }
        }
    }

    move.setLegalFlag();
    return true;
}

//SEARCH/EVAL METHODS
std::uint64_t EngineV1_0::perft_h(int16 depth, Move* moveStack, int16 startMoves)
{
    if (depth == 0) {
        return 1ULL;
    }

    int16 endMoves = startMoves;
    generatePseudoLegalMoves(moveStack, endMoves);

    std::uint64_t nodes = 0;

    for (int16 i = startMoves; i < endMoves; ++i) {
        if (makeMove(moveStack[i])) {
            nodes += perft_h(depth - 1, moveStack, endMoves);
            unmakeMove(moveStack[i]);
        }
    }

    return nodes;
}

int16 EngineV1_0::search_std(int16 plyFromRoot, int16 depth, Move* moveStack, int16 startMoves)
{
    // BASE CASES
    if (depth == 0) {
        return evaluate() * colorToMove();
    }
    if (isDrawByFiftyMoveRule() || repititionOcurred() || isDrawByInsufficientMaterial()) {
        return 0;
    }

    // GENERATE/ORDER MOVES
    int16 endMoves = startMoves;
    generatePseudoLegalMoves(moveStack, endMoves);

    // COMPUTE EVALUATION
    int16 bestEval = INT16_MIN;
    bool zeroLegalMoves = true;

    for (int16 i = startMoves; i < endMoves; ++i) {
        if (makeMove(moveStack[i])) {
            zeroLegalMoves = false;

            int16 eval = -search_std(plyFromRoot + 1, depth - 1, moveStack, endMoves);

            if (eval > bestEval) {
                bestEval = eval;
            }
            unmakeMove(moveStack[i]);
        }
    }

    if (zeroLegalMoves) {
        return inCheck() ? -(MAX_EVAL - plyFromRoot) : 0;
    }

    return bestEval;
}

int16 EngineV1_0::evaluate()
{
    return lazyEvaluation();
}

inline int16 EngineV1_0::lazyEvaluation() const noexcept
{
    return (material_stage_weight * earlygamePositionalMaterialInbalance + (128 - material_stage_weight) * endgamePositionalMaterialInbalance) / 128;
}
