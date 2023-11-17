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
 * struct for containing info about the current state of the chess game
 */
struct Board
{
    // DEFINITIONS
    static constexpr uint8 WHITE = 0b0000;
    static constexpr uint8 BLACK = 0b1000;
    static constexpr uint8 PAWN =   0b001;
    static constexpr uint8 KNIGHT = 0b010;
    static constexpr uint8 BISHOP = 0b011;
    static constexpr uint8 ROOK =   0b100;
    static constexpr uint8 QUEEN =  0b101;
    static constexpr uint8 KING =   0b110;


    // MEMBERS
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
     * zobrist hash of the current position
     */
    uint64 zobristHash;

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

        /**
         * Construct a new Move object from the given board
         */
        Move(Board *board, uint8 start, uint8 target, bool guarenteedLegal=false)
        {
            // TODO: move constructor
        }

        // FLAGS
        static constexpr uint8 NONE       = 0b00000;
        static constexpr uint8 CAPTURE    = 0b00001;
        static constexpr uint8 PROMOTION  = 0b00010;
        static constexpr uint8 CHECK      = 0b00100;
        static constexpr uint8 EN_PASSANT = 0b01000;
        static constexpr uint8 CASTLE     = 0b10000;
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

        peiceIndices[0] = std::unordered_set<uint8>(20);
        peiceIndices[1] = std::unordered_set<uint8>(20);
        
        zobristHash = 0;
        
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
            zobristHash ^= ZOBRIST_TURN_KEY;
        
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
                        zobristHash ^= ZOBRIST_KINGSIDE_CASTLING_KEYS[color];
                        break;
                    case 'Q':
                    case 'q':
                        queensideCastlingRightsLost[color] = 0;
                        zobristHash ^= ZOBRIST_QUEENSIDE_CASTLING_KEYS[color];
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
                eligibleEnPassantFile.push(algebraicNotationToBoardIndex(enPassantTarget) % 8);
                zobristHash ^= ZOBRIST_EN_PASSANT_KEYS[eligibleEnPassantFile.top()];
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
                zobristHash ^= ZOBRIST_PEICE_KEYS[peices[i] >> 3][peices[i] % (1 << 3) - 1][i];
            }
        }
    }
    
    /**
     * Construct a new Board object with the starting position
     */
    Board() : Board("rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1") {}


    // METHODS
    /**
     * Generates pseudo-legal moves for the current position
     * @param flags only generate moves with the given flags
     * @return std::vector<Move> of pseudo-legal moves for a position
     */
    std::vector<Move> pseudoLegalMoves(uint8 flags=Move::NONE)
    {
        uint8 color = totalHalfmoves % 2;
        uint8 enemy = !color;

        // Used for optimizing the legality checking of moves
        std::unordered_set<uint8> possiblyPinned(8);
        std::unordered_set<uint8> checkingSquares(16);

        // Check if king in check and record possibly pinned peices and checking squares
        uint8 king = kingIndex[color];
        uint8 checks = 0;

        for (uint8 j = 0; j < KNIGHT_MOVES[king][0]; ++j) {
            if (peices[KNIGHT_MOVES[king][j]] == enemy + KNIGHT) {
                checkingSquares.insert(KNIGHT_MOVES[king][j]);
                ++checks;
            }
        }

        for (uint8 j = king - 8; j < DIRECTION_BOUNDS[king][B]; j -= 8) {
            if (peices[j]) {
                if (peices[j] == enemy + ROOK || enemy + QUEEN) {
                    for (uint8 k = j; k < king; k += 8) {
                        checkingSquares.insert(k);
                    }
                    ++checks;

                } else if (peices[j] >> 3 == color) {
                    possiblyPinned.insert(j);
                }
                break;
            }
        }

        for (uint8 j = king + 8; j < DIRECTION_BOUNDS[king][F]; j += 8) {
            if (peices[j]) {
                if (peices[j] == enemy + ROOK || enemy + QUEEN) {
                    for (uint8 k = j; k > king; k -= 8) {
                        checkingSquares.insert(k);
                    }
                    ++checks;

                } else if (peices[j] >> 3 == color) {
                    possiblyPinned.insert(j);
                }
                break;
            }
        }

        for (uint8 j = king - 1; j < DIRECTION_BOUNDS[king][L]; j -= 1) {
            if (peices[j]) {
                if (peices[j] == enemy + ROOK || enemy + QUEEN) {
                    for (uint8 k = j; k < king; k += 1) {
                        checkingSquares.insert(k);
                    }
                    ++checks;

                } else if (peices[j] >> 3 == color) {
                    possiblyPinned.insert(j);
                }
                break;
            }
        }

        for (uint8 j = king + 1; j < DIRECTION_BOUNDS[king][R]; j += 1) {
            if (peices[j]) {
                if (peices[j] == enemy + ROOK || enemy + QUEEN) {
                    for (uint8 k = j; k > king; k -= 1) {
                        checkingSquares.insert(k);
                    }
                    ++checks;

                } else if (peices[j] >> 3 == color) {
                    possiblyPinned.insert(j);
                }
                break;
            }
        }

        for (uint8 j = king - 9; j < DIRECTION_BOUNDS[king][BL]; j -= 9) {
            if (peices[j]) {
                if (peices[j] == enemy + BISHOP || enemy + QUEEN) {
                    for (uint8 k = j; k < king; k += 9) {
                        checkingSquares.insert(k);
                    }
                    ++checks;

                } else if (peices[j] >> 3 == color) {
                    possiblyPinned.insert(j);
                }
                break;
            }
        }

        for (uint8 j = king + 9; j < DIRECTION_BOUNDS[king][FR]; j += 9) {
            if (peices[j]) {
                if (peices[j] == enemy + BISHOP || enemy + QUEEN) {
                    for (uint8 k = j; k > king; k -= 9) {
                        checkingSquares.insert(k);
                    }
                    ++checks;

                } else if (peices[j] >> 3 == color) {
                    possiblyPinned.insert(j);
                }
                break;
            }
        }

        for (uint8 j = king - 7; j < DIRECTION_BOUNDS[king][BR]; j -= 7) {
            if (peices[j]) {
                if (peices[j] == enemy + BISHOP || enemy + QUEEN) {
                    for (uint8 k = j; k < king; k += 7) {
                        checkingSquares.insert(k);
                    }
                    ++checks;

                } else if (peices[j] >> 3 == color) {
                    possiblyPinned.insert(j);
                }
                break;
            }
        }

        for (uint8 j = king + 7; j < DIRECTION_BOUNDS[king][FL]; j += 7) {
            if (peices[j]) {
                if (peices[j] == enemy + BISHOP || enemy + QUEEN) {
                    for (uint8 k = j; k > king; k -= 7) {
                        checkingSquares.insert(k);
                    }
                    ++checks;

                } else if (peices[j] >> 3 == color) {
                    possiblyPinned.insert(j);
                }
                break;
            }
        }

        // Generate moves based on data gathered
        std::vector<Move> moves;
        
        // Double check; Only king moves are legal
        if (checks > 1) {
            // Generate king moves
            for (uint8 j = 0; j < KING_MOVES[king][0]; ++j) {
                if (!peices[KING_MOVES[king][j]] || peices[KING_MOVES[king][j]] >> 3 == enemy) {
                    moves.emplace_back(this, king, KING_MOVES[king][j], false);
                }
            }
            return moves;
        }

        // Special move generation for when few number of checking squares
        if (checks && checkingSquares.size() < peiceIndices[color].size() * 0.0) {
            // TODO
        }

        // General case
        for (uint8 i : peiceIndices[color]) {
            bool pinned = possiblyPinned.count(i);

            switch (peices[i] % (1 << 3)) {
                case PAWN:
                    uint8 file = i % 8;
                    uint8 t = i + 8 - 16 * color;
                    if (file != 0 && (!peices[t - 1] || peices[t - 1] >> 3 == enemy) && (!checks || checkingSquares.count(t - 1))) {
                        moves.emplace_back(i, t, this, !pinned);
                    }
                    if (file != 7 && (!peices[t + 1] || peices[t + 1] >> 3 == enemy) && (!checks || checkingSquares.count(t + 1))) {
                        moves.emplace_back(i, t, this, !pinned);
                    }
                    break;

                case KNIGHT:
                    uint8 t;
                    for (uint8 j = 1; j < KNIGHT_MOVES[i][0]; ++j) {
                        t = KNIGHT_MOVES[i][j];
                        if ((!peices[t] || peices[t] >> 3 == enemy) && (!checks || checkingSquares.count(t))) {
                            moves.emplace_back(i, t, this, !pinned);
                        }
                    }
                    break;

                case BISHOP:
                case ROOK:
                case QUEEN:
                    if (peices[i] % (1 << 3) == BISHOP) {
                        goto bishopMoves;
                    }

                    for (uint8 t = i - 8; t < DIRECTION_BOUNDS[i][B]; t -= 8) {
                        if ((!peices[t] || peices[t] >> 3 == enemy) && (!checks || checkingSquares.count(t))) {
                            moves.emplace_back(i, t, this, !pinned);
                        }
                        if (peices[t]) {
                            break;
                        }
                    }

                    for (uint8 t = i + 8; t < DIRECTION_BOUNDS[i][F]; t += 8) {
                        if ((!peices[t] || peices[t] >> 3 == enemy) && (!checks || checkingSquares.count(t))) {
                            moves.emplace_back(i, t, this, !pinned);
                        }
                        if (peices[t]) {
                            break;
                        }
                    }

                    for (uint8 t = i - 1; t < DIRECTION_BOUNDS[i][L]; t -= 1) {
                        if ((!peices[t] || peices[t] >> 3 == enemy) && (!checks || checkingSquares.count(t))) {
                            moves.emplace_back(i, t, this, !pinned);
                        }
                        if (peices[t]) {
                            break;
                        }
                    }

                    for (uint8 t = i + 1; t < DIRECTION_BOUNDS[i][R]; t += 1) {
                        if ((!peices[t] || peices[t] >> 3 == enemy) && (!checks || checkingSquares.count(t))) {
                            moves.emplace_back(i, t, this, !pinned);
                        }
                        if (peices[t]) {
                            break;
                        }
                    }

                    if (peices[i] % (1 << 3) == ROOK) {
                        break;
                    }
                    bishopMoves:

                    for (uint8 t = i - 9; t < DIRECTION_BOUNDS[i][BL]; t -= 9) {
                        if ((!peices[t] || peices[t] >> 3 == enemy) && (!checks || checkingSquares.count(t))) {
                            moves.emplace_back(i, t, this, !pinned);
                        }
                        if (peices[t]) {
                            break;
                        }
                    }

                    for (uint8 t = i + 9; t < DIRECTION_BOUNDS[i][FR]; t += 9) {
                        if ((!peices[t] || peices[t] >> 3 == enemy) && (!checks || checkingSquares.count(t))) {
                            moves.emplace_back(i, t, this, !pinned);
                        }
                        if (peices[t]) {
                            break;
                        }
                    }

                    for (uint8 t = i - 7; t < DIRECTION_BOUNDS[i][BR]; t -= 7) {
                        if ((!peices[t] || peices[t] >> 3 == enemy) && (!checks || checkingSquares.count(t))) {
                            moves.emplace_back(i, t, this, !pinned);
                        }
                        if (peices[t]) {
                            break;
                        }
                    }

                    for (uint8 t = i + 7; t < DIRECTION_BOUNDS[i][FL]; t += 7) {
                        if ((!peices[t] || peices[t] >> 3 == enemy) && (!checks || checkingSquares.count(t))) {
                            moves.emplace_back(i, t, this, !pinned);
                        }
                        if (peices[t]) {
                            break;
                        }
                    }
                    
                    break;
                
                case KING:
                    uint8 t;
                    for (uint8 j = 1; j < KING_MOVES[i][0]; ++j) {
                        t = KING_MOVES[i][j];
                        if (!peices[t] || peices[t] >> 3 == enemy) {
                            moves.emplace_back(i, t, this, false);
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
            zobristHash ^= ZOBRIST_EN_PASSANT_KEYS[eligibleEnPassantFile.top()];
        }
        if (move.movingPeice == PAWN && std::abs(move.targetSquare - move.startSquare) == 16) {
            zobristHash ^= ZOBRIST_EN_PASSANT_KEYS[move.startSquare % 8];
            eligibleEnPassantFile.push(move.startSquare % 8);
        
        } else {
            eligibleEnPassantFile.push(-1);
        }

        // Update zobrist hash for turn change
        zobristHash ^= ZOBRIST_TURN_KEY;
        
        // Update zobrist hash for moving peice
        if (move.flags & Move::PROMOTION) {
            zobristHash ^= ZOBRIST_PEICE_KEYS[color][PAWN][move.startSquare];
            zobristHash ^= ZOBRIST_PEICE_KEYS[color][moving][move.targetSquare];
            moving = color << 3 + PAWN;
        
        } else { 
            zobristHash ^= ZOBRIST_PEICE_KEYS[color][moving][move.startSquare];
            zobristHash ^= ZOBRIST_PEICE_KEYS[color][moving][move.targetSquare];
        }

        // Update zobrist hash and peice indices set for capture
        if (move.capturedPeice) {
            zobristHash ^= ZOBRIST_PEICE_KEYS[enemy][move.capturedPeice % (1 << 3)][move.targetSquare];
            peiceIndices[enemy].erase(move.targetSquare);
        }

        // increment counters
        ++totalHalfmoves;
        if (move.capturedPeice || moving == color + PAWN) {
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
                zobristHash ^= ZOBRIST_PEICE_KEYS[color][ROOK][castlingRank + 3];
                zobristHash ^= ZOBRIST_PEICE_KEYS[color][ROOK][castlingRank];
                peiceIndices[color].erase(castlingRank);
                peiceIndices[color].insert(castlingRank + 3);
                
                queensideCastlingRightsLost[color] = totalHalfmoves;
                zobristHash ^= ZOBRIST_QUEENSIDE_CASTLING_KEYS[color];
                
            } else {
                // Kingside castling
                peices[castlingRank + 5] = peices[castlingRank + 7];
                peices[castlingRank + 7] = 0;
                zobristHash ^= ZOBRIST_PEICE_KEYS[color][ROOK][castlingRank + 5];
                zobristHash ^= ZOBRIST_PEICE_KEYS[color][ROOK][castlingRank + 7];
                peiceIndices[color].erase(castlingRank + 7);
                peiceIndices[color].insert(castlingRank + 5);
                
                kingsideCastlingRightsLost[color] = totalHalfmoves;
                zobristHash ^= ZOBRIST_KINGSIDE_CASTLING_KEYS[color];
            }
        }

        // update castling rights
        if (!kingsideCastlingRightsLost[color]) {
            if (moving == KING || (moving == ROOK && move.startSquare % 8 == 7)) {
                kingsideCastlingRightsLost[color] = totalHalfmoves;
                zobristHash ^= ZOBRIST_KINGSIDE_CASTLING_KEYS[color];
            }
        }
        if (!queensideCastlingRightsLost[color]) {
            if (moving == KING || (moving == ROOK && move.startSquare % 8 == 0)) {
                kingsideCastlingRightsLost[color] = totalHalfmoves;
                zobristHash ^= ZOBRIST_QUEENSIDE_CASTLING_KEYS[color];
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
            zobristHash ^= ZOBRIST_EN_PASSANT_KEYS[eligibleEnPassantFile.top()];
        }
        eligibleEnPassantFile.pop();
        if (eligibleEnPassantFile.top() >= 0) {
            zobristHash ^= ZOBRIST_EN_PASSANT_KEYS[eligibleEnPassantFile.top()];
        }

        // Undo zobrist hash for turn change
        zobristHash ^= ZOBRIST_TURN_KEY;
        
        // Undo zobrist hash for moving peice
        if (move.flags & Move::PROMOTION) {
            zobristHash ^= ZOBRIST_PEICE_KEYS[color][PAWN][move.startSquare];
            zobristHash ^= ZOBRIST_PEICE_KEYS[color][moving][move.targetSquare];
            moving = color << 3 + PAWN;
        
        } else { 
            zobristHash ^= ZOBRIST_PEICE_KEYS[color][moving][move.startSquare];
            zobristHash ^= ZOBRIST_PEICE_KEYS[color][moving][move.targetSquare];
        }

        // undo zobrist hash and peice indices set for capture
        if (move.capturedPeice) {
            zobristHash ^= ZOBRIST_PEICE_KEYS[enemy][move.capturedPeice % (1 << 3)][move.targetSquare];
            peiceIndices[enemy].insert(move.targetSquare);
        }

        // decrement counters
        --totalHalfmoves;
        if (move.capturedPeice || moving == color + PAWN) {
            halfmovesSincePawnMoveOrCapture.pop();
        } else {
            uint8 prev = halfmovesSincePawnMoveOrCapture.top();
            halfmovesSincePawnMoveOrCapture.pop();
            halfmovesSincePawnMoveOrCapture.push(prev - 1);
        }

        // undo peices array
        peices[move.startSquare] = moving;
        peices[move.targetSquare] = move.capturedPeice;
        
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
                zobristHash ^= ZOBRIST_PEICE_KEYS[color][ROOK][castlingRank + 3];
                zobristHash ^= ZOBRIST_PEICE_KEYS[color][ROOK][castlingRank];
                peiceIndices[color].erase(castlingRank + 3);
                peiceIndices[color].insert(castlingRank);
                
                queensideCastlingRightsLost[color] = 0;
                zobristHash ^= ZOBRIST_QUEENSIDE_CASTLING_KEYS[color];
                
            } else {
                // Kingside castling
                peices[castlingRank + 7] = peices[castlingRank + 5];
                peices[castlingRank + 5] = 0;
                zobristHash ^= ZOBRIST_PEICE_KEYS[color][ROOK][castlingRank + 5];
                zobristHash ^= ZOBRIST_PEICE_KEYS[color][ROOK][castlingRank + 7];
                peiceIndices[color].erase(castlingRank + 5);
                peiceIndices[color].insert(castlingRank + 7);
                
                kingsideCastlingRightsLost[color] = 0;
                zobristHash ^= ZOBRIST_KINGSIDE_CASTLING_KEYS[color];
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
     * @param algebraic notation for position on chess board (ex e3, a1, c8)
     * @return uint8 index [0, 63] -> [a1, h8] of square on board
     */
    static uint8 algebraicNotationToBoardIndex(const std::string &algebraic)
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