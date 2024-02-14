#include "Game.h"

#include <cstdint>
#include <optional>
#include <vector>
#include <algorithm>
#include <stdexcept>
#include <string>
#include <sstream>
#include <cctype>
#include <chrono>
#include <iomanip>

#include "StandardMove.h"
#include "precomputed_chess_data.h"
#include "chesshelpers.h"


typedef std::int_fast8_t int8;
typedef std::uint_fast8_t uint8;
typedef std::int_fast32_t int32;
typedef std::uint_fast32_t uint32;
typedef std::uint_fast64_t uint64;

constexpr int32 MAX_EVAL = INT32_MAX;

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
    return currentLegalMoves;
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

    if (std::find(currentLegalMoves.begin(), currentLegalMoves.end(), move) == currentLegalMoves.end()) {
        throw std::runtime_error("inputted move is not legal in the current position!");
    }

    uint8 moving = peices[move.startSquare];
    uint8 c = moving >> 3;
    uint8 color = c << 3;
    uint8 e = !color;
    uint8 enemy = e << 3;
    uint8 captured;
    if (move.targetSquare == eligibleEnPassantSquare) {
        captured = enemy + PAWN;
    }
    else {
        captured = peices[move.targetSquare];
    }

    // UPDATE PEICE DATA / ZOBRIST HASH
    // Update zobrist hash for turn change
    zobrist ^= ZOBRIST_TURN_KEY;

    // Update zobrist hash, numpieces and positonal imbalance for moving peice
    peices[move.startSquare] = 0;
    zobrist ^= ZOBRIST_PEICE_KEYS[c][(moving & 0b111) - 1][move.startSquare];

    if (move.promotion) {
        peices[move.targetSquare] = color + move.promotion;
        zobrist ^= ZOBRIST_PEICE_KEYS[c][move.promotion - 1][move.targetSquare];
        --numPeices[moving];
        ++numPeices[color + move.promotion];
    }
    else {
        peices[move.targetSquare] = moving;
        zobrist ^= ZOBRIST_PEICE_KEYS[c][(moving & 0b111) - 1][move.targetSquare];
    }

    // Update zobrist hash and peice indices set for capture
    if (captured) {
        if (move.targetSquare == eligibleEnPassantSquare) {
            peices[move.targetSquare - 8 + 16 * c] = 0;
            zobrist ^= ZOBRIST_PEICE_KEYS[e][(captured & 0b111) - 1][move.targetSquare - 8 + 16 * c];
        }
        zobrist ^= ZOBRIST_PEICE_KEYS[e][(captured & 0b111) - 1][move.targetSquare];
        --numPeices[captured];
        --numTotalPeices[e];
    }

    // Update rooks for castling
    if (moving == color + KING && std::abs(move.startSquare - move.targetSquare) == 2) {
        uint8 rookStart;
        uint8 rookEnd;

        uint8 castlingRank = move.targetSquare & 0b11111000;
        if (move.targetSquare % 8 < 4) {
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

    if (captured || moving == color + PAWN) {
        halfmovesSincePawnMoveOrCapture = 0;
    }
    else {
        positionHistory[halfmovesSincePawnMoveOrCapture] = static_cast<uint32>(zobrist);
        ++halfmovesSincePawnMoveOrCapture;
    }

    // En passant file
    if ((moving & 0b111) == PAWN && std::abs(move.targetSquare - move.startSquare) == 16) {
        eligibleEnPassantSquare = (move.startSquare + move.targetSquare) / 2;
    }
    else {
        eligibleEnPassantSquare = 0;
    }

    // update castling rights
    if (canKingsideCastle[c]) {
        if (moving == color + KING || (moving == color + ROOK && (color == WHITE ? move.startSquare == 7 : move.startSquare == 63))) {
            canKingsideCastle[c] = false;
            zobrist ^= ZOBRIST_KINGSIDE_CASTLING_KEYS[c];
        }
    }
    if (canQueensideCastle[c]) {
        if (moving == color + KING || (moving == color + ROOK && (color == WHITE ? move.startSquare == 0 : move.startSquare == 56))) {
            canQueensideCastle[c] = false;
            zobrist ^= ZOBRIST_QUEENSIDE_CASTLING_KEYS[c];
        }
    }
    if (canKingsideCastle[e]) {
        if (color == BLACK ? move.targetSquare == 7 : move.targetSquare == 63) {
            canKingsideCastle[e] = false;
            zobrist ^= ZOBRIST_KINGSIDE_CASTLING_KEYS[e];
        }
    }
    if (canQueensideCastle[e]) {
        if (color == BLACK ? move.targetSquare == 0 : move.targetSquare == 56) {
            canQueensideCastle[e] = false;
            zobrist ^= ZOBRIST_QUEENSIDE_CASTLING_KEYS[e];
        }
    }

    // Update king index
    if ((moving & 0b111) == KING) {
        kingIndex[c] = move.targetSquare;
    }

    gameMoves.push_back(move);

    // ALGEBRAIC NOTATION
    if (moving == color + KING && move.targetSquare - move.startSquare == -2) {
        gameMovesInAlgebraicNotation.push_back("O-O-O");
        currentLegalMoves = legalMoves();
        return;
    }
    if (moving == color + KING && move.targetSquare - move.startSquare == 2) {
        gameMovesInAlgebraicNotation.push_back("O-O");
        currentLegalMoves = legalMoves();
        return;
    }

    std::string algebraic = "";

    if (moving == color + PAWN) {
        if (captured) {
            algebraic += chesshelpers::boardIndexToAlgebraicNotation(move.startSquare)[0];
        }
    }
    else {
        char algebraicPeiceIndentifiers[] = { 'N', 'B', 'R', 'Q', 'K' };
        algebraic += algebraicPeiceIndentifiers[(moving & 0b111) - 2];

        // Disambiguation
        std::vector<uint8> ambiguousSquares;
        for (StandardMove& otherMove : currentLegalMoves) {
            if (peices[otherMove.startSquare] == moving && otherMove.targetSquare == move.targetSquare && otherMove.startSquare != move.startSquare) {
                ambiguousSquares.push_back(otherMove.startSquare);
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
    
    currentLegalMoves = legalMoves();

    if (captured) {
        algebraic += 'x';
    }

    algebraic += chesshelpers::boardIndexToAlgebraicNotation(move.targetSquare);

    if (move.promotion) {
        char algebraicPeiceIndentifiers[] = { 'N', 'B', 'R', 'Q' };
        algebraic += '=';
        algebraic += algebraicPeiceIndentifiers[move.promotion - 2];
    }

    if (inCheck()) {
        algebraic += currentLegalMoves.empty() ? '#' : '+';
    }

    gameMovesInAlgebraicNotation.push_back(algebraic);
}

std::optional<int> Game::gameOver() noexcept
{
    if (isDrawByFiftyMoveRule() || isDrawByInsufficientMaterial() || isDrawByThreefoldRepitition()) {
        return 0;
    }

    if (currentLegalMoves.size() == 0) {
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
    if (canKingsideCastle[0]) {
        castlingAvailability += 'K';
    }
    if (canQueensideCastle[0]) {
        castlingAvailability += 'Q';
    }
    if (canKingsideCastle[1]) {
        castlingAvailability += 'k';
    }
    if (canQueensideCastle[1]) {
        castlingAvailability += 'q';
    }

    if (castlingAvailability.size() == 0) {
        fen += "- ";
    }
    else {
        fen += castlingAvailability + " ";
    }

    // En passant target
    if (eligibleEnPassantSquare) {
        fen += chesshelpers::boardIndexToAlgebraicNotation(eligibleEnPassantSquare) + " ";
    }
    else {
        fen += "- ";
    }

    // Halfmoves since pawn move or capture
    fen += std::to_string(halfmovesSincePawnMoveOrCapture);
    fen += ' ';

    // Total moves
    fen += std::to_string(totalHalfmoves / 2 + 1);

    return fen;
}

std::string Game::asPGN(std::map<std::string, std::string> headers) noexcept
{
    if (headers.find("Event") == headers.end()) headers["Event"] = "??";
    if (headers.find("Date") == headers.end()) headers["Date"] = getCurrentDate();
    if (headers.find("White") == headers.end()) headers["White"] = "??";
    if (headers.find("Black") == headers.end()) headers["Black"] = "??";
    if (headers.find("Termination") == headers.end()) headers["Termination"] = gameOver().has_value() ? "Normal" : "Forfeit";

    std::string resultStr;
    if (headers.find("Result") == headers.end()) {
        int result = gameOver().has_value() ? gameOver().value() : -colorToMove();
        resultStr = (result == 1) ? "1-0" : (result == -1) ? "0-1" : "1/2-1/2";
        headers["Result"] = resultStr;
    }
    else {
        resultStr = headers["Result"];
    }

    std::string pgn;

    // Add headers
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

    gameMoves.clear();
    gameMovesInAlgebraicNotation.clear();

    // PARSE FEN
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
    canKingsideCastle[0] = false;
    canKingsideCastle[1] = false;
    canQueensideCastle[0] = false;
    canQueensideCastle[1] = false;

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
                    canKingsideCastle[c] = true;
                    zobrist ^= ZOBRIST_KINGSIDE_CASTLING_KEYS[c];
                }
                break;
            case 'Q':
            case 'q':
                if (peices[castlingRank + 4] == color + KING && peices[castlingRank] == color + ROOK) {
                    canQueensideCastle[c] = true;
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
        halfmovesSincePawnMoveOrCapture = static_cast<uint8>(std::stoi(halfmoveClock));
    }
    catch (const std::invalid_argument& e) {
        throw std::invalid_argument(std::string("Invalid FEN half move clock! ") + e.what());
    }

    // wait until halfmove clock are defined before defining
    if (enPassantTarget != "-") {
        try {
            eligibleEnPassantSquare = chesshelpers::algebraicNotationToBoardIndex(enPassantTarget);
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

    currentLegalMoves = legalMoves();
}

std::vector<StandardMove> Game::legalMoves()
{
    int c = totalHalfmoves % 2;
    int color = c << 3;
    int e = !color;
    int enemy = e << 3;

    std::vector<StandardMove> moves;

    // General case
    for (int s = 0; s < 64; ++s) {
        if (peices[s] && peices[s] >> 3 == c) {

            switch (peices[s] % (1 << 3)) {
            case PAWN: {
                int file = s % 8;
                int ahead = s + 8 - 16 * c;
                bool promotion = color == WHITE ? (s >> 3 == 6) : (s >> 3 == 1);

                // Pawn foward moves
                if (!peices[ahead]) {
                    if (promotion) {
                        if (!GENERATE_ONLY_QUEEN_PROMOTIONS) {
                            moves.emplace_back(s, ahead, KNIGHT);
                            moves.emplace_back(s, ahead, BISHOP);
                            moves.emplace_back(s, ahead, ROOK);
                        }
                        moves.emplace_back(s, ahead, QUEEN);
                    }
                    else {
                        moves.emplace_back(s, ahead);
                    }

                    bool doubleJumpAllowed = color == WHITE ? (s >> 3 == 1) : (s >> 3 == 6);
                    int doubleAhead = ahead + 8 - 16 * c;
                    if (doubleJumpAllowed && !peices[doubleAhead]) {
                        moves.emplace_back(s, doubleAhead);
                    }
                }

                // Pawn captures
                if (file != 0 && peices[ahead - 1] && peices[ahead - 1] >> 3 == e) {
                    if (promotion) {
                        if (!GENERATE_ONLY_QUEEN_PROMOTIONS) {
                            moves.emplace_back(s, ahead - 1, KNIGHT);
                            moves.emplace_back(s, ahead - 1, BISHOP);
                            moves.emplace_back(s, ahead - 1, ROOK);
                        }
                        moves.emplace_back(s, ahead - 1, QUEEN);
                    }
                    else {
                        moves.emplace_back(s, ahead - 1);
                    }
                }
                if (file != 7 && peices[ahead + 1] && peices[ahead + 1] >> 3 == e) {
                    if (promotion) {
                        if (!GENERATE_ONLY_QUEEN_PROMOTIONS) {
                            moves.emplace_back(s, ahead + 1, KNIGHT);
                            moves.emplace_back(s, ahead + 1, BISHOP);
                            moves.emplace_back(s, ahead + 1, ROOK);
                        }
                        moves.emplace_back(s, ahead + 1, QUEEN);
                    }
                    else {
                        moves.emplace_back(s, ahead + 1);
                    }
                }
                break;
            }
            case KNIGHT: {
                int t;
                for (int j = 1; j < KNIGHT_MOVES[s][0]; ++j) {
                    t = KNIGHT_MOVES[s][j];
                    if ((!peices[t] || peices[t] >> 3 == e)) {
                        moves.emplace_back(s, t);
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

                for (int t = s - 8; t >= DIRECTION_BOUNDS[s][B]; t -= 8) {
                    if ((!peices[t] || peices[t] >> 3 == e)) {
                        moves.emplace_back(s, t);
                    }
                    if (peices[t]) {
                        break;
                    }
                }

                for (int t = s + 8; t <= DIRECTION_BOUNDS[s][F]; t += 8) {
                    if ((!peices[t] || peices[t] >> 3 == e)) {
                        moves.emplace_back(s, t);
                    }
                    if (peices[t]) {
                        break;
                    }
                }

                for (int t = s - 1; t >= DIRECTION_BOUNDS[s][L]; t -= 1) {
                    if ((!peices[t] || peices[t] >> 3 == e)) {
                        moves.emplace_back(s, t);
                    }
                    if (peices[t]) {
                        break;
                    }
                }

                for (int t = s + 1; t <= DIRECTION_BOUNDS[s][R]; t += 1) {
                    if ((!peices[t] || peices[t] >> 3 == e)) {
                        moves.emplace_back(s, t);
                    }
                    if (peices[t]) {
                        break;
                    }
                }

                if (peices[s] % (1 << 3) == ROOK) {
                    break;
                }
            bishopMoves:

                for (int t = s - 9; t >= DIRECTION_BOUNDS[s][BL]; t -= 9) {
                    if ((!peices[t] || peices[t] >> 3 == e)) {
                        moves.emplace_back(s, t);
                    }
                    if (peices[t]) {
                        break;
                    }
                }

                for (int t = s + 9; t <= DIRECTION_BOUNDS[s][FR]; t += 9) {
                    if ((!peices[t] || peices[t] >> 3 == e)) {
                        moves.emplace_back(s, t);
                    }
                    if (peices[t]) {
                        break;
                    }
                }

                for (int t = s - 7; t >= DIRECTION_BOUNDS[s][BR]; t -= 7) {
                    if ((!peices[t] || peices[t] >> 3 == e)) {
                        moves.emplace_back(s, t);
                    }
                    if (peices[t]) {
                        break;
                    }
                }

                for (int t = s + 7; t <= DIRECTION_BOUNDS[s][FL]; t += 7) {
                    if ((!peices[t] || peices[t] >> 3 == e)) {
                        moves.emplace_back(s, t);
                    }
                    if (peices[t]) {
                        break;
                    }
                }

                break;
            }
            case KING: {
                int t;
                for (int j = 1; j < KING_MOVES[s][0]; ++j) {
                    t = KING_MOVES[s][j];
                    if (!peices[t] || peices[t] >> 3 == e) {
                        moves.emplace_back(s, t);
                    }
                }
            }
            }
        }
    }

    // Castling moves
    if (canKingsideCastle[c]) {
        int castlingRank = 56 * c;
        bool roomToCastle = true;
        for (int j = castlingRank + 5; j < castlingRank + 7; ++j) {
            if (peices[j]) {
                roomToCastle = false;
                break;
            }
        }
        if (roomToCastle) {
            moves.emplace_back(castlingRank + 4, castlingRank + 6);
        }
    }
    if (canQueensideCastle[c]) {
        int castlingRank = 56 * c;
        bool roomToCastle = true;
        for (int j = castlingRank + 3; j > castlingRank; --j) {
            if (peices[j]) {
                roomToCastle = false;
                break;
            }
        }
        if (roomToCastle) {
            moves.emplace_back(castlingRank + 4, castlingRank + 2);
        }
    }

    // En passant moves
    int epSquare = eligibleEnPassantSquare;
    if (epSquare >= 0) {
        int epfile = epSquare % 8;
        if (color == WHITE) {
            if (epfile != 0 && peices[epSquare - 9] == color + PAWN) {
                moves.emplace_back(epSquare - 9, epSquare);
            }
            if (epfile != 7 && peices[epSquare - 7] == color + PAWN) {
                moves.emplace_back(epSquare - 7, epSquare);
            }
        }
        else {
            if (epfile != 0 && peices[epSquare + 7] == color + PAWN) {
                moves.emplace_back(epSquare + 7, epSquare);
            }
            if (epfile != 7 && peices[epSquare + 9] == color + PAWN) {
                moves.emplace_back(epSquare + 9, epSquare);
            }
        }
    }

    // Filter out illegal moves
    moves.erase(std::remove_if(moves.begin(), moves.end(), [&](StandardMove m) { return !isLegal(m); }), moves.end());
    return moves;
}

bool Game::isDrawByThreefoldRepitition() const noexcept
{
    if (halfmovesSincePawnMoveOrCapture < 8) {
        return false;
    }

    uint32 currentHash = static_cast<uint32>(zobrist);
    bool repititionFound = false;

    for (uint8 i = 0; i < halfmovesSincePawnMoveOrCapture; ++i) {
        if (positionHistory[i] == currentHash) {
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
    return halfmovesSincePawnMoveOrCapture >= 50;
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

bool Game::isLegal(const StandardMove& move)
{
    uint8 moving = peices[move.startSquare];
    uint8 c = moving >> 3;
    uint8 color = c << 3;
    uint8 e = !color;
    uint8 enemy = e << 3;
    uint8 captured;
    if (move.targetSquare == eligibleEnPassantSquare) {
        captured = enemy + PAWN;
    }
    else {
        captured = peices[move.targetSquare];
    }

    if (moving == color + KING && std::abs(move.startSquare - move.targetSquare) == 2) {
        return castlingMoveIsLegal(move);
    }

    // Update peices array first to check legality
    peices[move.startSquare] = 0;
    peices[move.targetSquare] = move.promotion ? color + move.promotion : moving;
    if (move.targetSquare == eligibleEnPassantSquare) {
        peices[move.targetSquare - 8 + 16 * c] = 0;
    }
    // Update king index
    if ((moving & 0b111) == KING) {
        kingIndex[c] = move.targetSquare;
    }

    // Check if move was illegal
    bool legal = !inCheck(c);

    // Undo peices array
    peices[move.startSquare] = moving;
    peices[move.targetSquare] = captured;
    if (move.targetSquare == eligibleEnPassantSquare) {
        peices[move.targetSquare] = 0;
        peices[move.targetSquare - 8 + 16 * c] = captured;
    }
    // Undo king index
    if ((moving & 0b111) == KING) {
        kingIndex[c] = move.startSquare;
    }

    return legal;
}

bool Game::castlingMoveIsLegal(const StandardMove& move) {
    uint8 c = totalHalfmoves % 2;
    uint8 color = c << 3;
    uint8 e = !color;
    uint8 enemy = e << 3;
    uint8 castlingRank = move.startSquare & 0b11111000;

    // Check if king is in check
    if (inCheck()) {
        return false;
    }

    // Check if anything is attacking squares on king's path
    uint8 s;
    uint8 end;
    if (move.targetSquare - castlingRank < 4) {
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

/*
The class in which these errors are occurring is a chess engine class. I did some testing and the error occurs even if return value of the function is not assigned to the class member. Somehow just the calling of the function leads to memory being corrupted. The function doesnt cause an error every time, only in certain chess positions
This is the general layout of the function which is causing the error:
"std::vector<StandardMove> Game::legalMoves()
{
    // INITIALIZE VARIABLES

    // INITIALIZE MOVE VECTOR

    std::vector<StandardMove> moves;

    // LOOP OVER EVERY SQUARE AND APPEND TO MOVE VECTOR

    // FILTER OUT ILLEGAL MOVES AND RETURN MOVE VECTOR
    moves.erase(std::remove_if(moves.begin(), moves.end(), [&](StandardMove m) { return !isLegal(m); }), moves.end());
    return moves;
}"
Where could the memory corruption be happening?
*/
