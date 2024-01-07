#include "EngineV1_1.h"

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
#include "precomputed_engine_data.h"

constexpr int32 MAX_EVAL = INT32_MAX;

// TODO add some sort of protection for games with halfmove counter > 500

// PUBLIC METHODS
EngineV1_1::EngineV1_1(const std::string& fenString)
{
    loadFEN(fenString);
}

EngineV1_1::EngineV1_1()
{
    loadStartingPosition();
}

void EngineV1_1::loadStartingPosition()
{
    loadFEN("rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1");
}

void EngineV1_1::loadFEN(const std::string& fenString)
{
    initializeFen(fenString);
}

std::vector<StandardMove> EngineV1_1::getLegalMoves() noexcept
{
    std::vector<StandardMove> moves;

    for (Move& move : enginePositionMoves) {
        moves.emplace_back(move.start(), move.target(), move.promotion());
    }

    return moves;
}

int EngineV1_1::colorToMove() noexcept
{
    return 1 - 2 * (totalHalfmoves % 2);
}

StandardMove EngineV1_1::computerMove(std::chrono::milliseconds thinkTime)
{
    if (gameOver().has_value()) {
        throw std::runtime_error("Game is over, cannot get computer move!");
    }

    // Time management variables
    auto endSearch = std::chrono::high_resolution_clock::now() + thinkTime;
    std::chrono::nanoseconds lastSearchDuration(0);
    std::chrono::milliseconds totalSearchTime(0);

    Move moveStack[1500];
    uint64 totalNodesSearched = 0;

    std::cout << "SEARCH " << asFEN() << std::endl;

    // Order moves
    for (Move& move : enginePositionMoves) {
        MoveOrderer::generateStrengthGuess(this, move);
    }
    std::sort(enginePositionMoves.begin(), enginePositionMoves.end(), [](Move& l, Move& r) { return l.strengthGuess > r.strengthGuess; });

    // Incrementally increase depth until time is up
    for (uint8 depth = 0;; ++depth) {
        // Calculate the cutoff time to halt search based on last search
        auto searchCutoff = endSearch - lastSearchDuration * 1.25;
        auto start = std::chrono::high_resolution_clock::now();

        std::cout << "depth " << (int)depth + 1;

        uint64 nodesSearchedThisIteration = 0;

        int32 alpha = -MAX_EVAL;

        int index = 0;

        // Erase scores from last iteration (stable_sort will maintain order)
        for (Move& move : enginePositionMoves) {
            move.strengthGuess = -MAX_EVAL;
        }

        // Run search for each move
        for (Move& move : enginePositionMoves) {
            if (std::chrono::high_resolution_clock::now() > searchCutoff) {
                std::cout << " timeout";
                break;
            }

            uint32 nodesSearchedThisMove = 0;

            makeMove(move);
            move.strengthGuess = -search_std(1, depth, moveStack, 0, -MAX_EVAL, -alpha, nodesSearchedThisMove);
            unmakeMove(move);

            if (move.strengthGuess > alpha) {
                alpha = move.strengthGuess;
            }

            nodesSearchedThisIteration += nodesSearchedThisMove;
        }

        totalNodesSearched += nodesSearchedThisIteration;

        // Sort moves in order by score
        std::stable_sort(enginePositionMoves.begin(), enginePositionMoves.end(), [](const Move& l, const Move& r) { return l.strengthGuess > r.strengthGuess; });
        std::cout << " bestmove " << enginePositionMoves[0].toString();

        std::cout << " nodes " << nodesSearchedThisIteration;

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
    std::cout << "totalnodes " << totalNodesSearched;
    std::cout << " totaltime " << totalSearchTime.count() << "millis" << std::endl;
    std::cout << bestMove.toString() << std::endl;
    return StandardMove(bestMove.start(), bestMove.target(), bestMove.promotion());
}

void EngineV1_1::inputMove(const StandardMove& move)
{
    if (gameOver().has_value()) {
        throw std::runtime_error("Game is over, cannot input move!");
    }

    for (Move& legalMove : enginePositionMoves) {
        if (legalMove == move) {

            makeMove(legalMove);

            enginePositionMoves = legalMoves();

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

std::optional<int> EngineV1_1::gameOver() noexcept
{
    if (isDraw()) {
        return 0;
    }

    if (enginePositionMoves.size() == 0) {
        return inCheck() ? -colorToMove() : 0;
    }

    return std::nullopt;
}

bool EngineV1_1::inCheck() const noexcept
{
    return inCheck(totalHalfmoves % 2);
}

std::string EngineV1_1::asFEN() const noexcept
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

std::uint64_t EngineV1_1::perft(int depth, bool printOut = false) noexcept
{
    if (depth == 0) {
        return 1ULL;
    }

    Move moveStack[1500];

    if (printOut) {
        std::cout << "PERFT TEST\nFEN: " << asFEN() << std::endl;
    }

    std::uint64_t nodes = 0;

    for (size_t i = 0; i < enginePositionMoves.size(); ++i) {
        std::uint64_t subnodes = 0;

        if (printOut) {
            std::cout << std::setw(2) << i << " *** " << enginePositionMoves[i].toString() << ": ";
            std::cout.flush();
        }

        if (makeMove(enginePositionMoves[i])) {
            subnodes = perft_h(depth - 1, moveStack, 0);
            nodes += subnodes;
            unmakeMove(enginePositionMoves[i]);
        }

        if (printOut) {
            std::cout << subnodes << std::endl;
        }

    }

    if (printOut) {
        std::cout << "TOTAL: " << nodes << std::endl;
    }
    return nodes;
}

std::uint64_t EngineV1_1::search_perft(int depth) noexcept
{
    Move moveStack[1500];
    uint64 totalNodesSearched = 0;

    std::cout << "PERFT SEARCH " << asFEN();
    auto start = std::chrono::high_resolution_clock::now();

    // Order moves
    for (Move& move : enginePositionMoves) {
        MoveOrderer::generateStrengthGuess(this, move);
    }
    std::sort(enginePositionMoves.begin(), enginePositionMoves.end(), [](Move& l, Move& r) { return l.strengthGuess > r.strengthGuess; });

    // Incrementally increase depth until time is up
    for (int d = 0; d < depth; ++d) {
        uint64 nodesSearchedThisIteration = 0;

        int32 alpha = -MAX_EVAL;

        // Erase scores from last iteration (stable_sort will maintain order)
        for (Move& move : enginePositionMoves) {
            move.strengthGuess = -MAX_EVAL;
        }

        // Run search for each move
        for (Move& move : enginePositionMoves) {

            uint32 nodesSearchedThisMove = 0;

            makeMove(move);
            move.strengthGuess = -search_std(1, d, moveStack, 0, -MAX_EVAL, -alpha, nodesSearchedThisMove);
            unmakeMove(move);

            if (move.strengthGuess > alpha) {
                alpha = move.strengthGuess;
            }

            nodesSearchedThisIteration += nodesSearchedThisMove;
        }

        totalNodesSearched += nodesSearchedThisIteration;

        // Sort moves in order by score
        std::stable_sort(enginePositionMoves.begin(), enginePositionMoves.end(), [](const Move& l, const Move& r) { return l.strengthGuess > r.strengthGuess; });
    }

    auto end = std::chrono::high_resolution_clock::now();

    // Print the total nodes and time
    std::cout << " nodes " << totalNodesSearched;
    std::cout << " time " << std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count() << "millis" << std::endl;
    return totalNodesSearched;
}

std::uint64_t EngineV1_1::search_perft(std::chrono::milliseconds thinkTime) noexcept
{
    // Time management variables
    auto endSearch = std::chrono::high_resolution_clock::now() + thinkTime;
    std::chrono::nanoseconds lastSearchDuration(0);

    Move moveStack[1500];
    uint64 totalNodesSearched = 0;

    std::cout << "PERFT SEARCH " << asFEN();
    auto start = std::chrono::high_resolution_clock::now();

    // Order moves
    for (Move& move : enginePositionMoves) {
        MoveOrderer::generateStrengthGuess(this, move);
    }
    std::sort(enginePositionMoves.begin(), enginePositionMoves.end(), [](Move& l, Move& r) { return l.strengthGuess > r.strengthGuess; });

    // Incrementally increase depth until time is up
    for (uint8 depth = 0;; ++depth) {
        // Calculate the cutoff time to halt search based on last search
        auto searchCutoff = endSearch - lastSearchDuration * 1.25;
        auto start = std::chrono::high_resolution_clock::now();

        uint64 nodesSearchedThisIteration = 0;

        int32 alpha = -MAX_EVAL;

        // Erase scores from last iteration (stable_sort will maintain order)
        for (Move& move : enginePositionMoves) {
            move.strengthGuess = -MAX_EVAL;
        }

        // Run search for each move
        for (Move& move : enginePositionMoves) {
            if (std::chrono::high_resolution_clock::now() > searchCutoff) {
                break;
            }

            uint32 nodesSearchedThisMove = 0;

            makeMove(move);
            move.strengthGuess = -search_std(1, depth, moveStack, 0, -MAX_EVAL, -alpha, nodesSearchedThisMove);
            unmakeMove(move);

            if (move.strengthGuess > alpha) {
                alpha = move.strengthGuess;
            }

            nodesSearchedThisIteration += nodesSearchedThisMove;
        }

        totalNodesSearched += nodesSearchedThisIteration;

        // Sort moves in order by score
        std::stable_sort(enginePositionMoves.begin(), enginePositionMoves.end(), [](const Move& l, const Move& r) { return l.strengthGuess > r.strengthGuess; });

        if (std::chrono::high_resolution_clock::now() > searchCutoff) {
            break;
        }
    }

    auto end = std::chrono::high_resolution_clock::now();

    // Print the total nodes and time
    std::cout << " nodes " << totalNodesSearched;
    std::cout << " time " << std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count() << "millis" << std::endl;
    return totalNodesSearched;
}

void EngineV1_1::printZobrist()
{
    std::cout << zobrist << std::endl;
}

// MOVE STRUCT
// PUBLIC METHODS
inline uint8 EngineV1_1::Move::start() const noexcept
{
    return startSquare;
}

inline uint8 EngineV1_1::Move::target() const noexcept
{
    return targetSquare;
}

inline uint8 EngineV1_1::Move::moving() const noexcept
{
    return movingPeice;
}

inline uint8 EngineV1_1::Move::captured() const noexcept
{
    return capturedPeice;
}

inline uint8 EngineV1_1::Move::color() const noexcept
{
    return (movingPeice >> 3) << 3;
}

inline uint8 EngineV1_1::Move::enemy() const noexcept
{
    return !(movingPeice >> 3) << 3;
}

inline uint8 EngineV1_1::Move::promotion() const noexcept
{
    return flags & PROMOTION;
}

inline bool EngineV1_1::Move::isEnPassant() const noexcept
{
    return flags & EN_PASSANT;
}

inline bool EngineV1_1::Move::isCastling() const noexcept
{
    return flags & CASTLE;
}

inline bool EngineV1_1::Move::legalFlagSet() const noexcept
{
    return flags & LEGAL;
}

inline void EngineV1_1::Move::setLegalFlag() noexcept
{
    flags |= LEGAL;
}

inline void EngineV1_1::Move::unSetLegalFlag() noexcept
{
    flags |= LEGAL;
    flags ^= LEGAL;
}

inline int32 EngineV1_1::Move::earlygamePositionalMaterialChange() noexcept
{
    if (!posmatInit) {
        initializePosmat();
    }
    return earlyPosmat;
}

inline int32 EngineV1_1::Move::endgamePositionalMaterialChange() noexcept
{
    if (!posmatInit) {
        initializePosmat();
    }
    return endPosmat;
}

inline bool EngineV1_1::Move::operator==(const EngineV1_1::Move& other) const
{
    return this->start() == other.start()
        && this->target() == other.target()
        && this->promotion() == other.promotion();
}

inline bool EngineV1_1::Move::operator==(const StandardMove& other) const
{
    return this->start() == other.startSquare
        && this->target() == other.targetSquare
        && this->promotion() == other.promotion;
}

std::string EngineV1_1::Move::toString() const
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
EngineV1_1::Move::Move(const EngineV1_1* board, uint8 start, uint8 target, uint8 givenFlags) : startSquare(start), targetSquare(target), flags(givenFlags), strengthGuess(0), posmatInit(false), earlyPosmat(0), endPosmat(0)
{
    movingPeice = board->peices[start];
    capturedPeice = board->peices[target];
    if (isEnPassant()) {
        capturedPeice = enemy() + PAWN;
    }
}

EngineV1_1::Move::Move() : startSquare(0), targetSquare(0), movingPeice(0), capturedPeice(0), flags(0), strengthGuess(0), posmatInit(false), earlyPosmat(0), endPosmat(0) {}

// PRIVATE METHODS
void EngineV1_1::Move::initializePosmat() noexcept
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
        uint8 captureSquare = isEnPassant() ? target() - 8 + 16 * (color() >> 3) : target();
        earlyPosmat -= EARLYGAME_PEICE_VALUE[captured()][captureSquare];
        endPosmat -= ENDGAME_PEICE_VALUE[captured()][captureSquare];
    }

    // Update rooks for castling
    if (isCastling()) {
        uint8 rookStart;
        uint8 rookEnd;

        uint8 castlingRank = target() & 0b11111000;
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
void EngineV1_1::initializeFen(const std::string& fenString)
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

    for (uint8 i = 0; i < MAX_DEPTH + 50; ++i) {
        positionInfo[i] = 0;
    }

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
            material_stage_weight += PEICE_STAGE_WEIGHTS[peice];
            earlygamePositionalMaterialInbalance += EARLYGAME_PEICE_VALUE[peice][i];
            endgamePositionalMaterialInbalance += ENDGAME_PEICE_VALUE[peice][i];
        }
    }

    positionInfo[positionInfoIndex] |= zobrist >> 44;

    uint8 c = totalHalfmoves % 2;

    enginePositionMoves = legalMoves();
}


bool EngineV1_1::generatePseudoLegalMoves(Move* stack, uint32& idx, bool generateOnlyCaptures) noexcept
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

    if (!checks && generateOnlyCaptures) {
        generateCaptures(stack, idx, isPinned);
        return false;
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

std::vector<EngineV1_1::Move> EngineV1_1::legalMoves()
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

void EngineV1_1::generateCaptures(Move* stack, uint32& idx, bool* pinnedPeices) const
{
    uint8 c = totalHalfmoves % 2;
    uint8 color = c << 3;
    uint8 e = !color;
    uint8 enemy = e << 3;

    for (uint8 s = 0; s < 64; ++s) {
        if (peices[s] && peices[s] >> 3 == c) {
            uint8 legalFlag = pinnedPeices[s] ? Move::NONE : Move::LEGAL;

            switch (peices[s] & 0b111) {
            case PAWN: {
                uint8 file = s % 8;
                uint8 ahead = s + 8 - 16 * c;
                bool promotion = color == WHITE ? (s >> 3 == 6) : (s >> 3 == 1);

                // Pawn captures
                if (file != 0 && peices[ahead - 1] && peices[ahead - 1] >> 3 == e) {
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
                if (file != 7 && peices[ahead + 1] && peices[ahead + 1] >> 3 == e) {
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
                    if (peices[t] && peices[t] >> 3 == e) {
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
                    if (!peices[t]) {
                        continue;
                    }
                    if (peices[t] >> 3 == e) {
                        stack[idx++] = Move(this, s, t, legalFlag);
                    }
                    break;
                }

                for (int8 t = s + 8; t <= DIRECTION_BOUNDS[s][F]; t += 8) {
                    if (!peices[t]) {
                        continue;
                    }
                    if (peices[t] >> 3 == e) {
                        stack[idx++] = Move(this, s, t, legalFlag);
                    }
                    break;
                }

                for (int8 t = s - 1; t >= DIRECTION_BOUNDS[s][L]; t -= 1) {
                    if (!peices[t]) {
                        continue;
                    }
                    if (peices[t] >> 3 == e) {
                        stack[idx++] = Move(this, s, t, legalFlag);
                    }
                    break;
                }

                for (int8 t = s + 1; t <= DIRECTION_BOUNDS[s][R]; t += 1) {
                    if (!peices[t]) {
                        continue;
                    }
                    if (peices[t] >> 3 == e) {
                        stack[idx++] = Move(this, s, t, legalFlag);
                    }
                    break;
                }

                if ((peices[s] & 0b111) == ROOK) {
                    break;
                }
            bishopMoves:

                for (int8 t = s - 9; t >= DIRECTION_BOUNDS[s][BL]; t -= 9) {
                    if (!peices[t]) {
                        continue;
                    }
                    if (peices[t] >> 3 == e) {
                        stack[idx++] = Move(this, s, t, legalFlag);
                    }
                    break;
                }

                for (int8 t = s + 9; t <= DIRECTION_BOUNDS[s][FR]; t += 9) {
                    if (!peices[t]) {
                        continue;
                    }
                    if (peices[t] >> 3 == e) {
                        stack[idx++] = Move(this, s, t, legalFlag);
                    }
                    break;
                }

                for (int8 t = s - 7; t >= DIRECTION_BOUNDS[s][BR]; t -= 7) {
                    if (!peices[t]) {
                        continue;
                    }
                    if (peices[t] >> 3 == e) {
                        stack[idx++] = Move(this, s, t, legalFlag);
                    }
                    break;
                }

                for (int8 t = s + 7; t <= DIRECTION_BOUNDS[s][FL]; t += 7) {
                    if (!peices[t]) {
                        continue;
                    }
                    if (peices[t] >> 3 == e) {
                        stack[idx++] = Move(this, s, t, legalFlag);
                    }
                    break;
                }
                break;
            }
            case KING: {
                uint8 t;
                for (uint8 j = 1; j < KING_MOVES[s][0]; ++j) {
                    t = KING_MOVES[s][j];
                    if (peices[t] && peices[t] >> 3 == e) {
                        stack[idx++] = Move(this, s, t, Move::NONE);
                    }
                }
            }
            }
        }
    }
}

bool EngineV1_1::makeMove(EngineV1_1::Move& move)
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

    earlygamePositionalMaterialInbalance += move.earlygamePositionalMaterialChange();
    endgamePositionalMaterialInbalance += move.endgamePositionalMaterialChange();

    // Update zobrist hash, numpieces and positonal imbalance for moving peice
    zobrist ^= ZOBRIST_PEICE_KEYS[c][(move.moving() & 0b111) - 1][move.start()];
    if (move.promotion()) {
        zobrist ^= ZOBRIST_PEICE_KEYS[c][move.promotion() - 1][move.target()];
        --numPeices[move.moving()];
        ++numPeices[color + move.promotion()];
        material_stage_weight -= PEICE_STAGE_WEIGHTS[move.moving()];
        material_stage_weight += PEICE_STAGE_WEIGHTS[color + move.promotion()];
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
        material_stage_weight -= PEICE_STAGE_WEIGHTS[move.captured()];
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

void EngineV1_1::unmakeMove(EngineV1_1::Move& move)
{
    uint8 c = move.moving() >> 3;
    uint8 color = c << 3;
    uint8 e = !color;
    uint8 enemy = e << 3;

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
        zobrist ^= ZOBRIST_PEICE_KEYS[c][(move.moving() & 0b111) - 1][move.target()];
    }
    zobrist ^= ZOBRIST_PEICE_KEYS[c][(move.moving() & 0b111) - 1][move.start()];

    // Undo zobrist hash and peice indices set for capture
    if (move.captured()) {
        uint8 captureSquare = move.isEnPassant() ? move.target() - 8 + 16 * c : move.target();
        zobrist ^= ZOBRIST_PEICE_KEYS[e][(move.captured() & 0b111) - 1][captureSquare];
        material_stage_weight += PEICE_STAGE_WEIGHTS[move.captured()];
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

bool EngineV1_1::isDraw() const
{
    return isDrawByFiftyMoveRule() || isDrawByInsufficientMaterial() || isDrawByThreefoldRepitition();
}

uint8 EngineV1_1::halfMovesSincePawnMoveOrCapture() const noexcept
{
    return static_cast<uint8>((positionInfo[positionInfoIndex] >> 20) & 0b111111);
}

uint8 EngineV1_1::eligibleEnpassantSquare() const noexcept
{
    return static_cast<uint8>(positionInfo[positionInfoIndex] >> 26);
}

bool EngineV1_1::isDrawByThreefoldRepitition() const noexcept
{
    if (halfMovesSincePawnMoveOrCapture() < 8) {
        return false;
    }

    uint8 index = positionInfoIndex - 2;
    uint8 numPossibleRepitions = halfMovesSincePawnMoveOrCapture() / 2 - 1;

    uint32 currentHash = static_cast<uint32>(zobrist << 44);
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

inline bool EngineV1_1::isDrawByFiftyMoveRule() const noexcept
{
    return halfMovesSincePawnMoveOrCapture() >= 50;
}

bool EngineV1_1::isDrawByInsufficientMaterial() const noexcept
{
    if (numTotalPeices[0] > 3 || numTotalPeices[1] > 3) {
        return false;
    }
    if (numTotalPeices[0] == 3 || numTotalPeices[1] == 3) {
        return (numPeices[WHITE + KNIGHT] == 2 || numPeices[BLACK + KNIGHT] == 2) && (numTotalPeices[0] == 1 || numTotalPeices[1] == 1);
    }
    return !(numPeices[WHITE + PAWN] || numPeices[BLACK + PAWN] || numPeices[WHITE + ROOK] || numPeices[BLACK + ROOK] || numPeices[WHITE + QUEEN] || numPeices[BLACK + QUEEN]);
}

bool EngineV1_1::repititionOcurred() const noexcept
{
    if (halfMovesSincePawnMoveOrCapture() < 4) {
        return false;
    }

    uint8 index = positionInfoIndex - 2;
    uint8 numPossibleRepitions = halfMovesSincePawnMoveOrCapture() / 2 - 1;

    uint32 currentHash = static_cast<uint32>(zobrist << 44);

    for (uint8 i = 0; i < numPossibleRepitions; ++i) {
        index -= 2;
        if ((positionInfo[index] & ((1 << 20) - 1)) == currentHash) {
            return true;
        }
    }
    return false;
}

bool EngineV1_1::inCheck(uint8 c) const
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

bool EngineV1_1::isLegal(Move& move)
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

bool EngineV1_1::castlingMoveIsLegal(Move& move) {
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

//SEARCH/EVAL METHODS
std::uint64_t EngineV1_1::perft_h(uint8 depth, Move* moveStack, uint32 startMoves)
{
    if (depth == 0) {
        return 1ULL;
    }

    uint32 endMoves = startMoves;
    generatePseudoLegalMoves(moveStack, endMoves);

    std::uint64_t nodes = 0;

    for (uint32 i = startMoves; i < endMoves; ++i) {
        if (makeMove(moveStack[i])) {
            nodes += perft_h(depth - 1, moveStack, endMoves);
            unmakeMove(moveStack[i]);
        }
    }

    return nodes;
}

int32 EngineV1_1::search_std(uint8 plyFromRoot, uint8 depth, Move* moveStack, uint32 startMoves, int32 alpha, int32 beta, uint32& nodesSearched)
{
    ++nodesSearched;
    // BASE CASES
    if (isDrawByFiftyMoveRule() || repititionOcurred() || isDrawByInsufficientMaterial()) {
        return 0;
    }
    if (depth == 0) {
        return search_quiscence(moveStack, startMoves, alpha, beta, nodesSearched);
    }

    // GENERATE/ORDER MOVES
    uint32 endMoves = startMoves;
    generatePseudoLegalMoves(moveStack, endMoves);
    MoveOrderer pseudoLegalMoves(this, moveStack, startMoves, endMoves);

    // COMPUTE EVALUATION
    int32 best_eval = -MAX_EVAL;
    //int32 originalAplha = alpha;
    //uint32 nodesBeforeSearch = nodesSearched;
    bool zeroLegalMoves = true;

    for (Move& move : pseudoLegalMoves) {
        if (makeMove(move)) {
            zeroLegalMoves = false;

            int32 eval = -search_std(plyFromRoot + 1, depth - 1, moveStack, endMoves, -beta, -alpha, nodesSearched);

            unmakeMove(move);

            if (eval >= beta) {
                return eval; // Cut node (lower bound)
            }
            if (eval > best_eval) {
                best_eval = eval;

                if (eval > alpha) {
                    alpha = eval;
                }
            }
        }
    }

    if (zeroLegalMoves) {
        return inCheck() ? -MAX_EVAL + plyFromRoot : 0;
    }

    //if (alpha == originalAplha) All Node (upper bound)

    return best_eval; // PV node (exact value)
}

int32 EngineV1_1::search_quiscence(Move* moveStack, uint32 startMoves, int32 alpha, int32 beta, uint32& nodesSearched)
{
    ++nodesSearched;
    // GENERATE QUISCENCE MOVES
    uint32 endMoves = startMoves;
    bool inCheck = generatePseudoLegalMoves(moveStack, endMoves, true);

    // STATIC EVALUATION
    int32 best_eval;
    if (inCheck) {
        best_eval = -MAX_EVAL;
    }
    else {
        best_eval = evaluate() * colorToMove();

        if (best_eval >= beta) {
            return best_eval;
        }
        if (best_eval > alpha) {
            alpha = best_eval;
        }
    }

    // ORDER MOVES
    MoveOrderer pseudoLegalMoves(this, moveStack, startMoves, endMoves);

    // SEARCH
    for (Move& move : pseudoLegalMoves) {
        if (makeMove(move)) {

            int32 eval = isDrawByInsufficientMaterial() ? 0 : -search_quiscence(moveStack, endMoves, -beta, -alpha, nodesSearched);

            unmakeMove(move);

            if (eval >= beta) {
                return eval;
            }
            if (eval > best_eval) {
                best_eval = eval;

                if (eval > alpha) {
                    alpha = eval;
                }
            }
        }
    }

    return best_eval;
}

int32 EngineV1_1::evaluate()
{
    return lazyEvaluation();
}

inline int32 EngineV1_1::lazyEvaluation() const noexcept
{
    return (material_stage_weight * earlygamePositionalMaterialInbalance + (128 - material_stage_weight) * endgamePositionalMaterialInbalance) / 128;
}

// MOVE ORDERING CLASS
EngineV1_1::MoveOrderer::MoveOrderer(EngineV1_1* engine, Move* moveStack, uint32 startMoves, uint32 endMoves, Move* skipThisMove) : moveStack(moveStack), startMoves(startMoves), endMoves(endMoves)
{
    if (skipThisMove != nullptr) {
        for (uint32 i = startMoves; i < endMoves; ++i) {
            if (moveStack[i] == *skipThisMove) {
                std::swap(moveStack[i], moveStack[--endMoves]);
            }
        }
    }

    for (uint32 i = startMoves; i < endMoves; ++i) {
        generateStrengthGuess(engine, moveStack[i]);
    }
}

void EngineV1_1::MoveOrderer::generateStrengthGuess(EngineV1_1* engine, EngineV1_1::Move& move)
{
    /*
    if (move.promotion()) {
        move.strengthGuess = move.promotion() * 2;
        return;
    }

    int32 earlyGameScore = move.earlygamePositionalMaterialChange() << 1;
    int32 endGameScore = move.endgamePositionalMaterialChange() << 1;

    if (move.captured()) {
        earlyGameScore -= EARLYGAME_PEICE_VALUE[move.moving()][move.target()];
        endGameScore -= ENDGAME_PEICE_VALUE[move.moving()][move.target()];
    }

    int32 score = engine->colorToMove() * (engine->material_stage_weight * earlyGameScore + (128 - engine->material_stage_weight) * endGameScore) >> 8;

    //score += move.captured();
    */

    int32 score = (engine->material_stage_weight * move.earlygamePositionalMaterialChange() + (128 - engine->material_stage_weight) * move.endgamePositionalMaterialChange()) >> 7;

    move.strengthGuess = score * engine->colorToMove();
}

EngineV1_1::MoveOrderer::Iterator EngineV1_1::MoveOrderer::begin()
{
    uint32 maxIndex = startMoves;
    int32 maxStrength = moveStack[startMoves].strengthGuess;

    for (uint32 i = startMoves + 1; i < endMoves; ++i) {
        if (moveStack[i].strengthGuess > maxStrength) {
            maxStrength = moveStack[i].strengthGuess;
            maxIndex = i;
        }
    }

    if (maxIndex != startMoves) {
        std::swap(moveStack[startMoves], moveStack[maxIndex]);
    }

    return Iterator(moveStack + startMoves, endMoves - startMoves, 0);
}

EngineV1_1::MoveOrderer::Iterator EngineV1_1::MoveOrderer::end()
{
    return Iterator(moveStack + startMoves, endMoves - startMoves, endMoves - startMoves);
}

EngineV1_1::MoveOrderer::Iterator::Iterator(Move* start, uint32 size, uint32 currentIndex) : start(start), size(size), idx(currentIndex) {}

EngineV1_1::Move& EngineV1_1::MoveOrderer::Iterator::operator*()
{
    return start[idx];
}

EngineV1_1::MoveOrderer::Iterator& EngineV1_1::MoveOrderer::Iterator::operator++() {
    uint32 maxIndex = ++idx;
    int32 maxStrength = start[idx].strengthGuess;

    for (uint32 i = idx + 1; i < size; ++i) {
        if (start[i].strengthGuess > maxStrength) {
            maxStrength = start[i].strengthGuess;
            maxIndex = i;
        }
    }

    if (maxIndex != idx) {
        std::swap(start[idx], start[maxIndex]);
    }

    return *this;
}

bool EngineV1_1::MoveOrderer::Iterator::operator!=(const Iterator& other) const
{
    return idx != other.idx;
}
// END MOVE ORDERING CLASS
