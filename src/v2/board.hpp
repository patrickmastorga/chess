#ifndef BOARD_H
#define BOARD_H

#include <cstdint>
#include <vector>
#include <memory>
#include <stack>
#include <unordered_set>
#include <algorithm>
#include <stdexcept>
#include <string>
#include <sstream>
#include <cctype>

#include "precomputed.hpp"
#include "../chess.hpp"

typedef std::int_fast16_t _int;
typedef std::uint_fast64_t uint64;

// class representing the current state of the chess game
class Board
{
public:
    // MOVE STRUCT
    // struct for containing info about a move
    struct Move
    {
        // Starting square of the move [0, 63] -> [a1, h8]
        _int startSquare;
        
        // Ending square of the move [0, 63] -> [a1, h8]
        _int targetSquare;

        // Peice and color of moving peice
        _int movingPeice;

        // Peice and color of captured peice
        _int capturedPeice;

        // 8 bit flags of move
        _int flags;

        // Heuristic geuss for the strength of the move (used for move ordering)
        _int moveStrengthGuess;

        
        // CONSTRUCTORS
        // Construct a new Move object from the given board and given flags (en_passant, castle, promotion, etc.)
        Move(const Board *board, _int start, _int target, _int givenFlags=NONE) : startSquare(start), targetSquare(target), flags(givenFlags), moveStrengthGuess(0)
        {
            movingPeice = board->peices[start];
            _int color = movingPeice & 0b11111000;

            if (flags & PROMOTION) {
                movingPeice = color + flags & PROMOTION;
            }

            capturedPeice = board->peices[target];

            if (flags & EN_PASSANT) {
                capturedPeice = !color + PAWN;
            }

            if (capturedPeice) {
                flags |= CAPTURE;
            }
        }

/*
        // Construct a new Move object from the given board with unknown flags (en_passant, castle, promotion, etc.)
        Move(const Board *board, _int start, _int target, bool legal, _int promotion=0) : startSquare(start), targetSquare(target), flags(0), moveStrengthGuess(0)
        {
            movingPeice = board->peices[start];
            _int color = movingPeice & 0b11111000;

            capturedPeice = board->peices[target];

            if (movingPeice % (1 << 3) == KING && std::abs(target - start) == 2) {
                flags |= CASTLE;
            }

            if (movingPeice % (1 << 3) == PAWN) {
                if (target >> 3 == 0 || target >> 3 == 7) {
                    flags |= promotion ? promotion : QUEEN;
                    movingPeice = flags & PROMOTION;
                
                } else if (((target - start) % 7 == 0 || (target - start) % 9 == 0) && !capturedPeice) {
                    flags |= EN_PASSANT;
                    capturedPeice = !color + PAWN;
                }
            }

            if (capturedPeice) {
                flags |= CAPTURE;
            }

            if (legal) {
                flags |= LEGAL;
            }
        }
*/

        // PUBLIC METHODS
        // Generates a heurusitic guess for the strength of a move based on information from the board
        void initializeMoveStrengthGuess(Board *Board)
        {
            // TODO
        }

        // Override equality operator with other move
        bool operator==(const Move& other) const
        {
            return this->startSquare == other.startSquare
                && this->targetSquare == other.targetSquare
                && this->flags & PROMOTION == other.flags & PROMOTION;
        }

        // Override equality operator with standard move
        bool operator==(const StandardMove& other) const
        {
            return this->startSquare == other.startSquare
                && this->targetSquare == other.targetSquare
                && this->flags & PROMOTION - KNIGHT + 1 == other.promotion;
        }

        // Override stream insertion operator to display info about the move
        std::ostream& operator<<(std::ostream& os) const
        {
            os << "(" << ChessHelpers::boardIndexToAlgebraicNotation(startSquare) << " -> " << ChessHelpers::boardIndexToAlgebraicNotation(targetSquare) << ")";
            return os;
        }


        // FLAGS
        static constexpr _int NONE       = 0b00000000;
        static constexpr _int PROMOTION  = 0b00000111;
        static constexpr _int LEGAL      = 0b00001000;
        static constexpr _int EN_PASSANT = 0b00010000;
        static constexpr _int CASTLE     = 0b00100000;
        static constexpr _int CAPTURE    = 0b01000000;
        static constexpr _int CHECK      = 0b10000000;
    };


    // CONSTRUCTORS
    // Construct a new Board object from fen string
    Board(const std::string &fenString)
    {
        std::istringstream fenStringStream(fenString);
        std::string peicePlacementData, activeColor, castlingAvailabilty, enPassantTarget, halfmoveClock, fullmoveNumber;

        peiceIndices[0] = std::unordered_set<_int>(23);
        peiceIndices[1] = std::unordered_set<_int>(23);
        
        zobrist = 0;
        
        // Get peice placement data from fen string
        if (!std::getline(fenStringStream, peicePlacementData, ' ')) {
            throw std::invalid_argument("Cannot get peice placement from FEN!");
        }

        // Update the peices[] according to fen rules
        _int peiceIndex = 56;
        for (char peiceInfo : peicePlacementData) {
            if (std::isalpha(peiceInfo)) {
                // char contains data about a peice
                //_int color = std::islower(c);
                _int c = peiceInfo > 96 && peiceInfo < 123;
                _int color = c << 3;

                peiceIndices[c].insert(peiceIndex);
                color <<= 3;
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

            } else if (std::isdigit(peiceInfo)) {
                // char contains data about gaps between peices
                _int gap = peiceInfo - '0';
                for (_int i = 0; i < gap; ++i) {
                    peices[peiceIndex++] = 0;
                }

            } else {
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

        } else if (activeColor == "b") {
            // Black is to move
            totalHalfmoves = 1;
            zobrist ^= ZOBRIST_TURN_KEY;
        
        } else {
            // active color can only be "wb"
            throw std::invalid_argument("Unrecognised charecter in FEN active color");
        }

        // Get castling availability data from fen string
        if (!std::getline(fenStringStream, castlingAvailabilty, ' ')) {
            throw std::invalid_argument("Cannot get castling availability from FEN!");
        }

        // Update castling availibility according to fen rules
        kingsideCastlingRightsLost[0] = 1;
        kingsideCastlingRightsLost[1] = 1;
        queensideCastlingRightsLost[0] = 1;
        queensideCastlingRightsLost[1] = 1;

        if (castlingAvailabilty != "-") {
            for (char castlingInfo : castlingAvailabilty) {
                //_int color = std::islower(c);
                _int c = castlingInfo > 96 && castlingInfo < 123;
                switch (castlingInfo) {
                    case 'K':
                    case 'k':
                        kingsideCastlingRightsLost[c] = 0;
                        zobrist ^= ZOBRIST_KINGSIDE_CASTLING_KEYS[c];
                        break;
                    case 'Q':
                    case 'q':
                        queensideCastlingRightsLost[c] = 0;
                        zobrist ^= ZOBRIST_QUEENSIDE_CASTLING_KEYS[c];
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

        if (enPassantTarget != "-") {
            try {
                eligibleEnPassantFile.push(ChessHelpers::algebraicNoatationToBoardIndex(enPassantTarget) % 8);
                zobrist ^= ZOBRIST_EN_PASSANT_KEYS[eligibleEnPassantFile.top()];
            } catch (const std::invalid_argument &e) {
                throw std::invalid_argument(std::string("Invalid FEN en passant target! ") + e.what());
            }
        
        } else {
            eligibleEnPassantFile.push(-1);
        }

        // Get half move clock data from fen string
        if (!std::getline(fenStringStream, halfmoveClock, ' ')) {
            throw std::invalid_argument("Cannot get halfmove clock from FEN!");
        }

        try {
            halfmovesSincePawnMoveOrCapture.push(static_cast<_int>(std::stoi(halfmoveClock)));
        } catch (const std::invalid_argument &e) {
            throw std::invalid_argument(std::string("Invalid FEN half move clock! ") + e.what());
        }

        // Get full move number data from fen string
        if (!std::getline(fenStringStream, fullmoveNumber, ' ')) {
            throw std::invalid_argument("Cannot getfullmove number from FEN!");
        }

        try {
            totalHalfmoves += static_cast<_int>(std::stoi(fullmoveNumber) * 2 - 2);
        } catch (const std::invalid_argument &e) {
            throw std::invalid_argument(std::string("Invalid FEN full move number! ") + e.what());
        }


        // initialize zobrist hash for all of the peices
        
        for (_int i = 0; i < 64; ++i) {
            if (peices[i]) {
                zobrist ^= ZOBRIST_PEICE_KEYS[peices[i] >> 3][peices[i] % (1 << 3) - 1][i];
            }
        }
    }
    
    // Construct a new Board object with the starting position
    Board() : Board("rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1") {}


    // PUBLIC METHODS
    // Generates pseudo-legal moves for the current position
    std::vector<Move> pseudoLegalMoves(_int flags=Move::NONE) const
    {
        // TODO Backwards check/pin generation for endgame
        // TODO Better pinned peice generation (detect if sliding along diagonal of pin)
        
        _int c = totalHalfmoves % 2;
        _int color = c << 3;
        _int e = !color;
        _int enemy = e << 3;

        // Used for optimizing the legality checking of moves
        std::unordered_set<_int> pinnedPeices(11);
        std::unordered_set<_int> checkingSquares(11);

        // Check if king in check and record pinned peices and checking squares
        _int king = kingIndex[c];
        _int checks = 0;

        // Pawn checks
        _int kingFile = king % 8;
        _int ahead = king + 8 - 16 * c;
        if (kingFile != 0 && peices[ahead - 1] == enemy + PAWN) {
            checkingSquares.insert(ahead - 1);
            ++checks;
        }
        if (kingFile != 7 && peices[ahead + 1] == enemy + PAWN) {
            checkingSquares.insert(ahead + 1);
            ++checks;
        }

        // Knight checks
        for (_int j = 1; j < KNIGHT_MOVES[king][0]; ++j) {
            if (peices[KNIGHT_MOVES[king][j]] == enemy + KNIGHT) {
                checkingSquares.insert(KNIGHT_MOVES[king][j]);
                ++checks;
            }
        }

        // pins and sliding peice checks
        _int potentialPin = 0;
        for (_int j = king - 8; j > DIRECTION_BOUNDS[king][B]; j -= 8) {
            if (!peices[j]) {
                continue;
            }
            if (!potentialPin && peices[j] >> 3 == c) {
                potentialPin = j;
                continue;
            }
            if (peices[j] == enemy + ROOK || peices[j] == enemy + QUEEN) {
                if (potentialPin) {
                    pinnedPeices.insert(potentialPin);
                    break;
                }
                if (checks++) { // dont need to record checking squares/pins for double checks
                    goto moveGeneration;
                }
                for (_int k = j; k < king; k += 8) {
                    checkingSquares.insert(k);
                }
            }
            break;
        }
        
        potentialPin = 0;
        for (_int j = king + 8; j < DIRECTION_BOUNDS[king][F]; j += 8) {
            if (!peices[j]) {
                continue;
            }
            if (!potentialPin && peices[j] >> 3 == c) {
                potentialPin = j;
                continue;
            }
            if (peices[j] == enemy + ROOK || peices[j] == enemy + QUEEN) {
                if (potentialPin) {
                    pinnedPeices.insert(potentialPin);
                    break;
                }
                if (checks++) { // dont need to record checking squares/pins for double checks
                    goto moveGeneration;
                }
                for (_int k = j; k > king; k -= 8) {
                    checkingSquares.insert(k);
                }
            }
            break;
        }

        potentialPin = 0;
        for (_int j = king - 1; j > DIRECTION_BOUNDS[king][L]; j -= 1) {
            if (!peices[j]) {
                continue;
            }
            if (!potentialPin && peices[j] >> 3 == c) {
                potentialPin = j;
                continue;
            }
            if (peices[j] == enemy + ROOK || peices[j] == enemy + QUEEN) {
                if (potentialPin) {
                    pinnedPeices.insert(potentialPin);
                    break;
                }
                if (checks++) { // dont need to record checking squares/pins for double checks
                    goto moveGeneration;
                }
                for (_int k = j; k < king; k += 1) {
                    checkingSquares.insert(k);
                }
            }
            break;
        }

        potentialPin = 0;
        for (_int j = king + 1; j < DIRECTION_BOUNDS[king][R]; j += 1) {
            if (!peices[j]) {
                continue;
            }
            if (!potentialPin && peices[j] >> 3 == c) {
                potentialPin = j;
                continue;
            }
            if (peices[j] == enemy + ROOK || peices[j] == enemy + QUEEN) {
                if (potentialPin) {
                    pinnedPeices.insert(potentialPin);
                    break;
                }
                if (checks++) { // dont need to record checking squares/pins for double checks
                    goto moveGeneration;
                }
                for (_int k = j; k > king; k -= 1) {
                    checkingSquares.insert(k);
                }
            }
            break;
        }

        potentialPin = 0;
        for (_int j = king - 9; j > DIRECTION_BOUNDS[king][BL]; j -= 9) {
            if (!peices[j]) {
                continue;
            }
            if (!potentialPin && peices[j] >> 3 == c) {
                potentialPin = j;
                continue;
            }
            if (peices[j] == enemy + BISHOP || peices[j] == enemy + QUEEN) {
                if (potentialPin) {
                    pinnedPeices.insert(potentialPin);
                    break;
                }
                if (checks++) { // dont need to record checking squares/pins for double checks
                    goto moveGeneration;
                }
                for (_int k = j; k < king; k += 9) {
                    checkingSquares.insert(k);
                }
            }
            break;
        }

        potentialPin = 0;
        for (_int j = king + 9; j < DIRECTION_BOUNDS[king][FR]; j += 9) {
            if (!peices[j]) {
                continue;
            }
            if (!potentialPin && peices[j] >> 3 == c) {
                potentialPin = j;
                continue;
            }
            if (peices[j] == enemy + BISHOP || peices[j] == enemy + QUEEN) {
                if (potentialPin) {
                    pinnedPeices.insert(potentialPin);
                    break;
                }
                if (checks++) { // dont need to record checking squares/pins for double checks
                    goto moveGeneration;
                }
                for (_int k = j; k > king; k -= 9) {
                    checkingSquares.insert(k);
                }
            }
            break;
        }

        potentialPin = 0;
        for (_int j = king - 7; j > DIRECTION_BOUNDS[king][BR]; j -= 7) {
            if (!peices[j]) {
                continue;
            }
            if (!potentialPin && peices[j] >> 3 == c) {
                potentialPin = j;
                continue;
            }
            if (peices[j] == enemy + BISHOP || peices[j] == enemy + QUEEN) {
                if (potentialPin) {
                    pinnedPeices.insert(potentialPin);
                    break;
                }
                if (checks++) { // dont need to record checking squares/pins for double checks
                    goto moveGeneration;
                }
                for (_int k = j; k < king; k += 7) {
                    checkingSquares.insert(k);
                }
            }
            break;
        }

        potentialPin = 0;
        for (_int j = king + 7; j < DIRECTION_BOUNDS[king][FL]; j += 7) {
            if (!peices[j]) {
                continue;
            }
            if (!potentialPin && peices[j] >> 3 == c) {
                potentialPin = j;
                continue;
            }
            if (peices[j] == enemy + BISHOP || peices[j] == enemy + QUEEN) {
                if (potentialPin) {
                    pinnedPeices.insert(potentialPin);
                    break;
                }
                if (checks++) { // dont need to record checking squares/pins for double checks
                    goto moveGeneration;
                }
                for (_int k = j; k > king; k -= 7) {
                    checkingSquares.insert(k);
                }
            }
            break;
        }

        moveGeneration:
        // Generate moves based on data gathered
        std::vector<Move> moves;
        
        // Double check; Only king moves are legal
        if (checks > 1) {
            // Generate king moves
            for (_int j = 1; j < KING_MOVES[king][0]; ++j) {
                if (!peices[KING_MOVES[king][j]] || peices[KING_MOVES[king][j]] >> 3 == e) {
                    moves.emplace_back(this, king, KING_MOVES[king][j], Move::NONE);
                }
            }
            return moves;
        }

        // En passant moves
        _int epfile = eligibleEnPassantFile.top();
        if (epfile > 0) {
            if (color == WHITE) {
                if (epfile != 0 && peices[32 + epfile - 1] == color + PAWN && (!checks || checkingSquares.count(40 + epfile))) {
                    moves.emplace_back(this, 32 + epfile - 1, 40 + epfile, Move::EN_PASSANT);
                }
                if (epfile != 7 && peices[32 + epfile + 1] == color + PAWN && (!checks || checkingSquares.count(40 + epfile))) {
                    moves.emplace_back(this, 32 + epfile + 1, 40 + epfile, Move::EN_PASSANT);
                }
            } else {
                if (epfile != 0 && peices[24 + epfile - 1] == color + PAWN && (!checks || checkingSquares.count(16 + epfile))) {
                    moves.emplace_back(this, 24 + epfile - 1, 16 + epfile, Move::EN_PASSANT);
                }
                if (epfile != 7 && peices[24 + epfile + 1] == color + PAWN && (!checks || checkingSquares.count(16 + epfile))) {
                    moves.emplace_back(this, 24 + epfile + 1, 16 + epfile, Move::EN_PASSANT);
                }
            }
        }

        // Special move generation for when few number of checking squares
        if (checks && checkingSquares.size() < peiceIndices[c].size() * 0.4) {
            // Generate king moves
            for (_int j = 1; j < KING_MOVES[king][0]; ++j) {
                if (!peices[KING_MOVES[king][j]] || peices[KING_MOVES[king][j]] >> 3 == e) {
                    moves.emplace_back(this, king, KING_MOVES[king][j], Move::NONE);
                }
            }
           
            // Backwards search from checking squares to see if peices can move to them
            for (_int t : checkingSquares) {

                // Pawn can block/take
                if ((color == WHITE && t >> 3 >= 2) || (color == BLACK && t >> 3 <= 5)) {
                    _int file = t % 8;
                    _int ahead = t - 8 + 16 * c;
                    bool promotion = ahead >> 3 == 0 || ahead >> 3 == 7;
                    // Pawn capture
                    if (peices[t] >> 3 == e) { 
                        if (file != 0 && peices[ahead - 1] == color + PAWN && !pinnedPeices.count(ahead - 1)) {
                            if (promotion) {
                                moves.emplace_back(this, ahead - 1, t, KNIGHT | Move::LEGAL);
                                moves.emplace_back(this, ahead - 1, t, BISHOP | Move::LEGAL);
                                moves.emplace_back(this, ahead - 1, t, ROOK | Move::LEGAL);
                                moves.emplace_back(this, ahead - 1, t, QUEEN | Move::LEGAL);
                            } else {
                                moves.emplace_back(this, ahead - 1, t, Move::LEGAL);
                            }
                        }
                        if (file != 7 && peices[ahead + 1] == color + PAWN && !pinnedPeices.count(ahead + 1)) {
                            if (promotion) {
                                moves.emplace_back(this, ahead + 1, t, KNIGHT | Move::LEGAL);
                                moves.emplace_back(this, ahead + 1, t, BISHOP | Move::LEGAL);
                                moves.emplace_back(this, ahead + 1, t, ROOK | Move::LEGAL);
                                moves.emplace_back(this, ahead + 1, t, QUEEN | Move::LEGAL);
                            } else {
                                moves.emplace_back(this, ahead + 1, t, Move::LEGAL);
                            }
                        }
                    // Pawn move
                    } else if (!peices[t]) {
                        _int doubleAhead = ahead - 8 + 16 * c;
                        if (peices[ahead] == color + PAWN && !pinnedPeices.count(ahead)) {
                            if (promotion) {
                                moves.emplace_back(this, ahead, t, KNIGHT | Move::LEGAL);
                                moves.emplace_back(this, ahead, t, BISHOP | Move::LEGAL);
                                moves.emplace_back(this, ahead, t, ROOK | Move::LEGAL);
                                moves.emplace_back(this, ahead, t, QUEEN | Move::LEGAL);
                            } else {
                                moves.emplace_back(this, ahead, t, Move::LEGAL);
                            }
                    
                        } else if ((doubleAhead >> 3 == 1 || doubleAhead >> 3 == 6) && !peices[ahead] && peices[doubleAhead] == color + PAWN && !pinnedPeices.count(doubleAhead)) {
                            moves.emplace_back(this, doubleAhead, t, Move::LEGAL);
                        }
                    }                
                }

                // Knight can block/take
                for (_int j = 1; j < KNIGHT_MOVES[t][0]; ++j) {
                    if (peices[KNIGHT_MOVES[t][j]] == color + KNIGHT && !pinnedPeices.count(KNIGHT_MOVES[t][j])) {
                        moves.emplace_back(this, KNIGHT_MOVES[t][j], t, Move::LEGAL);
                    }
                }

                // Sliding peices can block/take
                for (_int s = t - 8; s > DIRECTION_BOUNDS[king][B]; s -= 8) {
                    if (peices[s]) {
                        if ((peices[s] == color + ROOK || peices[s] == color + QUEEN) && !pinnedPeices.count(s)) {
                            moves.emplace_back(this, s, t, Move::LEGAL);
                        }
                        break;
                    }
                }

                for (_int s = t + 8; s < DIRECTION_BOUNDS[king][F]; s += 8) {
                    if (peices[s]) {
                        if ((peices[s] == color + ROOK || peices[s] == color + QUEEN) && !pinnedPeices.count(s)) {
                            moves.emplace_back(this, s, t, Move::LEGAL);
                        }
                        break;
                    }
                }

                for (_int s = t - 1; s > DIRECTION_BOUNDS[king][L]; s -= 1) {
                    if (peices[s]) {
                        if ((peices[s] == color + ROOK || peices[s] == color + QUEEN) && !pinnedPeices.count(s)) {
                            moves.emplace_back(this, s, t, Move::LEGAL);
                        }
                        break;
                    }
                }

                for (_int s = t + 1; s < DIRECTION_BOUNDS[king][R]; s += 1) {
                    if (peices[s]) {
                        if ((peices[s] == color + ROOK || peices[s] == color + QUEEN) && !pinnedPeices.count(s)) {
                            moves.emplace_back(this, s, t, Move::LEGAL);
                        }
                        break;
                    }
                }

                for (_int s = t - 9; s > DIRECTION_BOUNDS[king][BL]; s -= 9) {
                    if (peices[s]) {
                        if ((peices[s] == color + BISHOP || peices[s] == color + QUEEN) && !pinnedPeices.count(s)) {
                            moves.emplace_back(this, s, t, Move::LEGAL);
                        }
                        break;
                    }
                }

                for (_int s = t + 9; s < DIRECTION_BOUNDS[king][FR]; s += 9) {
                    if (peices[s]) {
                        if ((peices[s] == color + BISHOP || peices[s] == color + QUEEN) && !pinnedPeices.count(s)) {
                            moves.emplace_back(this, s, t, Move::LEGAL);
                        }
                        break;
                    }
                }

                for (_int s = t - 7; s > DIRECTION_BOUNDS[king][BR]; s -= 7) {
                    if (peices[s]) {
                        if ((peices[s] == color + BISHOP || peices[s] == color + QUEEN) && !pinnedPeices.count(s)) {
                            moves.emplace_back(this, s, t, Move::LEGAL);
                        }
                        break;
                    }
                }

                for (_int s = t + 7; s < DIRECTION_BOUNDS[king][FL]; s += 7) {
                    if (peices[s]) {
                        if ((peices[s] == color + BISHOP || peices[s] == color + QUEEN) && !pinnedPeices.count(s)) {
                            moves.emplace_back(this, s, t, Move::LEGAL);
                        }
                        break;
                    }
                }
            }

            return moves;
        }

        // Castling
        if (!kingsideCastlingRightsLost[c] && !checks) {
            _int castlingRank = 56 * c;
            bool roomToCastle = true;
            for (_int j = castlingRank + 3; j > castlingRank; --j) {
                if (peices[j]) {
                    roomToCastle = false;
                    break;
                }
            }
            if (roomToCastle) {
                moves.emplace_back(this, castlingRank + 4, castlingRank + 6, Move::CASTLE);
            }
        }
        if (!queensideCastlingRightsLost[c] && !checks) {
            _int castlingRank = 56 * c;
            bool roomToCastle = true;
            for (_int j = castlingRank + 5; j < castlingRank + 7; ++j) {
                if (peices[j]) {
                    roomToCastle = false;
                    break;
                }
            }
            if (roomToCastle) {
                moves.emplace_back(this, castlingRank + 4, castlingRank + 2, Move::CASTLE);
            }
        }

        // General case
        for (_int s : peiceIndices[c]) {
            _int legalFlag = pinnedPeices.count(s) ? Move::LEGAL : Move::NONE;

            switch (peices[s] % (1 << 3)) {
                case PAWN: {
                    _int file = s % 8;
                    _int ahead = s + 8 - 16 * c;
                    bool promotion = s >> 3 == 0 || s >> 3 == 7;

                    // Pawn foward moves
                    if (!peices[ahead]) {
                        if (promotion) {
                            moves.emplace_back(this, s, ahead, legalFlag | KNIGHT);
                            moves.emplace_back(this, s, ahead, legalFlag | BISHOP);
                            moves.emplace_back(this, s, ahead, legalFlag | ROOK);
                            moves.emplace_back(this, s, ahead, legalFlag | KING);
                        } else {
                            moves.emplace_back(this, s, ahead, legalFlag);
                        }

                        if ((s >> 3 == 1 || s >> 3 == 6) && !peices[ahead + 8 - 16 * c]) {
                            moves.emplace_back(this, s, ahead + 8 - 16 * c, legalFlag);
                        }
                    }

                    // Pawn captures
                    if (file != 0 && peices[ahead - 1] && peices[ahead - 1] >> 3 == e) {
                        if (promotion) {
                            moves.emplace_back(this, s, ahead - 1, legalFlag | KNIGHT);
                            moves.emplace_back(this, s, ahead - 1, legalFlag | BISHOP);
                            moves.emplace_back(this, s, ahead - 1, legalFlag | ROOK);
                            moves.emplace_back(this, s, ahead - 1, legalFlag | QUEEN);
                        } else {
                            moves.emplace_back(this, ahead - 1, legalFlag);
                        }
                    }
                    if (file != 7 && peices[ahead + 1] && peices[ahead + 1] >> 3 == e) {
                        if (promotion) {
                            moves.emplace_back(this, s, ahead + 1, legalFlag | KNIGHT);
                            moves.emplace_back(this, s, ahead + 1, legalFlag | BISHOP);
                            moves.emplace_back(this, s, ahead + 1, legalFlag | ROOK);
                            moves.emplace_back(this, s, ahead + 1, legalFlag | QUEEN);
                        } else {
                            moves.emplace_back(this, s, ahead + 1, legalFlag);
                        }
                    }
                    break;
                }
                case KNIGHT: {
                    _int t;
                    for (_int j = 1; j < KNIGHT_MOVES[s][0]; ++j) {
                        t = KNIGHT_MOVES[s][j];
                        if ((!peices[t] || peices[t] >> 3 == e) && (!checks || checkingSquares.count(t))) {
                            moves.emplace_back(this, s, t, legalFlag);
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

                    for (_int t = s - 8; t > DIRECTION_BOUNDS[s][B]; t -= 8) {
                        if ((!peices[t] || peices[t] >> 3 == e) && (!checks || checkingSquares.count(t))) {
                            moves.emplace_back(this, s, t, legalFlag);
                        }
                        if (peices[t]) {
                            break;
                        }
                    }

                    for (_int t = s + 8; t < DIRECTION_BOUNDS[s][F]; t += 8) {
                        if ((!peices[t] || peices[t] >> 3 == e) && (!checks || checkingSquares.count(t))) {
                            moves.emplace_back(this, s, t, legalFlag);
                        }
                        if (peices[t]) {
                            break;
                        }
                    }

                    for (_int t = s - 1; t > DIRECTION_BOUNDS[s][L]; t -= 1) {
                        if ((!peices[t] || peices[t] >> 3 == e) && (!checks || checkingSquares.count(t))) {
                            moves.emplace_back(this, s, t, legalFlag);
                        }
                        if (peices[t]) {
                            break;
                        }
                    }

                    for (_int t = s + 1; t < DIRECTION_BOUNDS[s][R]; t += 1) {
                        if ((!peices[t] || peices[t] >> 3 == e) && (!checks || checkingSquares.count(t))) {
                            moves.emplace_back(this, s, t, legalFlag);
                        }
                        if (peices[t]) {
                            break;
                        }
                    }

                    if (peices[s] % (1 << 3) == ROOK) {
                        break;
                    }
                    bishopMoves:

                    for (_int t = s - 9; t > DIRECTION_BOUNDS[s][BL]; t -= 9) {
                        if ((!peices[t] || peices[t] >> 3 == e) && (!checks || checkingSquares.count(t))) {
                            moves.emplace_back(this, s, t, legalFlag);
                        }
                        if (peices[t]) {
                            break;
                        }
                    }

                    for (_int t = s + 9; t < DIRECTION_BOUNDS[s][FR]; t += 9) {
                        if ((!peices[t] || peices[t] >> 3 == e) && (!checks || checkingSquares.count(t))) {
                            moves.emplace_back(this, s, t, legalFlag);
                        }
                        if (peices[t]) {
                            break;
                        }
                    }

                    for (_int t = s - 7; t > DIRECTION_BOUNDS[s][BR]; t -= 7) {
                        if ((!peices[t] || peices[t] >> 3 == e) && (!checks || checkingSquares.count(t))) {
                            moves.emplace_back(this, s, t, legalFlag);
                        }
                        if (peices[t]) {
                            break;
                        }
                    }

                    for (_int t = s + 7; t < DIRECTION_BOUNDS[s][FL]; t += 7) {
                        if ((!peices[t] || peices[t] >> 3 == e) && (!checks || checkingSquares.count(t))) {
                            moves.emplace_back(this, s, t, legalFlag);
                        }
                        if (peices[t]) {
                            break;
                        }
                    }
                    
                    break;
                }
                case KING: {
                    _int t;
                    for (_int j = 1; j < KING_MOVES[s][0]; ++j) {
                        t = KING_MOVES[s][j];
                        if (!peices[t] || peices[t] >> 3 == e) {
                            moves.emplace_back(this, s, t, Move::NONE);
                        }
                    }
                }
            }
            
        }

        return moves;
    }

    // Generates legal moves for the current position
    // Not as fast as pseudoLegalMoves() for searching
    std::vector<Move> legalMoves()
    {
        std::vector<Move> moves = pseudoLegalMoves();

        moves.erase(std::remove_if(moves.begin(), moves.end(), [&](Move m) { return !isLegal(m); }), moves.end());
        return moves;
    }

    // update the board based on the inputted move (must be pseudo legal)
    // returns true if move was legal and process completed
    bool makeMove(Move &move)
    {
        _int c = move.movingPeice >> 3;
        _int color = c << 3;
        _int e = !color;
        _int enemy = e << 3;
        _int moving = move.movingPeice % (1 << 3);

        // Seperately check legality of castling moves
        if (move.flags & Move::CASTLE && !(move.flags & Move::LEGAL)) {
            if (!castlingMoveIsLegal(move)) {
                return false;
            }
            move.flags |= Move::LEGAL;
        }
        
        // Update peices array first to check legality
        peices[move.startSquare] = 0;
        peices[move.targetSquare] = move.movingPeice;
        if (move.flags & Move::EN_PASSANT) {
            peices[move.targetSquare - 8 + 16 * c] = 0;
        }

        // Update king index
        if (moving == KING) {
            kingIndex[c] = move.targetSquare;
        }

        // Check if move was illegal
        if (!(move.flags & Move::LEGAL) && inCheck(c)) {
            // Undo change
            peices[move.startSquare] = (move.flags & Move::PROMOTION) ? color + PAWN : move.movingPeice;;
            peices[move.targetSquare] = (move.flags & Move::EN_PASSANT) ? 0 : move.capturedPeice;
            if (move.flags & Move::EN_PASSANT) {
                peices[move.targetSquare - 8 + 16 * c] = move.capturedPeice;
            }
            // Undo king index
            if (moving == KING) {
                kingIndex[c] = move.startSquare;
            }
            return false;
        }

        move.flags |= Move::LEGAL;

        // Update peice indices set
        peiceIndices[c].erase(move.startSquare);
        peiceIndices[c].insert(move.targetSquare);

        // En passant file
        if (eligibleEnPassantFile.top() >= 0) {
            zobrist ^= ZOBRIST_EN_PASSANT_KEYS[eligibleEnPassantFile.top()];
        }
        if (move.movingPeice == PAWN && std::abs(move.targetSquare - move.startSquare) == 16) {
            zobrist ^= ZOBRIST_EN_PASSANT_KEYS[move.startSquare % 8];
            eligibleEnPassantFile.push(move.startSquare % 8);
        
        } else {
            eligibleEnPassantFile.push(-1);
        }

        // Update zobrist hash for turn change
        zobrist ^= ZOBRIST_TURN_KEY;
        
        // Update zobrist hash for moving peice
        if (move.flags & Move::PROMOTION) {
            zobrist ^= ZOBRIST_PEICE_KEYS[c][PAWN - 1][move.startSquare];
            zobrist ^= ZOBRIST_PEICE_KEYS[c][moving - 1][move.targetSquare];
            moving = PAWN;
        
        } else { 
            zobrist ^= ZOBRIST_PEICE_KEYS[c][moving - 1][move.startSquare];
            zobrist ^= ZOBRIST_PEICE_KEYS[c][moving - 1][move.targetSquare];
        }

        // Update zobrist hash and peice indices set for capture
        if (move.flags & Move::EN_PASSANT) {
            _int captureSquare = move.targetSquare - 8 + 16 * c;
            zobrist ^= ZOBRIST_PEICE_KEYS[e][move.capturedPeice % (1 << 3) - 1][captureSquare];
            peiceIndices[e].erase(captureSquare);

        } else if (move.capturedPeice) {
            zobrist ^= ZOBRIST_PEICE_KEYS[e][move.capturedPeice % (1 << 3) - 1][move.targetSquare];
            peiceIndices[e].erase(move.targetSquare);
        }

        // increment counters
        ++totalHalfmoves;
        if (move.capturedPeice || moving == PAWN) {
            halfmovesSincePawnMoveOrCapture.push(0);
        } else {
            _int prev = halfmovesSincePawnMoveOrCapture.top();
            halfmovesSincePawnMoveOrCapture.pop();
            halfmovesSincePawnMoveOrCapture.push(prev + 1);
        }

        // Update rooks for castling
        if (move.flags & Move::CASTLE) {
            _int castlingRank = move.targetSquare & 0b11111000;

            if (move.targetSquare % 8 < 4) {
                // Queenside castling
                peices[castlingRank + 3] = peices[castlingRank];
                peices[castlingRank] = 0;
                zobrist ^= ZOBRIST_PEICE_KEYS[c][ROOK - 1][castlingRank + 3];
                zobrist ^= ZOBRIST_PEICE_KEYS[c][ROOK - 1][castlingRank];
                peiceIndices[c].erase(castlingRank);
                peiceIndices[c].insert(castlingRank + 3);
                
            } else {
                // Kingside castling
                peices[castlingRank + 5] = peices[castlingRank + 7];
                peices[castlingRank + 7] = 0;
                zobrist ^= ZOBRIST_PEICE_KEYS[c][ROOK - 1][castlingRank + 5];
                zobrist ^= ZOBRIST_PEICE_KEYS[c][ROOK - 1][castlingRank + 7];
                peiceIndices[c].erase(castlingRank + 7);
                peiceIndices[c].insert(castlingRank + 5);
            }
        }

        // update castling rights
        if (!kingsideCastlingRightsLost[c]) {
            if (moving == KING || (moving == ROOK && move.startSquare % 8 == 7)) {
                kingsideCastlingRightsLost[c] = totalHalfmoves;
                zobrist ^= ZOBRIST_KINGSIDE_CASTLING_KEYS[c];
            }
        }
        if (!queensideCastlingRightsLost[c]) {
            if (moving == KING || (moving == ROOK && move.startSquare % 8 == 0)) {
                kingsideCastlingRightsLost[c] = totalHalfmoves;
                zobrist ^= ZOBRIST_QUEENSIDE_CASTLING_KEYS[c];
            }
        }

        return true;
    }

    // update the board based on the inputted move (must have just been move previously played)
    void unmakeMove(const Move &move)
    {
        _int c = move.movingPeice >> 3;
        _int color = c << 3;
        _int e = !color;
        _int enemy = e << 3;
        _int moving = move.movingPeice % (1 << 3);

        // Undo en passant file
        if (eligibleEnPassantFile.top() >= 0) {
            zobrist ^= ZOBRIST_EN_PASSANT_KEYS[eligibleEnPassantFile.top()];
        }
        eligibleEnPassantFile.pop();
        if (eligibleEnPassantFile.size() > 0 && eligibleEnPassantFile.top() >= 0) {
            zobrist ^= ZOBRIST_EN_PASSANT_KEYS[eligibleEnPassantFile.top()];
        }

        // Undo zobrist hash for turn change
        zobrist ^= ZOBRIST_TURN_KEY;
        
        // Undo zobrist hash for moving peice
        if (move.flags & Move::PROMOTION) {
            zobrist ^= ZOBRIST_PEICE_KEYS[c][PAWN - 1][move.startSquare];
            zobrist ^= ZOBRIST_PEICE_KEYS[c][moving - 1][move.targetSquare];
            moving = PAWN;
        
        } else { 
            zobrist ^= ZOBRIST_PEICE_KEYS[c][moving - 1][move.startSquare];
            zobrist ^= ZOBRIST_PEICE_KEYS[c][moving - 1][move.targetSquare];
        }

        // undo zobrist hash and peice indices set for capture
        if (move.flags & Move::EN_PASSANT) {
            _int captureSquare = move.targetSquare - 8 + 16 * c;
            peices[captureSquare] = move.capturedPeice;
            zobrist ^= ZOBRIST_PEICE_KEYS[e][move.capturedPeice % (1 << 3) - 1][captureSquare];
            peiceIndices[e].insert(captureSquare);

        } else if (move.capturedPeice) {
            zobrist ^= ZOBRIST_PEICE_KEYS[e][move.capturedPeice % (1 << 3) - 1][move.targetSquare];
            peiceIndices[e].insert(move.targetSquare);
        }

        // decrement counters
        --totalHalfmoves;
        if (move.capturedPeice || moving == PAWN) {
            halfmovesSincePawnMoveOrCapture.pop();
        } else {
            _int prev = halfmovesSincePawnMoveOrCapture.top();
            halfmovesSincePawnMoveOrCapture.pop();
            halfmovesSincePawnMoveOrCapture.push(prev - 1);
        }

        // undo peices array
        peices[move.startSquare] = color + moving;
        peices[move.targetSquare] = (move.flags & Move::EN_PASSANT) ? 0 : move.capturedPeice;
        
        // undo peice indices set
        peiceIndices[c].erase(move.targetSquare);
        peiceIndices[c].insert(move.startSquare);

        // undo rooks for castling
        if (move.flags & Move::CASTLE) {
            _int castlingRank = move.targetSquare & 0b11111000;

            if (move.targetSquare % 8 < 4) {
                // Queenside castling
                peices[castlingRank] = peices[castlingRank + 3];
                peices[castlingRank + 3] = 0;
                zobrist ^= ZOBRIST_PEICE_KEYS[c][ROOK - 1][castlingRank + 3];
                zobrist ^= ZOBRIST_PEICE_KEYS[c][ROOK - 1][castlingRank];
                peiceIndices[c].erase(castlingRank + 3);
                peiceIndices[c].insert(castlingRank);
                
            } else {
                // Kingside castling
                peices[castlingRank + 7] = peices[castlingRank + 5];
                peices[castlingRank + 5] = 0;
                zobrist ^= ZOBRIST_PEICE_KEYS[c][ROOK - 1][castlingRank + 5];
                zobrist ^= ZOBRIST_PEICE_KEYS[c][ROOK - 1][castlingRank + 7];
                peiceIndices[c].erase(castlingRank + 5);
                peiceIndices[c].insert(castlingRank + 7);
            }
        }

        // undo castling rights
        if (kingsideCastlingRightsLost[c] == totalHalfmoves) {
            kingsideCastlingRightsLost[c] = 0;
        }
        if (!queensideCastlingRightsLost[c] == totalHalfmoves) {
            queensideCastlingRightsLost[c] = 0;
        }
        
        // Update king index
        if (moving == KING) {
            kingIndex[c] = move.startSquare;
        }
    }
  
    // return true if the player about to move is in check
    bool inCheck() const
    {
        return inCheck(totalHalfmoves % 2);
    }
    
    // @return true if the last move has put the game into a forced draw (threefold repitition / 50 move rule)
    bool isDraw() const
    {
        // TODO
        return false;
    }

    // returns total half moves since game start (half move is one player taking a turn)
    int getTotalHalfmoves() const
    {
        return totalHalfmoves;
    }

private:
    // DEFINITIONS
    static constexpr _int WHITE = 0b0000;
    static constexpr _int BLACK = 0b1000;
    static constexpr _int PAWN =   0b001;
    static constexpr _int KNIGHT = 0b010;
    static constexpr _int BISHOP = 0b011;
    static constexpr _int ROOK =   0b100;
    static constexpr _int QUEEN =  0b101;
    static constexpr _int KING =   0b110;

    
    // PRIVATE MEMBERS
    // color and peice type at every square (index [0, 63] -> [a1, h8])
    _int peices[64];

    // contains the halfmove number when the kingside castling rights were lost for white or black (index 0 and 1)
    _int kingsideCastlingRightsLost[2];

    // contains the halfmove number when the queenside castling rights were lost for white or black (index 0 and 1)
    _int queensideCastlingRightsLost[2];

    // file where a pawn has just moved two squares over
    std::stack<_int> eligibleEnPassantFile;

    // number of half moves since pawn move or capture (half move is one player taking a turn) (used for 50 move rule)
    std::stack<_int> halfmovesSincePawnMoveOrCapture;

    // total half moves since game start (half move is one player taking a turn)
    _int totalHalfmoves;

    // index of the white and black king (index 0 and 1)
    _int kingIndex[2];

    // indices of all white and black peices (index 0 and 1)
    std::unordered_set<_int> peiceIndices[2];

    // zobrist hash of the current position
    uint64 zobrist;


    // PRIVATE METHODS  
    // return true if the king belonging to the inputted color is currently being attacked
    bool inCheck(_int c) const
    {
        // TODO Backwards check searching during endgame

        _int color = c << 3;
        _int e = !c;
        _int enemy = e << 3;
        _int king = kingIndex[c];

        // Pawn checks
        _int kingFile = king % 8;
        _int ahead = king + 8 - 16 * c;
        if (kingFile != 0 && peices[ahead - 1] == enemy + PAWN) {
            return true;
        }
        if (kingFile != 7 && peices[ahead + 1] == enemy + PAWN) {
            return true;
        }

        // Knight checks
        for (_int j = 1; j < KNIGHT_MOVES[king][0]; ++j) {
            if (peices[KNIGHT_MOVES[king][j]] == enemy + KNIGHT) {
                return true;
            }
        }

        // sliding peice checks
        for (_int j = king - 8; j > DIRECTION_BOUNDS[king][B]; j -= 8) {
            if (peices[j]) {
                if (peices[j] == enemy + ROOK || peices[j] == enemy + QUEEN) {
                    return true;
                }
                break;
            }
        }
        
        for (_int j = king + 8; j < DIRECTION_BOUNDS[king][F]; j += 8) {
            if (peices[j]) {
                if (peices[j] == enemy + ROOK || peices[j] == enemy + QUEEN) {
                    return true;
                }
                break;
            }
        }

        for (_int j = king - 1; j > DIRECTION_BOUNDS[king][L]; j -= 1) {
            if (peices[j]) {
                if (peices[j] == enemy + ROOK || peices[j] == enemy + QUEEN) {
                    return true;
                }
                break;
            }
        }

        for (_int j = king + 1; j < DIRECTION_BOUNDS[king][R]; j += 1) {
            if (peices[j]) {
                if (peices[j] == enemy + ROOK || peices[j] == enemy + QUEEN) {
                    return true;
                }
                break;
            }
        }

        for (_int j = king - 9; j > DIRECTION_BOUNDS[king][BL]; j -= 9) {
            if (peices[j]) {
                if (peices[j] == enemy + BISHOP || peices[j] == enemy + QUEEN) {
                    return true;
                }
                break;
            }
        }

        for (_int j = king + 9; j < DIRECTION_BOUNDS[king][FR]; j += 9) {
            if (peices[j]) {
                if (peices[j] == enemy + BISHOP || peices[j] == enemy + QUEEN) {
                    return true;
                }
                break;
            }
        }

        for (_int j = king - 7; j > DIRECTION_BOUNDS[king][BR]; j -= 7) {
            if (peices[j]) {
                if (peices[j] == enemy + BISHOP || peices[j] == enemy + QUEEN) {
                    return true;
                }
                break;
            }
        }

        for (_int j = king + 7; j < DIRECTION_BOUNDS[king][FL]; j += 7) {
            if (peices[j]) {
                if (peices[j] == enemy + BISHOP || peices[j] == enemy + QUEEN) {
                    return true;
                }
                break;
            }
        }

        // King checks (seems wierd, but needed for detecting illegal moves)
        for (_int j = 1; j < KING_MOVES[king][0]; ++j) {
            if (peices[KING_MOVES[king][j]] == enemy + KING) {
                return true;
            }
        }

        return false;
    }

    // return true if inputted pseudo legal move is legal in the current position
    bool isLegal(Move &move)
    {
        if (move.flags & Move::LEGAL) {
            return true;
        }

        if (move.flags & Move::CASTLE) {
            // Check if king is in check
            if (inCheck()) {
                return false;
            }

            return castlingMoveIsLegal(move);
        }

        _int color = totalHalfmoves % 2;
        makeMove(move);
        bool legal = !inCheck(color);
        unmakeMove(move);
        if (legal) {
            move.flags |= Move::LEGAL;
        }
        return legal;
    }

    // @param move pseudo legal castling move (castling rights are not lost and king is not in check)
    // @return true if the castling move is legal in the current position
    bool castlingMoveIsLegal(Move &move) {
        if (move.flags & Move::LEGAL) {
            return true;
        }

        _int c = totalHalfmoves % 2;
        _int color = c << 3;
        _int e = !color;
        _int enemy = e << 3;
        _int castlingRank = move.startSquare & 0b11111000;

        // Check if anything is attacking squares on king's path
        _int s;
        _int end;
        if (move.targetSquare - castlingRank < 4) {
            s = castlingRank + 2;
            end = castlingRank + 3;
        } else {
            s = castlingRank + 5;
            end = castlingRank + 6;
        }
        for (; s <= end; ++s) {
            // Pawn attacks
            _int file = s % 8;
            _int ahead = s + 8 - 16 * c;
            if (file != 0 && peices[ahead - 1] == enemy + PAWN) {
                return false;
            }
            if (file != 7 && peices[ahead + 1] == enemy + PAWN) {
                return false;
            }

            // Knight attacks
            for (_int j = 1; j < KNIGHT_MOVES[s][0]; ++j) {
                if (peices[KNIGHT_MOVES[s][j]] == enemy + KNIGHT) {
                    return false;
                }
            }

            // sliding peice attacks
            if (color == BLACK) {
                for (_int j = s - 8; j > DIRECTION_BOUNDS[s][B]; j -= 8) {
                    if (peices[j]) {
                        if (peices[j] == enemy + ROOK || peices[j] == enemy + QUEEN) {
                            return false;
                        }
                        break;
                    }
                }
                
                for (_int j = s - 9; j > DIRECTION_BOUNDS[s][BL]; j -= 9) {
                    if (peices[j]) {
                        if (peices[j] == enemy + BISHOP || peices[j] == enemy + QUEEN) {
                            return false;
                        }
                        break;
                    }
                }

                for (_int j = s - 7; j > DIRECTION_BOUNDS[s][BR]; j -= 7) {
                    if (peices[j]) {
                        if (peices[j] == enemy + BISHOP || peices[j] == enemy + QUEEN) {
                            return false;
                        }
                        break;
                    }
                }
            
            } else {
                for (_int j = s + 8; j < DIRECTION_BOUNDS[s][F]; j += 8) {
                    if (peices[j]) {
                        if (peices[j] == enemy + ROOK || peices[j] == enemy + QUEEN) {
                            return false;
                        }
                        break;
                    }
                }

                for (_int j = s + 9; j < DIRECTION_BOUNDS[s][FR]; j += 9) {
                    if (peices[j]) {
                        if (peices[j] == enemy + BISHOP || peices[j] == enemy + QUEEN) {
                            return false;
                        }
                        break;
                    }
                }

                for (_int j = s + 7; j < DIRECTION_BOUNDS[s][FL]; j += 7) {
                    if (peices[j]) {
                        if (peices[j] == enemy + BISHOP || peices[j] == enemy + QUEEN) {
                            return false;
                        }
                        break;
                    }
                }
            }

            // King attacks
            for (_int j = 1; j < KING_MOVES[s][0]; ++j) {
                if (peices[KING_MOVES[s][j]] == enemy + KING) {
                    return false;
                }
            }
        }

        move.flags |= Move::LEGAL;
        return true;
    }
};

#endif