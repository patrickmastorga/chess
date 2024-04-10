#pragma once
#include "StandardEngine.h"
#include "PerftTestableEngine.h"
#include "StandardMove.h"

#include <cstdint>
#include <optional>
#include <vector>
#include <unordered_set>
#include <string>

constexpr int MAX_GAME_LENGTH = 500;
constexpr int MAX_DEPTH = 32;
constexpr int MOVE_STACK_SIZE = 1500;
/*
typedef std::int_fast8_t std::int_fast8_t;
typedef std::uint_fast8_t std::uint_fast8_t;
typedef std::int_fast32_t std::int_fast32_t;
typedef std::uint_fast32_t std::uint_fast32_t;
typedef std::uint_fast64_t std::uint_fast64_t;
*/

class EngineV1_1 : public PerftTestableEngine
{
public:
    EngineV1_1(const std::string& fenString);

    EngineV1_1();

    void loadStartingPosition() override;

    void loadFEN(const std::string& fenString) override;

    std::vector<StandardMove> getLegalMoves() noexcept override;

    int colorToMove() noexcept override;

    StandardMove computerMove(std::chrono::milliseconds thinkTime) override;

    void inputMove(const StandardMove& move) override;

    std::optional<int> gameOver() noexcept override;

    bool inCheck() const noexcept override;

    std::string asFEN() const noexcept override;

    std::uint64_t perft(int depth, bool printOut) noexcept override;

    std::uint64_t search_perft(int depth) noexcept override;

    std::uint64_t search_perft(std::chrono::milliseconds thinkTime) noexcept override;

private:
    // DEFINITIONS
    static constexpr std::uint_fast8_t WHITE = 0b0000;
    static constexpr std::uint_fast8_t BLACK = 0b1000;
    static constexpr std::uint_fast8_t PAWN = 0b001;
    static constexpr std::uint_fast8_t KNIGHT = 0b010;
    static constexpr std::uint_fast8_t BISHOP = 0b011;
    static constexpr std::uint_fast8_t ROOK = 0b100;
    static constexpr std::uint_fast8_t QUEEN = 0b101;
    static constexpr std::uint_fast8_t KING = 0b110;


    // MOVE STRUCT
    // struct for containing info about a move
    class Move
    {
    public:
        // Starting square of the move [0, 63] -> [a1, h8]
        inline std::uint_fast8_t start() const noexcept;

        // Ending square of the move [0, 63] -> [a1, h8]
        inline std::uint_fast8_t target() const noexcept;

        // Peice and color of moving peice (peice that is on the starting square of the move
        inline std::uint_fast8_t moving() const noexcept;

        // Peice and color of captured peice
        inline std::uint_fast8_t captured() const noexcept;

        // Returns the color of the whose move is
        inline std::uint_fast8_t color() const noexcept;

        // Returs the color who the move is being played against
        inline std::uint_fast8_t enemy() const noexcept;

        // In case of promotion, returns the peice value of the promoted peice
        inline std::uint_fast8_t promotion() const noexcept;

        // Returns true if move is castling move
        inline bool isEnPassant() const noexcept;

        // Returns true if move is en passant move
        inline bool isCastling() const noexcept;

        // Returns true of move is guarrenteed to be legal in the posiiton it was generated in
        inline bool legalFlagSet() const noexcept;

        // Sets the legal flag of the move
        inline void setLegalFlag() noexcept;

        inline void unSetLegalFlag() noexcept;

        inline std::int_fast32_t earlygamePositionalMaterialChange() noexcept;

        inline std::int_fast32_t endgamePositionalMaterialChange() noexcept;

        // Heuristic geuss for the strength of the move (used for move ordering)
        std::int_fast32_t strengthGuess;

        // Override equality operator with other move
        inline bool operator==(const Move& other) const;

        // Override equality operator with standard move
        inline bool operator==(const StandardMove& other) const;

        // Override stream insertion operator to display info about the move
        std::string toString() const;

        // CONSTRUCTORS
        // Construct a new Move object from the given board and given flags (en_passant, castle, promotion, etc.)
        Move(const EngineV1_1* board, std::uint_fast8_t start, std::uint_fast8_t target, std::uint_fast8_t givenFlags);

        Move();

        // FLAGS
        static constexpr std::uint_fast8_t NONE = 0b00000000;
        static constexpr std::uint_fast8_t PROMOTION = 0b00000111;
        static constexpr std::uint_fast8_t LEGAL = 0b00001000;
        static constexpr std::uint_fast8_t EN_PASSANT = 0b00010000;
        static constexpr std::uint_fast8_t CASTLE = 0b00100000;
    private:
        // Starting square of the move [0, 63] -> [a1, h8]
        std::uint_fast8_t startSquare;

        // Ending square of the move [0, 63] -> [a1, h8]
        std::uint_fast8_t targetSquare;

        // Peice and color of moving peice (peice that is on the starting square of the move
        std::uint_fast8_t movingPeice;

        // Peice and color of captured peice
        std::uint_fast8_t capturedPeice;

        // 8 bit flags of move
        std::uint_fast8_t flags;

        bool posmatInit;

        std::int_fast32_t earlyPosmat;
        std::int_fast32_t endPosmat;

        void initializePosmat() noexcept;
    };

    // BOARD MEMBERS
    // color and peice type at every square (index [0, 63] -> [a1, h8])
    std::uint_fast8_t peices[64];

    // contains the halfmove number when the kingside castling rights were lost for white or black (index 0 and 1)
    std::int_fast32_t kingsideCastlingRightsLost[2];

    // contains the halfmove number when the queenside castling rights were lost for white or black (index 0 and 1)
    std::int_fast32_t queensideCastlingRightsLost[2];

    // | 6 bits en-passant square | 6 bits halfmoves since pawn move/capture | 20 bits zobrist hash |
    std::uint_fast32_t positionInfo[MAX_DEPTH + 50];
    std::uint_fast8_t positionInfoIndex;

    // total half moves since game start (half move is one player taking a turn)
    std::uint_fast32_t totalHalfmoves;

    // index of the white and black king (index 0 and 1)
    std::uint_fast8_t kingIndex[2];

    // zobrist hash of the current position
    std::uint_fast64_t zobrist;

    // number of peices on the board for either color and for every peice
    std::uint_fast8_t numPeices[15];

    // number of total on the board for either color 
    std::uint_fast8_t numTotalPeices[2];


    // SEARCH/EVALUATION MEMBERS
    // Legal moves for the current position stored in the engine
    std::vector<Move> enginePositionMoves;

    // Inbalance of peice placement, used for evaluation function 
    std::uint_fast8_t material_stage_weight;
    std::int_fast32_t earlygamePositionalMaterialInbalance;
    std::int_fast32_t endgamePositionalMaterialInbalance;

    // Search data
    std::uint_fast32_t nodesSearchedThisMove;


    // BOARD METHODS
    // Initialize engine members for position
    void initializeFen(const std::string& fenString);

    // Generates pseudo-legal moves for the current position
    // Populates the stack starting from the given index
    // Doesnt generate all pseudo legal moves, omits moves that are guarenteed to be illegal
    // Returns true of the king was in check
    bool generatePseudoLegalMoves(Move* stack, std::uint_fast32_t& idx, bool generateOnlyCaptures = false) noexcept;

    // Generates legal moves for the current position
    // Not as fast as pseudoLegalMoves() for searching
    std::vector<Move> legalMoves();

    // Generates pseudo-legal captures for the current position
    // Populates the stack starting from the given index
    // Assumes the king is not in check
    void generateCaptures(Move* stack, std::uint_fast32_t& idx, bool* pinnedPeices) const;

    // update the board based on the inputted move (must be pseudo legal)
    // returns true if move was legal and process completed
    bool makeMove(Move& move);

    // update the board to reverse the inputted move (must have just been move previously played)
    void unmakeMove(Move& move);

    // returns true if the last move has put the game into a forced draw (threefold repitition / 50 move rule / insufficient material)
    inline bool isDraw() const;

    // returns the number of moves since pawn move or capture
    inline std::uint_fast8_t halfMovesSincePawnMoveOrCapture() const noexcept;

    // returns the index of the square over which a pawn has just jumped over
    inline std::uint_fast8_t eligibleEnpassantSquare() const noexcept;

    // returns true if the last move played has led to a draw by threefold repitition
    bool isDrawByThreefoldRepitition() const noexcept;

    // returns true if the last move played has led to a draw by the fifty move rule
    inline bool isDrawByFiftyMoveRule() const noexcept;

    // returns true if there isnt enough material on the board to deliver checkmate
    bool isDrawByInsufficientMaterial() const noexcept;

    // returns true if the last move played has led to a repitition
    bool repititionOcurred() const noexcept;

    // return true if the king belonging to the inputted color is currently being attacked
    bool inCheck(std::uint_fast8_t c) const;

    // return true if inputted pseudo legal move is legal in the current position
    bool isLegal(Move& move);

    // @param move pseudo legal castling move (castling rights are not lost and king is not in check)
    // @return true if the castling move is legal in the current position
    bool castlingMoveIsLegal(Move& move);


    // SEARCH/EVAL METHODS
    
    // Resets the search parameters
    void resetSearchMembers();
    
    // returns number of total positions a certain depth away
    std::uint64_t perft_h(std::uint_fast8_t depth, Move* moveStack, std::uint_fast32_t startMoves);

    // Standard minimax search
    std::int_fast32_t search_std(std::uint_fast8_t plyFromRoot, std::uint_fast8_t depth, Move* moveStack, std::uint_fast32_t startMoves, std::int_fast32_t alpha, std::int_fast32_t beta);

    // Quiscence search
    std::int_fast32_t search_quiscence(std::uint_fast8_t plyFromRoot, Move* moveStack, std::uint_fast32_t startMoves, std::int_fast32_t alpha, std::int_fast32_t beta);

    // Static evaluation function
    std::int_fast32_t evaluate();

    // returns a heuristic evaluation of the position based on the total material and positions of the peices on the board
    inline std::int_fast32_t lazyEvaluation() const noexcept;


    // MOVE ORDERING CLASS
    // Wrapper container for the move stack which handles move ordering
    class MoveOrderer
    {
    public:
        // Initializes the container by generating heuristic scores for all of the moves in the stack within the bounds
        MoveOrderer(EngineV1_1* engine, Move* moveStack, std::uint_fast32_t startMoves, std::uint_fast32_t endMoves, Move* skipThisMove = nullptr);

        // Generates a heuristic guess for how strong a move is based on the current position
        static void generateStrengthGuess(EngineV1_1* engine, Move& move);

        class Iterator {
        public:
            Iterator(Move* start, std::uint_fast32_t size, std::uint_fast32_t currentIndex);

            Move& operator*();

            // Retreives move with the next highest score (selection sort)
            Iterator& operator++();

            bool operator!=(const Iterator& other) const;

        private:
            Move* start;
            std::uint_fast32_t size;
            std::uint_fast32_t idx;
        };

        // Retreives the move with the highest score
        Iterator begin();

        inline Iterator end();

    private:
        std::uint_fast32_t startMoves;

        std::uint_fast32_t endMoves;

        Move* moveStack;
    };
};