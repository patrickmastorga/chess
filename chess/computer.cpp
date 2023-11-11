#include <cstdint>
#include <vector>
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

namespace computer
{
constexpr uint8 WHITE = 0b0000;
constexpr uint8 BLACK = 0b1000;
constexpr uint8 PAWN =   0b001;
constexpr uint8 KNIGHT = 0b010;
constexpr uint8 BISHOP = 0b011;
constexpr uint8 ROOK =   0b100;
constexpr uint8 QUEEN =  0b101;
constexpr uint8 KING =   0b110;

/**
 * struct for containing info about the current state of the chess game
 */
struct Board
{
    // CHESS DATA

    /**
     * color and peice type at every square (index [0, 63] -> [a1, h8])
     */
    uint8 peices[64];

    /**
     * whether or not kingside castling is still allowed for white or black (index 0 and 1)
     */
    bool kingsideCastlingAvailable[2];

    /**
     * whether or not queenside castling is still allowed for white or black (index 0 and 1)
     */
    bool queensideCastlingAvailable[2];

    /**
     * file where a pawn has just moves two squares over
     */
    int8 eligibleEnPassantFile;

    /**
     * number of half moves since pawn move or capture (half move is one player taking a turn) (used for 50 move rule)
     */
    uint8 halfmovesSincePawnMoveOrCapture;

    // COMPUTER DATA

    /**
     * the number of attacks and pawn attacks from white and black (index 0 and 1) at every square (index [0, 63] -> [a1, h8])
     * |2 bits pawn attacks|6 bits total attacks|
     */
    uint8 attacks[2][64];

    /**
     * flag at every square (index [0, 63] -> [a1, h8]) for white and black (index 0 and 1)
     */
    uint8 flags[2][64];

    // flags
    static constexpr uint8 PINNED =   0b001;
    static constexpr uint8 CHECKING = 0b010;

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

    // CONSTRUCTORS

    /**
     * Construct a new Board object from fen string
     * @param fenString string in Forsyth–Edwards Notation
     */
    Board(const std::string &fenString)
    {
        // INITIALIZE GAME DATA
        // peice data, castling data, en-passant, move clocks, zobrist hash

        std::istringstream fenStringStream(fenString);
        std::string peicePlacementData, activeColor, castlingAvailabilty, enPassantTarget, halfmoveClock, fullmoveNumber;

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
        kingsideCastlingAvailable[0] = false;
        kingsideCastlingAvailable[1] = false;
        queensideCastlingAvailable[0] = false;
        queensideCastlingAvailable[1] = false;

        for (char c : castlingAvailabilty) {
            if (c != '-' && !std::isalpha(c)) {
                throw std::invalid_argument("Unrecognised charecter in FEN castling availability");
            }

            uint8 color = std::islower(c);
            switch (c) {
                case 'K':
                case 'k':
                    kingsideCastlingAvailable[color] = true;
                    zobristHash ^= ZOBRIST_KINGSIDE_CASTLING_KEYS[color];
                    break;
                case 'Q':
                case 'q':
                    queensideCastlingAvailable[color] = true;
                    zobristHash ^= ZOBRIST_QUEENSIDE_CASTLING_KEYS[color];
                    break;
                default:
                    throw std::invalid_argument("Unrecognised alpha char in FEN castling availability data!");
            }
        }

        // Get en passant target data from fen string
        if (!std::getline(fenStringStream, enPassantTarget, ' ')) {
            throw std::invalid_argument("Cannot get en passant target from FEN!");
        }

        if (enPassantTarget != "-") {
            try {
                eligibleEnPassantFile = algebraicNotationToBoardIndex(enPassantTarget) % 8;
                zobristHash ^= ZOBRIST_EN_PASSANT_KEYS[eligibleEnPassantFile];
            } catch (const std::invalid_argument &e) {
                throw std::invalid_argument(std::string("Invalid FEN en passant target! ") + e.what());
            }
        }

        // Get half move clock data from fen string
        if (!std::getline(fenStringStream, halfmoveClock, ' ')) {
            throw std::invalid_argument("Cannot get halfmove clock from FEN!");
        }

        try {
            halfmovesSincePawnMoveOrCapture = static_cast<uint8>(std::stoi(halfmoveClock));
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

        // INITIALIZE COMPUTER FLAGS
        // attacks, flags, zobrist hash
        
        for (uint8 i = 0; i < 64; ++i) {
            uint8 color = peices[i] / (1 << 3);

            zobristHash ^= ZOBRIST_PEICE_KEYS[color][peices[i] % (1 << 3)][i];

            switch (peices[i] % (1 << 3)) {
                case PAWN:
                    uint8 dir = 8 - 16 * color;
                    if (i % 8 != 0) {
                        ++attacks[color][i + dir - 1];
                    }
                    if (i % 8 != 7) {
                        ++attacks[color][i + dir + 1];
                    }
                    break;

                case KNIGHT:
                    for (uint8 j = 1; j < KNIGHT_MOVES[i][0]; ++j) {
                        ++attacks[color][KNIGHT_MOVES[i][j]];
                    }
                    break;

                case BISHOP:
                    uint8 file = i % 8;
                    uint8 rank = i / 8;
                    uint8 ifile = 7 - file;
                    uint8 irank = 7 - rank;
                    uint8 bl = std::min(rank, file);
                    uint8 br = std::min(rank, ifile);
                    uint8 fl = std::min(irank, file);
                    uint8 fr = std::min(irank, ifile);

                    uint8 target = i;
                    for (int j = 0; j < bl; ++j) {
                        target -= 9;
                        ++attacks[color][target];
                        if (peices[target]) {
                            break;
                        }
                    }
                    target = i;
                    for (int j = 0; j < br; ++j) {
                        target -= 7;
                        ++attacks[color][target];
                        if (peices[target]) {
                            break;
                        }
                    }
                    target = i;
                    for (int j = 0; j < fl; ++j) {
                        target += 7;
                        ++attacks[color][target];
                        if (peices[target]) {
                            break;
                        }
                    }
                    target = i;
                    for (int j = 0; j < fr; ++j) {
                        target += 9;
                        ++attacks[color][target];
                        if (peices[target]) {
                            break;
                        }
                    }
                    break;

                case ROOK:
                    uint8 file = i % 8;
                    uint8 rank = i / 8;
                    uint8 ifile = 7 - file;
                    uint8 irank = 7 - rank;

                    uint8 target = i;
                    for (int j = 0; j < rank; ++j) {
                        target -= 8;
                        ++attacks[color][target];
                        if (peices[target]) {
                            break;
                        }
                    }
                    target = i;
                    for (int j = 0; j < file; ++j) {
                        target -= 1;
                        ++attacks[color][target];
                        if (peices[target]) {
                            break;
                        }
                    }
                    target = i;
                    for (int j = 0; j < irank; ++j) {
                        target += 8;
                        ++attacks[color][target];
                        if (peices[target]) {
                            break;
                        }
                    }
                    target = i;
                    for (int j = 0; j < ifile; ++j) {
                        target += 1;
                        ++attacks[color][target];
                        if (peices[target]) {
                            break;
                        }
                    }
                    break;
                
                case QUEEN:
                    uint8 file = i % 8;
                    uint8 rank = i / 8;
                    uint8 ifile = 7 - file;
                    uint8 irank = 7 - rank;
                    uint8 bl = std::min(rank, file);
                    uint8 br = std::min(rank, ifile);
                    uint8 fl = std::min(irank, file);
                    uint8 fr = std::min(irank, ifile);

                    uint8 target = i;
                    for (int j = 0; j < bl; ++j) {
                        target -= 9;
                        ++attacks[color][target];
                        if (peices[target]) {
                            break;
                        }
                    }
                    target = i;
                    for (int j = 0; j < br; ++j) {
                        target -= 7;
                        ++attacks[color][target];
                        if (peices[target]) {
                            break;
                        }
                    }
                    target = i;
                    for (int j = 0; j < fl; ++j) {
                        target += 7;
                        ++attacks[color][target];
                        if (peices[target]) {
                            break;
                        }
                    }
                    target = i;
                    for (int j = 0; j < fr; ++j) {
                        target += 9;
                        ++attacks[color][target];
                        if (peices[target]) {
                            break;
                        }
                    }
                    target = i;
                    for (int j = 0; j < rank; ++j) {
                        target -= 8;
                        ++attacks[color][target];
                        if (peices[target]) {
                            break;
                        }
                    }
                    target = i;
                    for (int j = 0; j < file; ++j) {
                        target -= 1;
                        ++attacks[color][target];
                        if (peices[target]) {
                            break;
                        }
                    }
                    target = i;
                    for (int j = 0; j < irank; ++j) {
                        target += 8;
                        ++attacks[color][target];
                        if (peices[target]) {
                            break;
                        }
                    }
                    target = i;
                    for (int j = 0; j < ifile; ++j) {
                        target += 1;
                        ++attacks[color][target];
                        if (peices[target]) {
                            break;
                        }
                    }
                    break;
                
                case KING:
                    uint8 enemy = !color;
                    uint8 file = i % 8;
                    uint8 rank = i / 8;
                    uint8 ifile = 7 - file;
                    uint8 irank = 7 - rank;
                    uint8 bl = std::min(rank, file);
                    uint8 br = std::min(rank, ifile);
                    uint8 fl = std::min(irank, file);
                    uint8 fr = std::min(irank, ifile);

                    if (file) {
                        ++attacks[color][i - 1];
                    }
                    if (ifile) {
                        ++attacks[color][i + 1];
                    }
                    if (rank) {
                        ++attacks[color][i - 8];
                    }
                    if (irank) {
                        ++attacks[color][i + 8];
                    }
                    if (bl) {
                        ++attacks[color][i - 9];
                    }
                    if (br) {
                        ++attacks[color][i - 7];
                    }
                    if (fl) {
                        ++attacks[color][i + 7];
                    }
                    if (fr) {
                        ++attacks[color][i + 9];
                    }

                    kingIndex[color] = i;

                    // initialize pin + checking flags

                    for (int j = 1; j < KNIGHT_MOVES[i][0]; ++j) {
                        if (peices[KNIGHT_MOVES[i][j]] == enemy + KNIGHT) {
                            flags[color][KNIGHT_MOVES[i][j]] |= CHECKING;
                        }
                    }

                    uint8 target = i;
                    uint8 blocked = 0;
                    for (int j = 0; j < bl && blocked < 2; ++j) {
                        target -= 9;
                        if (peices[target]) {
                            if (peices[target] == enemy + BISHOP || peices[target] == enemy + QUEEN) {
                                for (int k = i - 9; k <= target; k -= 9) {
                                    flags[color][k] |= blocked ? PINNED : CHECKING;
                                }
                            }
                            ++blocked;
                        }
                    }
                    target = i;
                    blocked = 0;
                    for (int j = 0; j < br && blocked < 2; ++j) {
                        target -= 7;
                        if (peices[target]) {
                            if (peices[target] == enemy + BISHOP || peices[target] == enemy + QUEEN) {
                                for (int k = i - 7; k <= target; k -= 7) {
                                    flags[color][k] |= blocked ? PINNED : CHECKING;
                                }
                            }
                            ++blocked;
                        }
                    }
                    target = i;
                    blocked = 0;
                    for (int j = 0; j < fl && blocked < 2; ++j) {
                        target += 7;
                        if (peices[target]) {
                            if (peices[target] == enemy + BISHOP || peices[target] == enemy + QUEEN) {
                                for (int k = i + 7; k <= target; k += 7) {
                                    flags[color][k] |= blocked ? PINNED : CHECKING;
                                }
                            }
                            ++blocked;
                        }
                    }
                    target = i;
                    blocked = 0;
                    for (int j = 0; j < fr && blocked < 2; ++j) {
                        target += 9;
                        if (peices[target]) {
                            if (peices[target] == enemy + BISHOP || peices[target] == enemy + QUEEN) {
                                for (int k = i + 9; k <= target; k += 9) {
                                    flags[color][k] |= blocked ? PINNED : CHECKING;
                                }
                            }
                            ++blocked;
                        }
                    }
                    target = i;
                    blocked = 0;
                    for (int j = 0; j < rank && blocked < 2; ++j) {
                        target -= 8;
                        if (peices[target]) {
                            if (peices[target] == enemy + ROOK || peices[target] == enemy + QUEEN) {
                                for (int k = i - 8; k <= target; k -= 8) {
                                    flags[color][k] |= blocked ? PINNED : CHECKING;
                                }
                            }
                            ++blocked;
                        }
                    }
                    target = i;
                    blocked = 0;
                    for (int j = 0; j < file && blocked < 2; ++j) {
                        target -= 1;
                        if (peices[target]) {
                            if (peices[target] == enemy + ROOK || peices[target] == enemy + QUEEN) {
                                for (int k = i - 1; k <= target; k -= 1) {
                                    flags[color][k] |= blocked ? PINNED : CHECKING;
                                }
                            }
                            ++blocked;
                        }
                    }
                    target = i;
                    blocked = 0;
                    for (int j = 0; j < irank && blocked < 2; ++j) {
                        target += 8;
                        if (peices[target]) {
                            if (peices[target] == enemy + ROOK || peices[target] == enemy + QUEEN) {
                                for (int k = i + 8; k <= target; k += 8) {
                                    flags[color][k] |= blocked ? PINNED : CHECKING;
                                }
                            }
                            ++blocked;
                        }
                    }
                    target = i;
                    blocked = 0;
                    for (int j = 0; j < ifile && blocked < 2; ++j) {
                        target += 1;
                        if (peices[target]) {
                            if (peices[target] == enemy + ROOK || peices[target] == enemy + QUEEN) {
                                for (int k = i + 1; k <= target; k += 1) {
                                    flags[color][k] |= blocked ? PINNED : CHECKING;
                                }
                            }
                            ++blocked;
                        }
                    }
            }
        }
    }
    
    /**
     * Construct a new Board object with the starting position
     */
    Board() : Board("rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1") {}

    // METHODS

    /**
     * update the board based on the inputted move
     */
    void makeMove(const Move &move)
    {
        // TODO make move method
    }

    /**
     * update the board based on the inputted move
     */
    void unmakeMove(const Move &move)
    {
        // TODO unmake move method
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


/**
 * struct for containing info about a move
 */
struct Move
{
    uint8 startSquare;

    uint8 targetSquare;

    uint8 movingPeice;

    uint8 capturedPeice;

    uint8 flags;

    int16 moveStrengthGuess;

    Move(uint8 start, uint8 target, Board board)
    {
        // TODO: move constructor
    }

    // FLAGS
    static constexpr uint8 NONE       = 0b0000;
    static constexpr uint8 CAPTURE    = 0b0001;
    static constexpr uint8 PROMOTION  = 0b0010;
    static constexpr uint8 CHECK  =     0b0100;
    static constexpr uint8 EN_PASSANT = 0b1000;
};

/**
 * Generates all legal moves for a goven position and a given color
 * @param flags only generate moves with the given flags
 * @return std::vector<Move> of legal moves for a position
 */
static std::vector<Move> generateLegalMoves(const Board &board, uint8 flags=Move::NONE)
{
    // TODO generate legal moves
}

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

} // end of computer namespace