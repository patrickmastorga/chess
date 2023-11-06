#include <cstdint>
#include <vector>
#include <algorithm>
#include <stdexcept>
#include <string>
#include <sstream>
#include <cctype>

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
 * @brief Value of every peice (index is peice id) at every index [0, 63] -> [a1, h8]
 * Used for evaluating a position
 */
const int16 standardPeiceEvaluationValue[15][64];

/**
 * @brief Value of every peice (index is peice id) at every index [0, 63] -> [a1, h8]
 * Used for evaluating a position
 */
const int16 endgamePeiceEvaluationValue[15][64];

/**
 * @brief struct for containing info about the current state of the chess game
 */
struct Board
{
    // FLAGS

    static constexpr uint8 PINNED = 0b001;

    // DATA

    /**
     * @brief color and peice type at every square (index [0, 63] -> [a1, h8])
     */
    uint8 peices[64];

    /**
     * @brief the number of attacks and pawn attacks from white and black (index 0 and 1) at every square (index [0, 63] -> [a1, h8])
     * |2 bits pawn attacks|6 bits total attacks|
     */
    uint8 attacks[2][64];

    /**
     * @brief flag at every square (index [0, 63] -> [a1, h8]) for white and black (index 0 and 1)
     */
    uint8 flags[2][64];

    /**
     * @brief zobrist hash of the current position
     */
    uint64 zobristHash;

    /**
     * @brief total half moves since game start (half move is one player taking a turn)
     */
    uint16 totalHalfmoves;

    /**
     * @brief whether or not the white or black (index 0 and 1) king is in check
     */
    bool inCheck[2];

    /**
     * @brief whether or not the black/white king is in a double check
     */
    bool doubleCheck;

    /**
     * @brief whether or not kingside castling is still allowed for white or black (index 0 and 1)
     */
    bool kingsideCastlingAvailable[2];

    /**
     * @brief whether or not queenside castling is still allowed for white or black (index 0 and 1)
     */
    bool queensideCastlingAvailable[2];

    /**
     * @brief index of square over which a pawn has just moves two squares over
     */
    uint8 eligibleForEnPassant;

    /**
     * @brief number of half moves since pawn move or capture (half move is one player taking a turn) (used for 50 move rule)
     */
    uint8 halfmovesSincePawnMoveOrCapture;

    // CONSTRUCTORS

    /**
     * @brief Construct a new Board object from fen string
     * @param fenString string in Forsyth–Edwards Notation
     */
    Board(const std::string &fenString)
    {
        // INITIALIZE GAME DATA
        // peice data, castling data, en-passant, move clocks

        std::istringstream fenStringStream(fenString);
        std::string peicePlacementData, activeColor, castlingAvailabilty, enPassantTarget, halfmoveClock, fullmoveNumber;
        
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
        for (char c : peicePlacementData) {
            if (c != '-' && !std::isalpha(c)) {
                throw std::invalid_argument("Unrecognised charecter in FEN castling availability");
            }

            uint8 color = std::islower(c);
            switch (c) {
                case 'K':
                case 'k':
                    kingsideCastlingAvailable[color] = true;
                    break;
                case 'Q':
                case 'q':
                    queensideCastlingAvailable[color] = true;
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
                enPassantTarget = algebraicNotationToBoardIndex(enPassantTarget);
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
        // checks, attacks, flags


        // INITIALIZE ZOBRIST HASH
    }
    
    /**
     * @brief Construct a new Board object with the starting position
     */
    Board() : Board("rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1") {}

    // METHODS

    /**
     * @brief update the board based on the inputted move
     */
    void makeMove(const Move &move)
    {
        // TODO make move method
    }

    /**
     * @brief update the board based on the inputted move
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
            throw std::invalid_argument("Algebraic notation should be in the form [a-g][1-8]!");
        }

        return (rank - '1') * 8 + (file - 'a');
    }
};


/**
 * @brief struct for containing info about a move
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
    static constexpr uint8 NONE       = 0b000;
    static constexpr uint8 CAPTURE    = 0b001;
    static constexpr uint8 PROMOTION  = 0b010;
    static constexpr uint8 EN_PASSANT = 0b100;
};

/**
 * @brief Generates all legal moves for a goven position and a given color
 * @param flags only generate moves with the given flags
 * @return std::vector<Move> of legal moves for a position
 */
static std::vector<Move> generateLegalMoves(const Board &board, uint8 flags=Move::NONE)
{
    // TODO generate legal moves
}

} // end of computer namespace