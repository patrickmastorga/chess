#ifndef BOARD_H
#define BOARD_H

#include <cstdint>
#include <vector>
#include <stack>
#include <forward_list>
#include <unordered_set>
#include <algorithm>
#include <stdexcept>
#include <string>
#include <sstream>
#include <cctype>

#include "precomputed.hpp"
#include "../chess.hpp"

#define MAX_GAME_LENGTH 500

// TODO add some sort of protection for games with halfmove counter > 500

typedef std::int_fast16_t _int;
typedef std::uint_fast32_t uint32;
typedef std::uint_fast64_t uint64;


// class representing the current state of the chess game
class Board
{
public:
    // MOVE STRUCT
    // struct for containing info about a move
    class Move
    {   
    public:
        // Starting square of the move [0, 63] -> [a1, h8]
        inline _int start() const noexcept
        {
            return startSquare;
        }
        
        // Ending square of the move [0, 63] -> [a1, h8]
        inline _int target() const noexcept
        {
            return targetSquare;
        }

        // Peice and color of moving peice (peice that is on the starting square of the move
        inline _int moving() const noexcept
        {
            return movingPeice;
        }

        // Peice and color of captured peice
        inline _int captured() const noexcept
        {
            return capturedPeice;
        }

        // Returns the color of the whose move is
        inline _int color() const noexcept
        {
            return (movingPeice >> 3) << 3;
        }

        // Returs the color who the move is being played against
        inline _int enemy() const noexcept
        {
            return !(movingPeice >> 3) << 3;
        }

        // In case of promotion, returns the peice value of the promoted peice
        inline _int promotion() const noexcept
        {
            return flags & PROMOTION;
        }

        // Returns true if move is castling move
        inline bool isEnPassant() const noexcept
        {
            return flags & EN_PASSANT;
        }

        // Returns true if move is en passant move
        inline bool isCastling() const noexcept
        {
            return flags & CASTLE;
        }
        
        // Returns true of move is guarrenteed to be legal in the posiiton it was generated in
        inline bool legalFlagSet() const noexcept
        {
            return flags & LEGAL;
        }

        // Sets the legal flag of the move
        inline void setLegalFlag() noexcept
        {
            flags |= LEGAL;
        }

        inline _int earlygamePositionalMaterialChange() noexcept
        {
            if (!posmatInit) {
                initializePosmat();
            }
            return earlyPosmat;
        }

        inline _int endgamePositionalMaterialChange() noexcept
        {
            if (!posmatInit) {
                initializePosmat();
            }
            return endPosmat;
        }
        
        // Heuristic geuss for the strength of the move (used for move ordering)
        _int strengthGuess; 

        // Override equality operator with other move
        inline bool operator==(const Move& other) const
        {
            return this->start() == other.start()
                && this->target() == other.target()
                && this->flags & PROMOTION == other.flags & PROMOTION;
        }

        // Override equality operator with standard move
        inline bool operator==(const StandardMove& other) const
        {
            return this->start() == other.startSquare
                && this->target() == other.targetSquare
                && std::max(0L, this->flags & PROMOTION - KNIGHT + 1) == other.promotion;
        }

        // Override stream insertion operator to display info about the move
        friend std::ostream& operator<<(std::ostream& os, const Move &obj)
        {
            os << ChessHelpers::boardIndexToAlgebraicNotation(obj.start()) << ChessHelpers::boardIndexToAlgebraicNotation(obj.target());
            if (obj.promotion()) {
                char values[4] = {'n', 'b', 'r', 'q'};
                os << values[obj.promotion() - 2];
            }
            return os;
        }

        // CONSTRUCTORS
        // Construct a new Move object from the given board and given flags (en_passant, castle, promotion, etc.)
        Move(const Board *board, _int start, _int target, _int givenFlags) : startSquare(start), targetSquare(target), flags(givenFlags), strengthGuess(0), posmatInit(false), earlyPosmat(0), endPosmat(0)
        {
            movingPeice = board->peices[start];
            capturedPeice = board->peices[target];
            if (isEnPassant()) {
                capturedPeice = enemy() + PAWN;
            }
        }

        Move() : startSquare(0), targetSquare(0), movingPeice(0), capturedPeice(0), flags(0), strengthGuess(0), posmatInit(false), earlyPosmat(0), endPosmat(0) {}
        
        // FLAGS
        static constexpr _int NONE       = 0b00000000;
        static constexpr _int PROMOTION  = 0b00000111;
        static constexpr _int LEGAL      = 0b00001000;
        static constexpr _int EN_PASSANT = 0b00010000;
        static constexpr _int CASTLE     = 0b00100000;
    private:
        // Starting square of the move [0, 63] -> [a1, h8]
        _int startSquare;
        
        // Ending square of the move [0, 63] -> [a1, h8]
        _int targetSquare;

        // Peice and color of moving peice (peice that is on the starting square of the move
        _int movingPeice;

        // Peice and color of captured peice
        _int capturedPeice;

        // 8 bit flags of move
        _int flags;

        bool posmatInit;

        _int earlyPosmat;
        _int endPosmat;

        void initializePosmat() noexcept
        {
            // Update zobrist hash, numpieces and positonal imbalance for moving peice
            earlyPosmat -= EARLYGAME_PEICE_VALUE[moving()][start()];
            endPosmat -= ENDGAME_PEICE_VALUE[moving()][start()];
            if (promotion()) {
                earlyPosmat += EARLYGAME_PEICE_VALUE[color() + promotion()][target()];
                endPosmat += ENDGAME_PEICE_VALUE[color() + promotion()][target()];
            } else {
                earlyPosmat += EARLYGAME_PEICE_VALUE[moving()][target()];
                endPosmat += ENDGAME_PEICE_VALUE[moving()][target()];
            }
            

            // Update zobrist hash and peice indices set for capture
            if (captured()) {
                _int captureSquare = isEnPassant() ? target() - 8 + 16 * (color() >> 3) : target();
                earlyPosmat -= EARLYGAME_PEICE_VALUE[captured()][captureSquare];
                endPosmat -= ENDGAME_PEICE_VALUE[captured()][captureSquare];
            }

            // Update rooks for castling
            if (isCastling()) {
                _int rookStart;
                _int rookEnd;

                _int castlingRank = target() & 0b11111000;
                if (target() % 8 < 4) {
                    // Queenside castling
                    rookStart = castlingRank;
                    rookEnd = castlingRank + 3;
                } else {
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
    };


    // CONSTRUCTORS
    // Construct a new Board object from fen string
    Board(const std::string &fenString)
    {
        std::istringstream fenStringStream(fenString);
        std::string peicePlacementData, activeColor, castlingAvailabilty, enPassantTarget, halfmoveClock, fullmoveNumber;
        
        for (int i = 0; i < 15; ++i) {
            numPeices[i] = 0;
        }
        numTotalPeices[0] = 0;
        numTotalPeices[1] = 0;
        material_stage_weight = 0;
        earlygamePositionalMaterialInbalance = 0;
        endgamePositionalMaterialInbalance = 0;
       
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
        kingsideCastlingRightsLost[0] = -1;
        kingsideCastlingRightsLost[1] = -1;
        queensideCastlingRightsLost[0] = -1;
        queensideCastlingRightsLost[1] = -1;

        if (castlingAvailabilty != "-") {
            for (char castlingInfo : castlingAvailabilty) {
                //_int color = std::islower(c);
                _int c = castlingInfo > 96 && castlingInfo < 123;
                _int color = c << 3;
                _int castlingRank = 56 * c;
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
            halfmovesSincePawnMoveOrCapture[hmspmocIndex++] = static_cast<_int>(std::stoi(halfmoveClock));
        } catch (const std::invalid_argument &e) {
            throw std::invalid_argument(std::string("Invalid FEN half move clock! ") + e.what());
        }

        // Get full move number data from fen string
        if (!std::getline(fenStringStream, fullmoveNumber, ' ')) {
            fullmoveNumber = "1";
            //throw std::invalid_argument("Cannot getfullmove number from FEN!");
        }

        try {
            totalHalfmoves += static_cast<_int>(std::stoi(fullmoveNumber) * 2 - 2);
        } catch (const std::invalid_argument &e) {
            throw std::invalid_argument(std::string("Invalid FEN full move number! ") + e.what());
        }

        // wait until total halfmoves are defined before defining
        for (int i = 0; i < MAX_GAME_LENGTH; ++i) {
            eligibleEnPassantSquare[i] = -1;
        }
        if (enPassantTarget != "-") {
            try {
                eligibleEnPassantSquare[totalHalfmoves] = ChessHelpers::algebraicNotationToBoardIndex(enPassantTarget);
            } catch (const std::invalid_argument &e) {
                throw std::invalid_argument(std::string("Invalid FEN en passant target! ") + e.what());
            }
        
        } else {
            eligibleEnPassantSquare[totalHalfmoves] = -1;
        }

        // initialize zobrist hash for all of the peices
        for (_int i = 0; i < 64; ++i) {
            _int peice = peices[i];
            if (peice) {
                zobrist ^= ZOBRIST_PEICE_KEYS[peice >> 3][peice % (1 << 3) - 1][i];
                ++numPeices[peice];
                ++numTotalPeices[peice >> 3];
                material_stage_weight += PEICE_STAGE_WEIGHTS[peice];
                earlygamePositionalMaterialInbalance += EARLYGAME_PEICE_VALUE[peice][i];
                endgamePositionalMaterialInbalance += ENDGAME_PEICE_VALUE[peice][i];
            }
        }
    }
    
    // Construct a new Board object with the starting position
    Board() : Board("rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1") {}


    // PUBLIC METHODS
    // color and peice type at every square (index [0, 63] -> [a1, h8])
    inline _int peiceAt(_int index) const
    {
        return peices[index];
    }

    // returns true if the speicified color has not forfieted thier kingside castling rights
    inline bool canKingsideCastle(_int c) const noexcept
    {
        return !kingsideCastlingRightsLost[c];
    }

    // returns true if the speicified color has not forfieted thier queenside castling rights
    inline bool canQueensideCastle(_int c) const noexcept
    {
        return !queensideCastlingRightsLost[c];
    }

    // returns the square where a pawn has just passed over, -1 otherwise
    inline _int enPassantSquare() const noexcept
    {
        return eligibleEnPassantSquare[totalHalfmoves];
    }

    // returns total half moves since game start (half move is one player taking a turn)
    inline _int getTotalHalfmoves() const
    {
        return totalHalfmoves;
    }

    // returns true if the last move played has led to a repitition
    bool repititionOcurred() const noexcept
    {
        if (halfmovesSincePawnMoveOrCapture[hmspmocIndex - 1] < 4) {
            return false;
        }

        _int index = totalHalfmoves - 4;
        _int numPossibleRepitions = halfmovesSincePawnMoveOrCapture[hmspmocIndex - 1] / 2 - 1;
        uint32 currentHash = static_cast<uint32>(zobrist);
        
        for (_int i = 0; i < numPossibleRepitions; ++i) {
            if (positionHistory[index] == currentHash) {
                return true;
            }
            index -= 2;
        }
        return false;
    }

    // returns true if the last move played has led to a draw by threefold repitition
    bool isDrawByThreefoldRepitition() const noexcept
    {
        if (halfmovesSincePawnMoveOrCapture[hmspmocIndex - 1] < 8) {
            return false;
        }

        _int index = totalHalfmoves - 4;
        _int numPossibleRepitions = halfmovesSincePawnMoveOrCapture[hmspmocIndex - 1] / 2 - 1;
        uint32 currentHash = static_cast<uint32>(zobrist);
        bool repititionFound;
        
        for (_int i = 0; i < numPossibleRepitions; ++i) {
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
    
    // returns true if the last move played has led to a draw by the fifty move rule
    inline bool isDrawByFiftyMoveRule() const noexcept
    {
        return halfmovesSincePawnMoveOrCapture[hmspmocIndex - 1] >= 50;
    }
    
    // returns true if there isnt enough material on the board to deliver checkmate
    bool isDrawByInsufficientMaterial() const noexcept
    {
        if (numTotalPeices[0] > 3 || numTotalPeices[1] > 3) {
            return false;
        }
        if (numTotalPeices[0] == 3 || numTotalPeices[1] == 3) {
            return (numPeices[WHITE + KNIGHT] == 2 || numPeices[BLACK + KNIGHT] == 2) && (numTotalPeices[0] == 1 || numTotalPeices[1] == 1);
        }
        return !(numPeices[WHITE + PAWN] || numPeices[BLACK + PAWN] || numPeices[WHITE + ROOK] || numPeices[BLACK + ROOK] || numPeices[WHITE + QUEEN] || numPeices[BLACK + QUEEN]);
    }

    // Generates pseudo-legal moves for the current position
    // Populates the stack starting from the given index
    // Doesnt generate all pseudo legal moves, omits moves that are guarenteed to be illegal
    void generatePseudoLegalMoves(Move *stack, _int &idx) const
    {
        // TODO Backwards check/pin generation for endgame
        // TODO Better pinned peice generation (detect if sliding along diagonal of pin)
        
        _int c = totalHalfmoves % 2;
        _int color = c << 3;
        _int e = !color;
        _int enemy = e << 3;

        // Used for optimizing the legality checking of moves
        bool isPinned[64] = {0};
        std::unordered_set<_int> checkingSquares(11);

        // Check if king in check and record pinned peices and checking squares
        _int king = kingIndex[c];
        _int checks = 0;

        // Pawn checks
        if (numPeices[enemy + PAWN]) {
            _int kingFile = king % 8;
            _int kingAhead = king + 8 - 16 * c;
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
            for (_int j = 1; j < KNIGHT_MOVES[king][0]; ++j) {
                if (peices[KNIGHT_MOVES[king][j]] == enemy + KNIGHT) {
                    checkingSquares.insert(KNIGHT_MOVES[king][j]);
                    ++checks;
                }
            }
        }

        // pins and sliding peice checks
        _int potentialPin = 0;
        // Dont bother checking horizontals if no bishops or queens
        if (numPeices[enemy + ROOK] | numPeices[enemy + QUEEN]) {
            for (_int j = king - 8; j >= DIRECTION_BOUNDS[king][B]; j -= 8) {
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
                    for (_int k = j; k < king; k += 8) {
                        checkingSquares.insert(k);
                    }
                }
                break;
            }
            
            potentialPin = 0;
            for (_int j = king + 8; j <= DIRECTION_BOUNDS[king][F]; j += 8) {
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
                    for (_int k = j; k > king; k -= 8) {
                        checkingSquares.insert(k);
                    }
                }
                break;
            }

            potentialPin = 0;
            for (_int j = king - 1; j >= DIRECTION_BOUNDS[king][L]; j -= 1) {
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
                    for (_int k = j; k < king; k += 1) {
                        checkingSquares.insert(k);
                    }
                }
                break;
            }

            potentialPin = 0;
            for (_int j = king + 1; j <= DIRECTION_BOUNDS[king][R]; j += 1) {
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
                    for (_int k = j; k > king; k -= 1) {
                        checkingSquares.insert(k);
                    }
                }
                break;
            }
        }

        // Dont bother checking diagonals if no bishops or queens
        if (numPeices[enemy + BISHOP] | numPeices[enemy + QUEEN]) {
            potentialPin = 0;
            for (_int j = king - 9; j >= DIRECTION_BOUNDS[king][BL]; j -= 9) {
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
                    for (_int k = j; k < king; k += 9) {
                        checkingSquares.insert(k);
                    }
                }
                break;
            }

            potentialPin = 0;
            for (_int j = king + 9; j <= DIRECTION_BOUNDS[king][FR]; j += 9) {
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
                    for (_int k = j; k > king; k -= 9) {
                        checkingSquares.insert(k);
                    }
                }
                break;
            }

            potentialPin = 0;
            for (_int j = king - 7; j >= DIRECTION_BOUNDS[king][BR]; j -= 7) {
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
                    for (_int k = j; k < king; k += 7) {
                        checkingSquares.insert(k);
                    }
                }
                break;
            }

            potentialPin = 0;
            for (_int j = king + 7; j <= DIRECTION_BOUNDS[king][FL]; j += 7) {
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
                    for (_int k = j; k > king; k -= 7) {
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
            for (_int j = 1; j < KING_MOVES[king][0]; ++j) {
                if ((!peices[KING_MOVES[king][j]] && !checkingSquares.count(KING_MOVES[king][j])) || peices[KING_MOVES[king][j]] >> 3 == e) {
                    stack[idx++] = Move(this, king, KING_MOVES[king][j], Move::NONE);
                }
            }
            return;
        }

        // En passant moves
        _int epSquare = eligibleEnPassantSquare[totalHalfmoves];
        if (epSquare >= 0) {
            _int epfile = epSquare % 8;
            if (color == WHITE) {
                if (epfile != 0 && peices[epSquare - 9] == color + PAWN && (!checks || checkingSquares.count(epSquare - 8))) {
                    stack[idx++] = Move(this, epSquare - 9, epSquare, Move::EN_PASSANT);
                }
                if (epfile != 7 && peices[epSquare - 7] == color + PAWN && (!checks || checkingSquares.count(epSquare - 8))) {
                    stack[idx++] = Move(this, epSquare - 7, epSquare, Move::EN_PASSANT);
                }
            } else {
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
            for (_int j = 1; j < KING_MOVES[king][0]; ++j) {
                if ((!peices[KING_MOVES[king][j]] && !checkingSquares.count(KING_MOVES[king][j])) || peices[KING_MOVES[king][j]] >> 3 == e) {
                    stack[idx++] = Move(this, king, KING_MOVES[king][j], Move::NONE);
                }
            }

            // Backwards search from checking squares to see if peices can move to them
            for (_int t : checkingSquares) {

                // Pawn can block/take
                if (numPeices[color + PAWN]) {
                    if ((color == WHITE && t >> 3 >= 2) || (color == BLACK && t >> 3 <= 5)) {
                        _int file = t % 8;
                        _int ahead = t - 8 + 16 * c;
                        bool promotion = t >> 3 == 0 || t >> 3 == 7;
                        // Pawn capture
                        if (peices[t] && peices[t] >> 3 == e) { 
                            if (file != 0 && peices[ahead - 1] == color + PAWN && !isPinned[ahead - 1]) {
                                if (promotion) {
                                    stack[idx++] = Move(this, ahead - 1, t, KNIGHT | Move::LEGAL);
                                    stack[idx++] = Move(this, ahead - 1, t, BISHOP | Move::LEGAL);
                                    stack[idx++] = Move(this, ahead - 1, t, ROOK | Move::LEGAL);
                                    stack[idx++] = Move(this, ahead - 1, t, QUEEN | Move::LEGAL);
                                } else {
                                    stack[idx++] = Move(this, ahead - 1, t, Move::LEGAL);
                                }
                            }
                            if (file != 7 && peices[ahead + 1] == color + PAWN && !isPinned[ahead + 1]) {
                                if (promotion) {
                                    stack[idx++] = Move(this, ahead + 1, t, KNIGHT | Move::LEGAL);
                                    stack[idx++] = Move(this, ahead + 1, t, BISHOP | Move::LEGAL);
                                    stack[idx++] = Move(this, ahead + 1, t, ROOK | Move::LEGAL);
                                    stack[idx++] = Move(this, ahead + 1, t, QUEEN | Move::LEGAL);
                                } else {
                                    stack[idx++] = Move(this, ahead + 1, t, Move::LEGAL);
                                }
                            }
                        // Pawn move
                        } else if (!peices[t]) {
                            _int doubleAhead = ahead - 8 + 16 * c;
                            if (peices[ahead] == color + PAWN && !isPinned[ahead]) {
                                if (promotion) {
                                    stack[idx++] = Move(this, ahead, t, KNIGHT | Move::LEGAL);
                                    stack[idx++] = Move(this, ahead, t, BISHOP | Move::LEGAL);
                                    stack[idx++] = Move(this, ahead, t, ROOK | Move::LEGAL);
                                    stack[idx++] = Move(this, ahead, t, QUEEN | Move::LEGAL);
                                } else {
                                    stack[idx++] = Move(this, ahead, t, Move::LEGAL);
                                }
                        
                            } else if ((doubleAhead >> 3 == 1 || doubleAhead >> 3 == 6) && !peices[ahead] && peices[doubleAhead] == color + PAWN && !isPinned[doubleAhead]) {
                                stack[idx++] = Move(this, doubleAhead, t, Move::LEGAL);
                            }
                        }                
                    }
                }

                // Knight can block/take
                if (numPeices[color + KNIGHT]) {
                    for (_int j = 1; j < KNIGHT_MOVES[t][0]; ++j) {
                        if (peices[KNIGHT_MOVES[t][j]] == color + KNIGHT && !isPinned[KNIGHT_MOVES[t][j]]) {
                            stack[idx++] = Move(this, KNIGHT_MOVES[t][j], t, Move::LEGAL);
                        }
                    }
                }

                // Sliding peices can block/take
                if (numPeices[color + ROOK] | numPeices[color + QUEEN]) {
                    for (_int s = t - 8; s >= DIRECTION_BOUNDS[t][B]; s -= 8) {
                        if (peices[s]) {
                            if ((peices[s] == color + ROOK || peices[s] == color + QUEEN) && !isPinned[s]) {
                                stack[idx++] = Move(this, s, t, Move::LEGAL);
                            }
                            break;
                        }
                    }

                    for (_int s = t + 8; s <= DIRECTION_BOUNDS[t][F]; s += 8) {
                        if (peices[s]) {
                            if ((peices[s] == color + ROOK || peices[s] == color + QUEEN) && !isPinned[s]) {
                                stack[idx++] = Move(this, s, t, Move::LEGAL);
                            }
                            break;
                        }
                    }

                    for (_int s = t - 1; s >= DIRECTION_BOUNDS[t][L]; s -= 1) {
                        if (peices[s]) {
                            if ((peices[s] == color + ROOK || peices[s] == color + QUEEN) && !isPinned[s]) {
                                stack[idx++] = Move(this, s, t, Move::LEGAL);
                            }
                            break;
                        }
                    }

                    for (_int s = t + 1; s <= DIRECTION_BOUNDS[t][R]; s += 1) {
                        if (peices[s]) {
                            if ((peices[s] == color + ROOK || peices[s] == color + QUEEN) && !isPinned[s]) {
                                stack[idx++] = Move(this, s, t, Move::LEGAL);
                            }
                            break;
                        }
                    }
                }

                if (numPeices[color + BISHOP] | numPeices[color + QUEEN]) {
                    for (_int s = t - 9; s >= DIRECTION_BOUNDS[t][BL]; s -= 9) {
                        if (peices[s]) {
                            if ((peices[s] == color + BISHOP || peices[s] == color + QUEEN) && !isPinned[s]) {
                                stack[idx++] = Move(this, s, t, Move::LEGAL);
                            }
                            break;
                        }
                    }

                    for (_int s = t + 9; s <= DIRECTION_BOUNDS[t][FR]; s += 9) {
                        if (peices[s]) {
                            if ((peices[s] == color + BISHOP || peices[s] == color + QUEEN) && !isPinned[s]) {
                                stack[idx++] = Move(this, s, t, Move::LEGAL);
                            }
                            break;
                        }
                    }

                    for (_int s = t - 7; s >= DIRECTION_BOUNDS[t][BR]; s -= 7) {
                        if (peices[s]) {
                            if ((peices[s] == color + BISHOP || peices[s] == color + QUEEN) && !isPinned[s]) {
                                stack[idx++] = Move(this, s, t, Move::LEGAL);
                            }
                            break;
                        }
                    }

                    for (_int s = t + 7; s <= DIRECTION_BOUNDS[t][FL]; s += 7) {
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
            _int castlingRank = 56 * c;
            bool roomToCastle = true;
            for (_int j = castlingRank + 5; j < castlingRank + 7; ++j) {
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
            _int castlingRank = 56 * c;
            bool roomToCastle = true;
            for (_int j = castlingRank + 3; j > castlingRank; --j) {
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
        for (_int s = 0; s < 64; ++s) {
            if (peices[s] && peices[s] >> 3 == c) {
                _int legalFlag = isPinned[s] ? Move::NONE : Move::LEGAL;

                switch (peices[s] % (1 << 3)) {
                    case PAWN: {
                        _int file = s % 8;
                        _int ahead = s + 8 - 16 * c;
                        bool promotion = color == WHITE ? (s >> 3 == 6) : (s >> 3 == 1);

                        // Pawn foward moves
                        if (!peices[ahead]) {
                            if (!checks || checkingSquares.count(ahead)) {
                                if (promotion) {
                                    stack[idx++] = Move(this, s, ahead, legalFlag | KNIGHT);
                                    stack[idx++] = Move(this, s, ahead, legalFlag | BISHOP);
                                    stack[idx++] = Move(this, s, ahead, legalFlag | ROOK);
                                    stack[idx++] = Move(this, s, ahead, legalFlag | QUEEN);
                                } else {
                                    stack[idx++] = Move(this, s, ahead, legalFlag);
                                }
                            }

                            bool doubleJumpAllowed = color == WHITE ? (s >> 3 == 1) : (s >> 3 == 6);
                            _int doubleAhead = ahead + 8 - 16 * c;
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
                            } else {
                                stack[idx++] = Move(this, s, ahead - 1, legalFlag);
                            }
                        }
                        if (file != 7 && peices[ahead + 1] && peices[ahead + 1] >> 3 == e && (!checks || checkingSquares.count(ahead + 1))) {
                            if (promotion) {
                                stack[idx++] = Move(this, s, ahead + 1, legalFlag | KNIGHT);
                                stack[idx++] = Move(this, s, ahead + 1, legalFlag | BISHOP);
                                stack[idx++] = Move(this, s, ahead + 1, legalFlag | ROOK);
                                stack[idx++] = Move(this, s, ahead + 1, legalFlag | QUEEN);
                            } else {
                                stack[idx++] = Move(this, s, ahead + 1, legalFlag);
                            }
                        }
                        break;
                    }
                    case KNIGHT: {
                        _int t;
                        for (_int j = 1; j < KNIGHT_MOVES[s][0]; ++j) {
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

                        for (_int t = s - 8; t >= DIRECTION_BOUNDS[s][B]; t -= 8) {
                            if ((!peices[t] || peices[t] >> 3 == e) && (!checks || checkingSquares.count(t))) {
                                stack[idx++] = Move(this, s, t, legalFlag);
                            }
                            if (peices[t]) {
                                break;
                            }
                        }

                        for (_int t = s + 8; t <= DIRECTION_BOUNDS[s][F]; t += 8) {
                            if ((!peices[t] || peices[t] >> 3 == e) && (!checks || checkingSquares.count(t))) {
                                stack[idx++] = Move(this, s, t, legalFlag);
                            }
                            if (peices[t]) {
                                break;
                            }
                        }

                        for (_int t = s - 1; t >= DIRECTION_BOUNDS[s][L]; t -= 1) {
                            if ((!peices[t] || peices[t] >> 3 == e) && (!checks || checkingSquares.count(t))) {
                                stack[idx++] = Move(this, s, t, legalFlag);
                            }
                            if (peices[t]) {
                                break;
                            }
                        }

                        for (_int t = s + 1; t <= DIRECTION_BOUNDS[s][R]; t += 1) {
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

                        for (_int t = s - 9; t >= DIRECTION_BOUNDS[s][BL]; t -= 9) {
                            if ((!peices[t] || peices[t] >> 3 == e) && (!checks || checkingSquares.count(t))) {
                                stack[idx++] = Move(this, s, t, legalFlag);
                            }
                            if (peices[t]) {
                                break;
                            }
                        }

                        for (_int t = s + 9; t <= DIRECTION_BOUNDS[s][FR]; t += 9) {
                            if ((!peices[t] || peices[t] >> 3 == e) && (!checks || checkingSquares.count(t))) {
                                stack[idx++] = Move(this, s, t, legalFlag);
                            }
                            if (peices[t]) {
                                break;
                            }
                        }

                        for (_int t = s - 7; t >= DIRECTION_BOUNDS[s][BR]; t -= 7) {
                            if ((!peices[t] || peices[t] >> 3 == e) && (!checks || checkingSquares.count(t))) {
                                stack[idx++] = Move(this, s, t, legalFlag);
                            }
                            if (peices[t]) {
                                break;
                            }
                        }

                        for (_int t = s + 7; t <= DIRECTION_BOUNDS[s][FL]; t += 7) {
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
                        _int t;
                        for (_int j = 1; j < KING_MOVES[s][0]; ++j) {
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

    // Generates legal moves for the current position
    // Not as fast as pseudoLegalMoves() for searching
    std::vector<Move> legalMoves()
    {
        Move moves[225];
        _int end = 0;
        generatePseudoLegalMoves(moves, end);

        std::vector<Move> legalMoves;

        for (int i = 0; i < end; ++i) {
            if (isLegal(moves[i])) {
                legalMoves.push_back(moves[i]);
            }
        }

        return legalMoves;
    }

    // update the board based on the inputted move (must be pseudo legal)
    // returns true if move was legal and process completed
    bool makeMove(Move &move)
    {
        _int c = move.moving() >> 3;
        _int color = c << 3;
        _int e = !color;
        _int enemy = e << 3;

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
        } else {
            zobrist ^= ZOBRIST_PEICE_KEYS[c][move.moving() % (1 << 3) - 1][move.target()];
        }
        

        // Update zobrist hash and peice indices set for capture
        if (move.captured()) {
            _int captureSquare = move.isEnPassant() ? move.target() - 8 + 16 * c : move.target();
            zobrist ^= ZOBRIST_PEICE_KEYS[e][move.captured() % (1 << 3) - 1][captureSquare];
            --numPeices[move.captured()];
            --numTotalPeices[e];
            material_stage_weight -= PEICE_STAGE_WEIGHTS[move.captured()];
        }

        // Update rooks for castling
        if (move.isCastling()) {
            _int rookStart;
            _int rookEnd;

            _int castlingRank = move.target() & 0b11111000;
            if (move.target() % 8 < 4) {
                // Queenside castling
                rookStart = castlingRank;
                rookEnd = castlingRank + 3;
            } else {
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
        } else {
            ++halfmovesSincePawnMoveOrCapture[hmspmocIndex - 1];
        }

        // En passant file
        if (move.moving() % (1 << 3) == PAWN && std::abs(move.target() - move.start()) == 16) {
            eligibleEnPassantSquare[totalHalfmoves] = (move.start() + move.target()) / 2;
        } else {
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

        positionHistory[totalHalfmoves - 1] = static_cast<uint32>(zobrist);
        return true;
    }

    // update the board to reverse the inputted move (must have just been move previously played)
    void unmakeMove(Move &move)
    {
        _int c = move.moving() >> 3;
        _int color = c << 3;
        _int e = !color;
        _int enemy = e << 3;

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
            
        } else {
            zobrist ^= ZOBRIST_PEICE_KEYS[c][move.moving() % (1 << 3) - 1][move.target()];
        }
        zobrist ^= ZOBRIST_PEICE_KEYS[c][move.moving() % (1 << 3) - 1][move.start()];

        // Undo zobrist hash and peice indices set for capture
        if (move.captured()){
            _int captureSquare = move.isEnPassant() ? move.target() - 8 + 16 * c : move.target();
            zobrist ^= ZOBRIST_PEICE_KEYS[e][move.captured() % (1 << 3) - 1][captureSquare];
            material_stage_weight += PEICE_STAGE_WEIGHTS[move.captured()];
            ++numPeices[move.captured()];
            ++numTotalPeices[e];
        }

        // Undo rooks for castling
        if (move.isCastling()) {
            _int rookStart;
            _int rookEnd;

            _int castlingRank = move.target() & 0b11111000;
            if (move.target() % 8 < 4) {
                // Queenside castling
                rookStart = castlingRank;
                rookEnd = castlingRank + 3;
            } else {
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
        } else {
            --halfmovesSincePawnMoveOrCapture[hmspmocIndex - 1];
        }
    }
  
    // return true if the player about to move is in check
    bool inCheck() const
    {
        return inCheck(totalHalfmoves % 2);
    }
    
    // returns true if the last move has put the game into a forced draw (threefold repitition / 50 move rule / insufficient material)
    bool isDraw() const
    {
        return isDrawByFiftyMoveRule() || isDrawByInsufficientMaterial() || isDrawByThreefoldRepitition();
    }

    // return a string representation of the position in Forsyth–Edwards Notation
    std::string asFEN()
    {
        std::string fen = "";
        _int c = totalHalfmoves % 2;

        // Peice placement data
        char pcs[6] = {'P', 'N', 'B', 'R', 'Q', 'K'};
        _int gap = 0;
        for (_int i = 56; i >= 0; i -= 8) {
            for (_int j = 0; j < 8; ++j) {
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
        } else {
            fen += castlingAvailability + " ";
        }

        // En passant target
        if (eligibleEnPassantSquare[totalHalfmoves] >= 0) {
            fen += ChessHelpers::boardIndexToAlgebraicNotation(eligibleEnPassantSquare[totalHalfmoves]) + " ";
        } else {
            fen += "- ";
        }

        // Halfmoves since pawn move or capture
        fen += std::to_string(halfmovesSincePawnMoveOrCapture[hmspmocIndex - 1]);
        fen += ' ';

        // Total moves
        fen += '1' + totalHalfmoves / 2;

        return fen;
    }

    // returns a heuristic evaluation of the position based on the total material and positions of the peices on the board
    _int positionalMaterialInbalance() const noexcept
    {
        return (material_stage_weight * earlygamePositionalMaterialInbalance + (128 - material_stage_weight) * endgamePositionalMaterialInbalance) / 128;
    }


    // DEFINITIONS
    static constexpr _int WHITE = 0b0000;
    static constexpr _int BLACK = 0b1000;
    static constexpr _int PAWN =   0b001;
    static constexpr _int KNIGHT = 0b010;
    static constexpr _int BISHOP = 0b011;
    static constexpr _int ROOK =   0b100;
    static constexpr _int QUEEN =  0b101;
    static constexpr _int KING =   0b110;
private:    
    // PRIVATE MEMBERS
    // color and peice type at every square (index [0, 63] -> [a1, h8])
    _int peices[64];

    // contains the halfmove number when the kingside castling rights were lost for white or black (index 0 and 1)
    _int kingsideCastlingRightsLost[2];

    // contains the halfmove number when the queenside castling rights were lost for white or black (index 0 and 1)
    _int queensideCastlingRightsLost[2];

    // file where a pawn has just moved two squares over
    _int eligibleEnPassantSquare[500];

    // number of half moves since pawn move or capture (half move is one player taking a turn) (used for 50 move rule)
    _int halfmovesSincePawnMoveOrCapture[250];
    _int hmspmocIndex;

    // total half moves since game start (half move is one player taking a turn)
    _int totalHalfmoves;

    // index of the white and black king (index 0 and 1)
    _int kingIndex[2];

    //std::forward_list<uint32> positionHistory;
    // array of 32 bit hashes of the positions used for checking for repititions
    uint32 positionHistory[500];

    // zobrist hash of the current position
    uint64 zobrist;

    // number of peices on the board for either color and for every peice
    _int numPeices[15];

    // number of total on the board for either color 
    _int numTotalPeices[2];

    // EVALUATION
    // Inbalance of peice placement, used for evaluation function 
    _int material_stage_weight;
    _int earlygamePositionalMaterialInbalance;
    _int endgamePositionalMaterialInbalance;


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
        for (_int j = king - 8; j >= DIRECTION_BOUNDS[king][B]; j -= 8) {
            if (peices[j]) {
                if (peices[j] == enemy + ROOK || peices[j] == enemy + QUEEN) {
                    return true;
                }
                break;
            }
        }
        
        for (_int j = king + 8; j <= DIRECTION_BOUNDS[king][F]; j += 8) {
            if (peices[j]) {
                if (peices[j] == enemy + ROOK || peices[j] == enemy + QUEEN) {
                    return true;
                }
                break;
            }
        }

        for (_int j = king - 1; j >= DIRECTION_BOUNDS[king][L]; j -= 1) {
            if (peices[j]) {
                if (peices[j] == enemy + ROOK || peices[j] == enemy + QUEEN) {
                    return true;
                }
                break;
            }
        }

        for (_int j = king + 1; j <= DIRECTION_BOUNDS[king][R]; j += 1) {
            if (peices[j]) {
                if (peices[j] == enemy + ROOK || peices[j] == enemy + QUEEN) {
                    return true;
                }
                break;
            }
        }

        for (_int j = king - 9; j >= DIRECTION_BOUNDS[king][BL]; j -= 9) {
            if (peices[j]) {
                if (peices[j] == enemy + BISHOP || peices[j] == enemy + QUEEN) {
                    return true;
                }
                break;
            }
        }

        for (_int j = king + 9; j <= DIRECTION_BOUNDS[king][FR]; j += 9) {
            if (peices[j]) {
                if (peices[j] == enemy + BISHOP || peices[j] == enemy + QUEEN) {
                    return true;
                }
                break;
            }
        }

        for (_int j = king - 7; j >= DIRECTION_BOUNDS[king][BR]; j -= 7) {
            if (peices[j]) {
                if (peices[j] == enemy + BISHOP || peices[j] == enemy + QUEEN) {
                    return true;
                }
                break;
            }
        }

        for (_int j = king + 7; j <= DIRECTION_BOUNDS[king][FL]; j += 7) {
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

    // @param move pseudo legal castling move (castling rights are not lost and king is not in check)
    // @return true if the castling move is legal in the current position
    bool castlingMoveIsLegal(Move &move) {
        if (move.legalFlagSet()) {
            return true;
        }

        _int c = totalHalfmoves % 2;
        _int color = c << 3;
        _int e = !color;
        _int enemy = e << 3;
        _int castlingRank = move.start() & 0b11111000;

        // Check if anything is attacking squares on king's path
        _int s;
        _int end;
        if (move.target() - castlingRank < 4) {
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
                for (_int j = s - 8; j >= DIRECTION_BOUNDS[s][B]; j -= 8) {
                    if (peices[j]) {
                        if (peices[j] == enemy + ROOK || peices[j] == enemy + QUEEN) {
                            return false;
                        }
                        break;
                    }
                }
                
                for (_int j = s - 9; j >= DIRECTION_BOUNDS[s][BL]; j -= 9) {
                    if (peices[j]) {
                        if (peices[j] == enemy + BISHOP || peices[j] == enemy + QUEEN) {
                            return false;
                        }
                        break;
                    }
                }

                for (_int j = s - 7; j >= DIRECTION_BOUNDS[s][BR]; j -= 7) {
                    if (peices[j]) {
                        if (peices[j] == enemy + BISHOP || peices[j] == enemy + QUEEN) {
                            return false;
                        }
                        break;
                    }
                }
            
            } else {
                for (_int j = s + 8; j <= DIRECTION_BOUNDS[s][F]; j += 8) {
                    if (peices[j]) {
                        if (peices[j] == enemy + ROOK || peices[j] == enemy + QUEEN) {
                            return false;
                        }
                        break;
                    }
                }

                for (_int j = s + 9; j <= DIRECTION_BOUNDS[s][FR]; j += 9) {
                    if (peices[j]) {
                        if (peices[j] == enemy + BISHOP || peices[j] == enemy + QUEEN) {
                            return false;
                        }
                        break;
                    }
                }

                for (_int j = s + 7; j <= DIRECTION_BOUNDS[s][FL]; j += 7) {
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

        move.setLegalFlag();
        return true;
    }
};

#endif