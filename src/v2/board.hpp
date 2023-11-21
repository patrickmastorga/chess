#include <cstdint>
#include <vector>
#include <stack>
#include <unordered_set>
#include <algorithm>
#include <stdexcept>
#include <string>
#include <sstream>
#include <cctype>

#include "precomputed.hpp"

typedef std::uint_fast8_t uint8;
typedef std::int_fast8_t int8;
typedef std::uint_fast16_t uint16;
typedef std::int_fast16_t int16;
typedef std::uint_fast32_t uint32;
typedef std::uint_fast64_t uint64;

/**
 * class representing the current state of the chess game
 */
class Board
{
public:
    // DEFINITIONS
    static constexpr uint8 WHITE = 0b0000;
    static constexpr uint8 BLACK = 0b1000;
    static constexpr uint8 PAWN =   0b001;
    static constexpr uint8 KNIGHT = 0b010;
    static constexpr uint8 BISHOP = 0b011;
    static constexpr uint8 ROOK =   0b100;
    static constexpr uint8 QUEEN =  0b101;
    static constexpr uint8 KING =   0b110;


    // MOVE STRUCT
    /**
     * struct for containing info about a move
     */
    struct Move
    {
        /**
         * Starting square of the move [0, 63] -> [a1, h8]
         */
        uint8 startSquare;

        /**
         * Ending square of the move [0, 63] -> [a1, h8]
         */
        uint8 targetSquare;

        /**
         * Peice and color of moving peice
         */
        uint8 movingPeice;

        /**
         * Peice and color of captured peice
         */
        uint8 capturedPeice;

        /**
         * 8 bit flags of move
         */
        uint8 flags;

        /**
         * Heuristic geuss for the strength of the move (used for move ordering)
         */
        int16 moveStrengthGuess;

        
        // CONSTRUCTORS
        /**
         * Construct a new Move object from the given board and given flags (en_passant, castle, promotion, etc.)
         */
        Move(const Board *board, uint8 start, uint8 target, uint8 givenFlags) : startSquare(start), targetSquare(target), flags(givenFlags), moveStrengthGuess(0)
        {
            movingPeice = board->peices[start];
            uint8 color = movingPeice & 0b11111000;

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

        /**
         * @brief Construct a new Move object from the given board with unknown flags (en_passant, castle, promotion, etc.)
         */
        Move(const Board *board, uint8 start, uint8 target, uint8 promotion) : startSquare(start), targetSquare(target), moveStrengthGuess(0)
        {
            // TODO
        }


        // PUBLIC METHODS
        /**
         * Generates a heurusitic guess for the strength of a move based on information from the board
         */
        void initializeMoveStrengthGuess(Board *Board)
        {
            // TODO
        }

        /**
         * Override stream insertion operator to display info about the move
         */
        std::ostream& operator<<(std::ostream& os) const {
            os << "(" << Board::indexToAlgebraic(startSquare) << " -> " << Board::indexToAlgebraic(targetSquare) << ")";
            return os;
        }


        // FLAGS
        static constexpr uint8 NONE       = 0b00000000;
        static constexpr uint8 PROMOTION  = 0b00000111;
        static constexpr uint8 LEGAL      = 0b00001000;
        static constexpr uint8 EN_PASSANT = 0b00010000;
        static constexpr uint8 CASTLE     = 0b00100000;
        static constexpr uint8 CAPTURE    = 0b01000000;
        static constexpr uint8 CHECK      = 0b10000000;
    };
    
    
    // CONSTRUCTORS
    /**
     * Construct a new Board object from fen string
     * @param fenString string in Forsyth–Edwards Notation
     */
    Board(const std::string &fenString)
    {
        std::istringstream fenStringStream(fenString);
        std::string peicePlacementData, activeColor, castlingAvailabilty, enPassantTarget, halfmoveClock, fullmoveNumber;

        peiceIndices[0] = std::unordered_set<uint8>(23);
        peiceIndices[1] = std::unordered_set<uint8>(23);
        
        zobrist = 0;
        
        // Get peice placement data from fen string
        if (!std::getline(fenStringStream, peicePlacementData, ' ')) {
            throw std::invalid_argument("Cannot get peice placement from FEN!");
        }

        // Update the peices[] according to fen rules
        int8 peiceIndex = 56;
        for (char c : peicePlacementData) {
            if (std::isalpha(c)) {
                // char contains data about a peice
                uint8 color = std::islower(c);

                peiceIndices[color].insert(peiceIndex);
                color <<= 3;
                switch (c) {
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

            } else if (std::isdigit(c)) {
                // char contains data about gaps between peices
                uint8 gap = c - '0';
                for (uint8 i = 0; i < gap; ++i) {
                    peices[peiceIndex++] = 0;
                }

            } else {
                if (c != '/') {
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
            for (char c : castlingAvailabilty) {
                uint8 color = std::islower(c);
                switch (c) {
                    case 'K':
                    case 'k':
                        kingsideCastlingRightsLost[color] = 0;
                        zobrist ^= ZOBRIST_KINGSIDE_CASTLING_KEYS[color];
                        break;
                    case 'Q':
                    case 'q':
                        queensideCastlingRightsLost[color] = 0;
                        zobrist ^= ZOBRIST_QUEENSIDE_CASTLING_KEYS[color];
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
                eligibleEnPassantFile.push(algebraicToIndex(enPassantTarget) % 8);
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
            halfmovesSincePawnMoveOrCapture.push(static_cast<uint8>(std::stoi(halfmoveClock)));
        } catch (const std::invalid_argument &e) {
            throw std::invalid_argument(std::string("Invalid FEN half move clock! ") + e.what());
        }

        // Get full move number data from fen string
        if (!std::getline(fenStringStream, fullmoveNumber, ' ')) {
            throw std::invalid_argument("Cannot getfullmove number from FEN!");
        }

        try {
            totalHalfmoves += static_cast<uint16>(std::stoi(fullmoveNumber) * 2 - 2);
        } catch (const std::invalid_argument &e) {
            throw std::invalid_argument(std::string("Invalid FEN full move number! ") + e.what());
        }


        // initialize zobrist hash for all of the peices
        
        for (uint8 i = 0; i < 64; ++i) {
            if (peices[i]) {
                zobrist ^= ZOBRIST_PEICE_KEYS[peices[i] >> 3][peices[i] % (1 << 3) - 1][i];
            }
        }
    }
    
    /**
     * Construct a new Board object with the starting position
     */
    Board() : Board("rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1") {}


    // PUBLIC METHODS
    /**
     * Generates pseudo-legal moves for the current position
     * @param flags only generate moves with the given flags
     * @return std::vector<Move> of pseudo-legal moves for a position
     */
    std::vector<Move> pseudoLegalMoves(uint8 flags=Move::NONE) const
    {
        // TODO Backwards check/pin generation for endgame
        // TODO Better pinned peice generation
        
        uint8 color = totalHalfmoves % 2;
        uint8 enemy = !color;

        // Used for optimizing the legality checking of moves
        std::unordered_set<uint8> pinnedPeices(11);
        std::unordered_set<uint8> checkingSquares(11);

        // Check if king in check and record pinned peices and checking squares
        uint8 king = kingIndex[color];
        uint8 checks = 0;

        // Pawn checks
        uint8 kingFile = king % 8;
        uint8 ahead = king + 8 - 16 * color;
        if (kingFile != 0 && peices[ahead - 1] == enemy + PAWN) {
            checkingSquares.insert(ahead - 1);
            ++checks;
        }
        if (kingFile != 7 && peices[ahead + 1] == enemy + PAWN) {
            checkingSquares.insert(ahead + 1);
            ++checks;
        }

        // Knight checks
        for (uint8 j = 0; j < KNIGHT_MOVES[king][0]; ++j) {
            if (peices[KNIGHT_MOVES[king][j]] == enemy + KNIGHT) {
                checkingSquares.insert(KNIGHT_MOVES[king][j]);
                ++checks;
            }
        }

        // pins and sliding peice checks
        uint8 potentialPin = 0;
        for (uint8 j = king - 8; j < DIRECTION_BOUNDS[king][B]; j -= 8) {
            if (!peices[j]) {
                continue;
            }
            if (!potentialPin && peices[j] >> 3 == color) {
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
                for (uint8 k = j; k < king; k += 8) {
                    checkingSquares.insert(k);
                }
            }
            break;
        }
        
        potentialPin = 0;
        for (uint8 j = king + 8; j < DIRECTION_BOUNDS[king][F]; j += 8) {
            if (!peices[j]) {
                continue;
            }
            if (!potentialPin && peices[j] >> 3 == color) {
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
                for (uint8 k = j; k > king; k -= 8) {
                    checkingSquares.insert(k);
                }
            }
            break;
        }

        potentialPin = 0;
        for (uint8 j = king - 1; j < DIRECTION_BOUNDS[king][L]; j -= 1) {
            if (!peices[j]) {
                continue;
            }
            if (!potentialPin && peices[j] >> 3 == color) {
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
                for (uint8 k = j; k < king; k += 1) {
                    checkingSquares.insert(k);
                }
            }
            break;
        }

        potentialPin = 0;
        for (uint8 j = king + 1; j < DIRECTION_BOUNDS[king][R]; j += 1) {
            if (!peices[j]) {
                continue;
            }
            if (!potentialPin && peices[j] >> 3 == color) {
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
                for (uint8 k = j; k > king; k -= 1) {
                    checkingSquares.insert(k);
                }
            }
            break;
        }

        potentialPin = 0;
        for (uint8 j = king - 9; j < DIRECTION_BOUNDS[king][BL]; j -= 9) {
            if (!peices[j]) {
                continue;
            }
            if (!potentialPin && peices[j] >> 3 == color) {
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
                for (uint8 k = j; k < king; k += 9) {
                    checkingSquares.insert(k);
                }
            }
            break;
        }

        potentialPin = 0;
        for (uint8 j = king + 9; j < DIRECTION_BOUNDS[king][FR]; j += 9) {
            if (!peices[j]) {
                continue;
            }
            if (!potentialPin && peices[j] >> 3 == color) {
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
                for (uint8 k = j; k > king; k -= 9) {
                    checkingSquares.insert(k);
                }
            }
            break;
        }

        potentialPin = 0;
        for (uint8 j = king - 7; j < DIRECTION_BOUNDS[king][BR]; j -= 7) {
            if (!peices[j]) {
                continue;
            }
            if (!potentialPin && peices[j] >> 3 == color) {
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
                for (uint8 k = j; k < king; k += 7) {
                    checkingSquares.insert(k);
                }
            }
            break;
        }

        potentialPin = 0;
        for (uint8 j = king + 7; j < DIRECTION_BOUNDS[king][FL]; j += 7) {
            if (!peices[j]) {
                continue;
            }
            if (!potentialPin && peices[j] >> 3 == color) {
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
                for (uint8 k = j; k > king; k -= 7) {
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
            for (uint8 j = 0; j < KING_MOVES[king][0]; ++j) {
                if (!peices[KING_MOVES[king][j]] || peices[KING_MOVES[king][j]] >> 3 == enemy) {
                    moves.emplace_back(this, king, KING_MOVES[king][j], Move::NONE);
                }
            }
            return moves;
        }

        // En passant moves
        uint8 epfile = eligibleEnPassantFile.top();
        if (epfile > 0) {
            if (color == WHITE >> 3) {
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
        if (checks && checkingSquares.size() < peiceIndices[color].size() * 0.4) {
            // Generate king moves
            for (uint8 j = 0; j < KING_MOVES[king][0]; ++j) {
                if (!peices[KING_MOVES[king][j]] || peices[KING_MOVES[king][j]] >> 3 == enemy) {
                    moves.emplace_back(this, king, KING_MOVES[king][j], Move::NONE);
                }
            }
           
            // Backwards search from checking squares to see if peices can move to them
            for (uint8 t : checkingSquares) {

                // Pawn can block/take
                if ((color == WHITE >> 3 && t >> 3 >= 2) || (color == BLACK >> 3 && t >> 3 <= 5)) {
                    uint8 file = t % 8;
                    uint8 ahead = t - 8 + 16 * color;
                    bool promotion = ahead >> 3 == 0 || ahead >> 3 == 7;
                    // Pawn capture
                    if (peices[t] >> 3 == enemy) { 
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
                        uint8 doubleAhead = ahead - 8 + 16 * color;
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
                for (uint8 j = 0; j < KNIGHT_MOVES[t][0]; ++j) {
                    if (peices[KNIGHT_MOVES[t][j]] == color + KNIGHT && !pinnedPeices.count(KNIGHT_MOVES[t][j])) {
                        moves.emplace_back(this, KNIGHT_MOVES[t][j], t, Move::LEGAL);
                    }
                }

                // Sliding peices can block/take
                for (uint8 s = t - 8; s < DIRECTION_BOUNDS[king][B]; s -= 8) {
                    if (peices[s]) {
                        if ((peices[s] == color + ROOK || peices[s] == color + QUEEN) && !pinnedPeices.count(s)) {
                            moves.emplace_back(this, s, t, Move::LEGAL);
                        }
                        break;
                    }
                }

                for (uint8 s = t + 8; s < DIRECTION_BOUNDS[king][F]; s += 8) {
                    if (peices[s]) {
                        if ((peices[s] == color + ROOK || peices[s] == color + QUEEN) && !pinnedPeices.count(s)) {
                            moves.emplace_back(this, s, t, Move::LEGAL);
                        }
                        break;
                    }
                }

                for (uint8 s = t - 1; s < DIRECTION_BOUNDS[king][L]; s -= 1) {
                    if (peices[s]) {
                        if ((peices[s] == color + ROOK || peices[s] == color + QUEEN) && !pinnedPeices.count(s)) {
                            moves.emplace_back(this, s, t, Move::LEGAL);
                        }
                        break;
                    }
                }

                for (uint8 s = t + 1; s < DIRECTION_BOUNDS[king][R]; s += 1) {
                    if (peices[s]) {
                        if ((peices[s] == color + ROOK || peices[s] == color + QUEEN) && !pinnedPeices.count(s)) {
                            moves.emplace_back(this, s, t, Move::LEGAL);
                        }
                        break;
                    }
                }

                for (uint8 s = t - 9; s < DIRECTION_BOUNDS[king][BL]; s -= 9) {
                    if (peices[s]) {
                        if ((peices[s] == color + BISHOP || peices[s] == color + QUEEN) && !pinnedPeices.count(s)) {
                            moves.emplace_back(this, s, t, Move::LEGAL);
                        }
                        break;
                    }
                }

                for (uint8 s = t + 9; s < DIRECTION_BOUNDS[king][FR]; s += 9) {
                    if (peices[s]) {
                        if ((peices[s] == color + BISHOP || peices[s] == color + QUEEN) && !pinnedPeices.count(s)) {
                            moves.emplace_back(this, s, t, Move::LEGAL);
                        }
                        break;
                    }
                }

                for (uint8 s = t - 7; s < DIRECTION_BOUNDS[king][BR]; s -= 7) {
                    if (peices[s]) {
                        if ((peices[s] == color + BISHOP || peices[s] == color + QUEEN) && !pinnedPeices.count(s)) {
                            moves.emplace_back(this, s, t, Move::LEGAL);
                        }
                        break;
                    }
                }

                for (uint8 s = t + 7; s < DIRECTION_BOUNDS[king][FL]; s += 7) {
                    if (peices[s]) {
                        if ((peices[s] == color + BISHOP || peices[s] == color + QUEEN) && !pinnedPeices.count(s)) {
                            moves.emplace_back(this, s, t, Move::LEGAL);
                        }
                        break;
                    }
                }
            }
        }

        // Castling
        if (!kingsideCastlingRightsLost[color]) {
            uint8 castlingRank = 56 * color;
            moves.emplace_back(this, castlingRank + 4, castlingRank + 6, Move::CASTLE);
        }
        if (!queensideCastlingRightsLost[color]) {
            uint8 castlingRank = 56 * color;
            moves.emplace_back(this, castlingRank + 4, castlingRank + 2, Move::CASTLE);
        }

        // General case
        for (uint8 s : peiceIndices[color]) {
            uint8 legalFlag = pinnedPeices.count(s) ? Move::LEGAL : Move::NONE;

            switch (peices[s] % (1 << 3)) {
                case PAWN:
                    uint8 file = s % 8;
                    uint8 ahead = s + 8 - 16 * color;
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

                        if ((s >> 3 == 1 || s >> 3 == 6) && !peices[ahead + 8 - 16 * color]) {
                            moves.emplace_back(this, s, ahead + 8 - 16 * color, legalFlag);
                        }
                    }

                    // Pawn captures
                    if (file != 0 && peices[ahead - 1] && peices[ahead - 1] >> 3 == enemy) {
                        if (promotion) {
                            moves.emplace_back(this, s, ahead - 1, legalFlag | KNIGHT);
                            moves.emplace_back(this, s, ahead - 1, legalFlag | BISHOP);
                            moves.emplace_back(this, s, ahead - 1, legalFlag | ROOK);
                            moves.emplace_back(this, s, ahead - 1, legalFlag | QUEEN);
                        } else {
                            moves.emplace_back(this, ahead - 1, legalFlag);
                        }
                    }
                    if (file != 7 && peices[ahead + 1] && peices[ahead + 1] >> 3 == enemy) {
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

                case KNIGHT:
                    uint8 t;
                    for (uint8 j = 1; j < KNIGHT_MOVES[s][0]; ++j) {
                        t = KNIGHT_MOVES[s][j];
                        if ((!peices[t] || peices[t] >> 3 == enemy) && (!checks || checkingSquares.count(t))) {
                            moves.emplace_back(this, s, t, legalFlag);
                        }
                    }
                    break;

                case BISHOP:
                case ROOK:
                case QUEEN:
                    if (peices[s] % (1 << 3) == BISHOP) {
                        goto bishopMoves;
                    }

                    for (uint8 t = s - 8; t < DIRECTION_BOUNDS[s][B]; t -= 8) {
                        if ((!peices[t] || peices[t] >> 3 == enemy) && (!checks || checkingSquares.count(t))) {
                            moves.emplace_back(this, s, t, legalFlag);
                        }
                        if (peices[t]) {
                            break;
                        }
                    }

                    for (uint8 t = s + 8; t < DIRECTION_BOUNDS[s][F]; t += 8) {
                        if ((!peices[t] || peices[t] >> 3 == enemy) && (!checks || checkingSquares.count(t))) {
                            moves.emplace_back(this, s, t, legalFlag);
                        }
                        if (peices[t]) {
                            break;
                        }
                    }

                    for (uint8 t = s - 1; t < DIRECTION_BOUNDS[s][L]; t -= 1) {
                        if ((!peices[t] || peices[t] >> 3 == enemy) && (!checks || checkingSquares.count(t))) {
                            moves.emplace_back(this, s, t, legalFlag);
                        }
                        if (peices[t]) {
                            break;
                        }
                    }

                    for (uint8 t = s + 1; t < DIRECTION_BOUNDS[s][R]; t += 1) {
                        if ((!peices[t] || peices[t] >> 3 == enemy) && (!checks || checkingSquares.count(t))) {
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

                    for (uint8 t = s - 9; t < DIRECTION_BOUNDS[s][BL]; t -= 9) {
                        if ((!peices[t] || peices[t] >> 3 == enemy) && (!checks || checkingSquares.count(t))) {
                            moves.emplace_back(this, s, t, legalFlag);
                        }
                        if (peices[t]) {
                            break;
                        }
                    }

                    for (uint8 t = s + 9; t < DIRECTION_BOUNDS[s][FR]; t += 9) {
                        if ((!peices[t] || peices[t] >> 3 == enemy) && (!checks || checkingSquares.count(t))) {
                            moves.emplace_back(this, s, t, legalFlag);
                        }
                        if (peices[t]) {
                            break;
                        }
                    }

                    for (uint8 t = s - 7; t < DIRECTION_BOUNDS[s][BR]; t -= 7) {
                        if ((!peices[t] || peices[t] >> 3 == enemy) && (!checks || checkingSquares.count(t))) {
                            moves.emplace_back(this, s, t, legalFlag);
                        }
                        if (peices[t]) {
                            break;
                        }
                    }

                    for (uint8 t = s + 7; t < DIRECTION_BOUNDS[s][FL]; t += 7) {
                        if ((!peices[t] || peices[t] >> 3 == enemy) && (!checks || checkingSquares.count(t))) {
                            moves.emplace_back(this, s, t, legalFlag);
                        }
                        if (peices[t]) {
                            break;
                        }
                    }
                    
                    break;
                
                case KING:
                    uint8 t;
                    for (uint8 j = 1; j < KING_MOVES[s][0]; ++j) {
                        t = KING_MOVES[s][j];
                        if (!peices[t] || peices[t] >> 3 == enemy) {
                            moves.emplace_back(this, s, t, Move::NONE);
                        }
                    }
            }
            
        }

        return moves;
    }

    /**
     * update the board based on the inputted move
     */
    void makeMove(const Move &move)
    {
        uint8 color = move.movingPeice >> 3;
        uint8 enemy = !color;
        uint8 moving = move.movingPeice % (1 << 3);

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
            zobrist ^= ZOBRIST_PEICE_KEYS[color][PAWN - 1][move.startSquare];
            zobrist ^= ZOBRIST_PEICE_KEYS[color][moving - 1][move.targetSquare];
            moving = PAWN;
        
        } else { 
            zobrist ^= ZOBRIST_PEICE_KEYS[color][moving - 1][move.startSquare];
            zobrist ^= ZOBRIST_PEICE_KEYS[color][moving - 1][move.targetSquare];
        }

        // Update zobrist hash and peice indices set for capture
        if (move.flags & Move::EN_PASSANT) {
            uint8 captureSquare = move.targetSquare - 8 + 16 * color;
            peices[captureSquare] = 0;
            zobrist ^= ZOBRIST_PEICE_KEYS[enemy][move.capturedPeice % (1 << 3) - 1][captureSquare];
            peiceIndices[enemy].erase(captureSquare);

        } else if (move.capturedPeice) {
            zobrist ^= ZOBRIST_PEICE_KEYS[enemy][move.capturedPeice % (1 << 3) - 1][move.targetSquare];
            peiceIndices[enemy].erase(move.targetSquare);
        }

        // increment counters
        ++totalHalfmoves;
        if (move.capturedPeice || moving == PAWN) {
            halfmovesSincePawnMoveOrCapture.push(0);
        } else {
            uint8 prev = halfmovesSincePawnMoveOrCapture.top();
            halfmovesSincePawnMoveOrCapture.pop();
            halfmovesSincePawnMoveOrCapture.push(prev + 1);
        }

        // Update peices array
        peices[move.startSquare] = 0;
        peices[move.targetSquare] = move.movingPeice;

        // Update peice indices set
        peiceIndices[color].erase(move.startSquare);
        peiceIndices[color].insert(move.targetSquare);

        // Update rooks for castling
        if (move.flags & Move::CASTLE) {
            uint8 castlingRank = move.targetSquare & 0b11111000;

            if (move.targetSquare % 8 < 4) {
                // Queenside castling
                peices[castlingRank + 3] = peices[castlingRank];
                peices[castlingRank] = 0;
                zobrist ^= ZOBRIST_PEICE_KEYS[color][ROOK - 1][castlingRank + 3];
                zobrist ^= ZOBRIST_PEICE_KEYS[color][ROOK - 1][castlingRank];
                peiceIndices[color].erase(castlingRank);
                peiceIndices[color].insert(castlingRank + 3);
                
            } else {
                // Kingside castling
                peices[castlingRank + 5] = peices[castlingRank + 7];
                peices[castlingRank + 7] = 0;
                zobrist ^= ZOBRIST_PEICE_KEYS[color][ROOK - 1][castlingRank + 5];
                zobrist ^= ZOBRIST_PEICE_KEYS[color][ROOK - 1][castlingRank + 7];
                peiceIndices[color].erase(castlingRank + 7);
                peiceIndices[color].insert(castlingRank + 5);
            }
        }

        // update castling rights
        if (!kingsideCastlingRightsLost[color]) {
            if (moving == KING || (moving == ROOK && move.startSquare % 8 == 7)) {
                kingsideCastlingRightsLost[color] = totalHalfmoves;
                zobrist ^= ZOBRIST_KINGSIDE_CASTLING_KEYS[color];
            }
        }
        if (!queensideCastlingRightsLost[color]) {
            if (moving == KING || (moving == ROOK && move.startSquare % 8 == 0)) {
                kingsideCastlingRightsLost[color] = totalHalfmoves;
                zobrist ^= ZOBRIST_QUEENSIDE_CASTLING_KEYS[color];
            }
        }
        
        // Update king index
        if (moving == KING) {
            kingIndex[color] = move.targetSquare;
        }
    }

    /**
     * update the board based on the inputted move
     */
    void unmakeMove(const Move &move)
    {
        uint8 color = move.movingPeice >> 3;
        uint8 enemy = !color;
        uint8 moving = move.movingPeice % (1 << 3);

        // Undo en passant file
        if (eligibleEnPassantFile.top() >= 0) {
            zobrist ^= ZOBRIST_EN_PASSANT_KEYS[eligibleEnPassantFile.top()];
        }
        eligibleEnPassantFile.pop();
        if (eligibleEnPassantFile.top() >= 0) {
            zobrist ^= ZOBRIST_EN_PASSANT_KEYS[eligibleEnPassantFile.top()];
        }

        // Undo zobrist hash for turn change
        zobrist ^= ZOBRIST_TURN_KEY;
        
        // Undo zobrist hash for moving peice
        if (move.flags & Move::PROMOTION) {
            zobrist ^= ZOBRIST_PEICE_KEYS[color][PAWN - 1][move.startSquare];
            zobrist ^= ZOBRIST_PEICE_KEYS[color][moving - 1][move.targetSquare];
            moving = PAWN;
        
        } else { 
            zobrist ^= ZOBRIST_PEICE_KEYS[color][moving - 1][move.startSquare];
            zobrist ^= ZOBRIST_PEICE_KEYS[color][moving - 1][move.targetSquare];
        }

        // undo zobrist hash and peice indices set for capture
        if (move.flags & Move::EN_PASSANT) {
            uint8 captureSquare = move.targetSquare - 8 + 16 * color;
            peices[captureSquare] = move.capturedPeice;
            zobrist ^= ZOBRIST_PEICE_KEYS[enemy][move.capturedPeice % (1 << 3) - 1][captureSquare];
            peiceIndices[enemy].insert(captureSquare);

        } else if (move.capturedPeice) {
            zobrist ^= ZOBRIST_PEICE_KEYS[enemy][move.capturedPeice % (1 << 3) - 1][move.targetSquare];
            peiceIndices[enemy].insert(move.targetSquare);
        }

        // decrement counters
        --totalHalfmoves;
        if (move.capturedPeice || moving == PAWN) {
            halfmovesSincePawnMoveOrCapture.pop();
        } else {
            uint8 prev = halfmovesSincePawnMoveOrCapture.top();
            halfmovesSincePawnMoveOrCapture.pop();
            halfmovesSincePawnMoveOrCapture.push(prev - 1);
        }

        // undo peices array
        peices[move.startSquare] = moving;
        peices[move.targetSquare] = (move.flags & Move::EN_PASSANT) ? 0 : move.capturedPeice;
        
        // undo peice indices set
        peiceIndices[color].erase(move.targetSquare);
        peiceIndices[color].insert(move.startSquare);

        // undo rooks for castling
        if (move.flags & Move::CASTLE) {
            uint8 castlingRank = move.targetSquare & 0b11111000;

            if (move.targetSquare % 8 < 4) {
                // Queenside castling
                peices[castlingRank] = peices[castlingRank + 3];
                peices[castlingRank + 3] = 0;
                zobrist ^= ZOBRIST_PEICE_KEYS[color][ROOK - 1][castlingRank + 3];
                zobrist ^= ZOBRIST_PEICE_KEYS[color][ROOK - 1][castlingRank];
                peiceIndices[color].erase(castlingRank + 3);
                peiceIndices[color].insert(castlingRank);
                
            } else {
                // Kingside castling
                peices[castlingRank + 7] = peices[castlingRank + 5];
                peices[castlingRank + 5] = 0;
                zobrist ^= ZOBRIST_PEICE_KEYS[color][ROOK - 1][castlingRank + 5];
                zobrist ^= ZOBRIST_PEICE_KEYS[color][ROOK - 1][castlingRank + 7];
                peiceIndices[color].erase(castlingRank + 5);
                peiceIndices[color].insert(castlingRank + 7);
            }
        }

        // undo castling rights
        if (kingsideCastlingRightsLost[color] == totalHalfmoves) {
            kingsideCastlingRightsLost[color] = 0;
        }
        if (!queensideCastlingRightsLost[color] == totalHalfmoves) {
            queensideCastlingRightsLost[color] = 0;
        }
        
        // Update king index
        if (moving == KING) {
            kingIndex[color] = move.startSquare;
        }
    }

    /**
     * @return true if inputted move is legal in the current position
     */
    bool isLegal(const Move &move)
    {
        if (move.flags & Move::LEGAL) {
            return true;
        }

        if (move.flags & Move::CASTLE) {
            // TODO
        }

        uint8 color = totalHalfmoves % 2;
        makeMove(move);
        bool legal = !inCheck(color);
        unmakeMove(move);
        return legal;
    }

    /**
     * @return true if the king belonging to the inputted color is currently being attacked
     */
    bool inCheck(uint8 color) const
    {
        // TODO Backwards check searching during endgame

        uint8 enemy = !color;
        uint8 king = kingIndex[color];

        // Pawn checks
        uint8 kingFile = king % 8;
        uint8 ahead = king + 8 - 16 * color;
        if (kingFile != 0 && peices[ahead - 1] == enemy + PAWN) {
            return true;
        }
        if (kingFile != 7 && peices[ahead + 1] == enemy + PAWN) {
            return true;
        }

        // Knight checks
        for (uint8 j = 0; j < KNIGHT_MOVES[king][0]; ++j) {
            if (peices[KNIGHT_MOVES[king][j]] == enemy + KNIGHT) {
                return true;
            }
        }

        // sliding peice checks
        for (uint8 j = king - 8; j < DIRECTION_BOUNDS[king][B]; j -= 8) {
            if (peices[j]) {
                if (peices[j] == enemy + ROOK || peices[j] == enemy + QUEEN) {
                    return true;
                }
                break;
            }
        }
        
        for (uint8 j = king + 8; j < DIRECTION_BOUNDS[king][F]; j += 8) {
            if (peices[j]) {
                if (peices[j] == enemy + ROOK || peices[j] == enemy + QUEEN) {
                    return true;
                }
                break;
            }
        }

        for (uint8 j = king - 1; j < DIRECTION_BOUNDS[king][L]; j -= 1) {
            if (peices[j]) {
                if (peices[j] == enemy + ROOK || peices[j] == enemy + QUEEN) {
                    return true;
                }
                break;
            }
        }

        for (uint8 j = king + 1; j < DIRECTION_BOUNDS[king][R]; j += 1) {
            if (peices[j]) {
                if (peices[j] == enemy + ROOK || peices[j] == enemy + QUEEN) {
                    return true;
                }
                break;
            }
        }

        for (uint8 j = king - 9; j < DIRECTION_BOUNDS[king][BL]; j -= 9) {
            if (peices[j]) {
                if (peices[j] == enemy + BISHOP || peices[j] == enemy + QUEEN) {
                    return true;
                }
                break;
            }
        }

        for (uint8 j = king + 9; j < DIRECTION_BOUNDS[king][FR]; j += 9) {
            if (peices[j]) {
                if (peices[j] == enemy + BISHOP || peices[j] == enemy + QUEEN) {
                    return true;
                }
                break;
            }
        }

        for (uint8 j = king - 7; j < DIRECTION_BOUNDS[king][BR]; j -= 7) {
            if (peices[j]) {
                if (peices[j] == enemy + BISHOP || peices[j] == enemy + QUEEN) {
                    return true;
                }
                break;
            }
        }

        for (uint8 j = king + 7; j < DIRECTION_BOUNDS[king][FL]; j += 7) {
            if (peices[j]) {
                if (peices[j] == enemy + BISHOP || peices[j] == enemy + QUEEN) {
                    return true;
                }
                break;
            }
        }

        // King checks (seems wierd, but needed for detecting illegal moves)
        for (uint8 j = 0; j < KING_MOVES[king][0]; ++j) {
            if (peices[KING_MOVES[king][j]] == enemy + KING) {
                return true;
            }
        }

        return false;
    }

    /**
     * @return true if the player who is to move is in check
     */
    bool inCheck() const
    {
        return inCheck(totalHalfmoves % 2);
    }
    
    bool isDraw() const
    {
        // TODO
    }
    
    /**
     * @return 64 bit hashing of the current state of the game
     */
    uint64 zobristHash() const
    {
        return zobrist;
    }

private:
    // PRIVATE MEMBERS
    /**
     * color and peice type at every square (index [0, 63] -> [a1, h8])
     */
    uint8 peices[64];

    /**
     * contains the halfmove number when the kingside castling rights were lost for white or black (index 0 and 1)
     */
    uint16 kingsideCastlingRightsLost[2];

    /**
     * contains the halfmove number when the queenside castling rights were lost for white or black (index 0 and 1)
     */
    uint16 queensideCastlingRightsLost[2];

    /**
     * file where a pawn has just moved two squares over
     */
    std::stack<int8> eligibleEnPassantFile;

    /**
     * number of half moves since pawn move or capture (half move is one player taking a turn) (used for 50 move rule)
     */
    std::stack<uint8> halfmovesSincePawnMoveOrCapture;

    /**
     * total half moves since game start (half move is one player taking a turn)
     */
    uint16 totalHalfmoves;

    /**
     * Index of the white and black king (index 0 and 1)
     */
    uint8 kingIndex[2];

    /**
     * Indices of all white and black peices (index 0 and 1)
     */
    std::unordered_set<uint8> peiceIndices[2];

    /**
     * zobrist hash of the current position
     */
    uint64 zobrist;

    
    // PRIVATE METHODS
    /**
     * @param algebraic notation for position on chess board (ex e3, a1, c8)
     * @return uint8 index [0, 63] -> [a1, h8] of square on board
     */
    static uint8 algebraicToIndex(const std::string &algebraic)
    {
        if (algebraic.size() != 2) {
            throw std::invalid_argument("Algebraic notation should only be two letters long!");
        }

        uint8 file = algebraic[0] - 'a';
        uint8 rank = algebraic[1] - '1';

        if (file < 0 || file > 7 || rank < 0 || rank > 7) {
            throw std::invalid_argument("Algebraic notation should be in the form [a-h][1-8]!");
        }

        return (rank - '1') * 8 + (file - 'a');
    }

    /**
     * @param boardIndex index [0, 63] -> [a1, h8] of square on board
     * @return std::string notation for position on chess board (ex e3, a1, c8)
     */
    static std::string& indexToAlgebraic(uint8 boardIndex)
    {
        char file = 'a' + boardIndex % 8;
        char rank = '1' + boardIndex >> 3;
        
        return {file, rank};
    }
};

/*

TODO REGULAR SEARCH

TODO ALPHA BETA SEARCH WITH MOVE ORDERING

TODO ALPHA BETA SEARCH WITH TRANSPOSITION TABLE

function alpha_beta_search(node, depth, alpha, beta):
    if depth == 0 or node is a terminal node:
        return evaluate(node)

    transposition_entry = transposition_table_lookup(node)
    if transposition_entry is not empty and transposition_entry.depth >= depth:
        if transposition_entry.flag == EXACT:
            return transposition_entry.value
        if transposition_entry.flag == LOWER_BOUND:
            alpha = max(alpha, transposition_entry.value)
        else:  # transposition_entry.flag == UPPER_BOUND
            beta = min(beta, transposition_entry.value)

        if alpha >= beta:
            return transposition_entry.value  # Transposition table cutoff

    if maximizing_player(node):
        value = -infinity
        for child in generate_moves(node):
            value = max(value, alpha_beta_search(child, depth - 1, alpha, beta))
            alpha = max(alpha, value)
            if alpha >= beta:
                break  # Beta cutoff
    else:  # minimizing player
        value = infinity
        for child in generate_moves(node):
            value = min(value, alpha_beta_search(child, depth - 1, alpha, beta))
            beta = min(beta, value)
            if alpha >= beta:
                break  # Alpha cutoff

    store_transposition_entry(node, value, depth, alpha, beta)
    return value


// For adding to transpostiontion table
if value == alpha:
    flag = LOWER_BOUND
elif value == beta:
    flag = UPPER_BOUND
else:
    flag = EXACT
*/