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

typedef std::int_fast8_t int8;
typedef std::uint_fast8_t uint8;
typedef std::int_fast32_t int32;
typedef std::uint_fast32_t uint32;
typedef std::uint_fast64_t uint64;

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

    void printZobrist();
private:
    // DEFINITIONS
    static constexpr uint8 WHITE = 0b0000;
    static constexpr uint8 BLACK = 0b1000;
    static constexpr uint8 PAWN = 0b001;
    static constexpr uint8 KNIGHT = 0b010;
    static constexpr uint8 BISHOP = 0b011;
    static constexpr uint8 ROOK = 0b100;
    static constexpr uint8 QUEEN = 0b101;
    static constexpr uint8 KING = 0b110;


    // MOVE STRUCT
    // struct for containing info about a move
    class Move
    {
    public:
        // Starting square of the move [0, 63] -> [a1, h8]
        inline uint8 start() const noexcept;

        // Ending square of the move [0, 63] -> [a1, h8]
        inline uint8 target() const noexcept;

        // Peice and color of moving peice (peice that is on the starting square of the move
        inline uint8 moving() const noexcept;

        // Peice and color of captured peice
        inline uint8 captured() const noexcept;

        // Returns the color of the whose move is
        inline uint8 color() const noexcept;

        // Returs the color who the move is being played against
        inline uint8 enemy() const noexcept;

        // In case of promotion, returns the peice value of the promoted peice
        inline uint8 promotion() const noexcept;

        // Returns true if move is castling move
        inline bool isEnPassant() const noexcept;

        // Returns true if move is en passant move
        inline bool isCastling() const noexcept;

        // Returns true of move is guarrenteed to be legal in the posiiton it was generated in
        inline bool legalFlagSet() const noexcept;

        // Sets the legal flag of the move
        inline void setLegalFlag() noexcept;

        inline void unSetLegalFlag() noexcept;

        inline int32 earlygamePositionalMaterialChange() noexcept;

        inline int32 endgamePositionalMaterialChange() noexcept;

        // Heuristic geuss for the strength of the move (used for move ordering)
        int32 strengthGuess;

        // Override equality operator with other move
        inline bool operator==(const Move& other) const;

        // Override equality operator with standard move
        inline bool operator==(const StandardMove& other) const;

        // Override stream insertion operator to display info about the move
        std::string toString() const;

        // CONSTRUCTORS
        // Construct a new Move object from the given board and given flags (en_passant, castle, promotion, etc.)
        Move(const EngineV1_1* board, uint8 start, uint8 target, uint8 givenFlags);

        Move();

        // FLAGS
        static constexpr uint8 NONE = 0b00000000;
        static constexpr uint8 PROMOTION = 0b00000111;
        static constexpr uint8 LEGAL = 0b00001000;
        static constexpr uint8 EN_PASSANT = 0b00010000;
        static constexpr uint8 CASTLE = 0b00100000;
    private:
        // Starting square of the move [0, 63] -> [a1, h8]
        uint8 startSquare;

        // Ending square of the move [0, 63] -> [a1, h8]
        uint8 targetSquare;

        // Peice and color of moving peice (peice that is on the starting square of the move
        uint8 movingPeice;

        // Peice and color of captured peice
        uint8 capturedPeice;

        // 8 bit flags of move
        uint8 flags;

        bool posmatInit;

        int32 earlyPosmat;
        int32 endPosmat;

        void initializePosmat() noexcept;
    };

    // BOARD MEMBERS
    // color and peice type at every square (index [0, 63] -> [a1, h8])
    uint8 peices[64];

    // contains the halfmove number when the kingside castling rights were lost for white or black (index 0 and 1)
    int32 kingsideCastlingRightsLost[2];

    // contains the halfmove number when the queenside castling rights were lost for white or black (index 0 and 1)
    int32 queensideCastlingRightsLost[2];

    // | 6 bits en-passant square | 6 bits halfmoves since pawn move/capture | 20 bits zobrist hash |
    uint32 positionInfo[MAX_DEPTH + 50];
    uint8 positionInfoIndex;

    // total half moves since game start (half move is one player taking a turn)
    uint32 totalHalfmoves;

    // index of the white and black king (index 0 and 1)
    uint8 kingIndex[2];

    // zobrist hash of the current position
    uint64 zobrist;

    // number of peices on the board for either color and for every peice
    uint8 numPeices[15];

    // number of total on the board for either color 
    uint8 numTotalPeices[2];


    // SEARCH/EVALUATION MEMBERS
    // Legal moves for the current position stored in the engine
    std::vector<Move> enginePositionMoves;

    // Inbalance of peice placement, used for evaluation function 
    uint8 material_stage_weight;
    int32 earlygamePositionalMaterialInbalance;
    int32 endgamePositionalMaterialInbalance;


    // BOARD METHODS
    // Initialize engine members for position
    void initializeFen(const std::string& fenString);

    // Generates pseudo-legal moves for the current position
    // Populates the stack starting from the given index
    // Doesnt generate all pseudo legal moves, omits moves that are guarenteed to be illegal
    // Returns true of the king was in check
    bool generatePseudoLegalMoves(Move* stack, uint32& idx, bool generateOnlyCaptures = false) noexcept;

    // Generates legal moves for the current position
    // Not as fast as pseudoLegalMoves() for searching
    std::vector<Move> legalMoves();

    // Generates pseudo-legal captures for the current position
    // Populates the stack starting from the given index
    // Assumes the king is not in check
    void generateCaptures(Move* stack, uint32& idx, bool* pinnedPeices) const;

    // update the board based on the inputted move (must be pseudo legal)
    // returns true if move was legal and process completed
    bool makeMove(Move& move);

    // update the board to reverse the inputted move (must have just been move previously played)
    void unmakeMove(Move& move);

    // returns true if the last move has put the game into a forced draw (threefold repitition / 50 move rule / insufficient material)
    bool isDraw() const;

    // returns the number of moves since pawn move or capture
    inline uint8 halfMovesSincePawnMoveOrCapture() const noexcept;

    // returns the index of the square over which a pawn has just jumped over
    inline uint8 eligibleEnpassantSquare() const noexcept;

    // returns true if the last move played has led to a draw by threefold repitition
    bool isDrawByThreefoldRepitition() const noexcept;

    // returns true if the last move played has led to a draw by the fifty move rule
    inline bool isDrawByFiftyMoveRule() const noexcept;

    // returns true if there isnt enough material on the board to deliver checkmate
    bool isDrawByInsufficientMaterial() const noexcept;

    // returns true if the last move played has led to a repitition
    bool repititionOcurred() const noexcept;

    // return true if the king belonging to the inputted color is currently being attacked
    bool inCheck(uint8 c) const;

    // return true if inputted pseudo legal move is legal in the current position
    bool isLegal(Move& move);

    // @param move pseudo legal castling move (castling rights are not lost and king is not in check)
    // @return true if the castling move is legal in the current position
    bool castlingMoveIsLegal(Move& move);


    //SEARCH/EVAL METHODS
    // returns number of total positions a certain depth away
    std::uint64_t perft_h(uint8 depth, Move* moveStack, uint32 startMoves);

    // Standard minimax search
    int32 search_std(uint8 plyFromRoot, uint8 depth, Move* moveStack, uint32 startMoves, int32 alpha, int32 beta, uint32& nodesSearched);

    // Quiscence search
    int32 search_quiscence(Move* moveStack, uint32 startMoves, int32 alpha, int32 beta, uint32& nodesSearched);

    // Static evaluation function
    int32 evaluate();

    // returns a heuristic evaluation of the position based on the total material and positions of the peices on the board
    inline int32 lazyEvaluation() const noexcept;


    // MOVE ORDERING CLASS
    // Wrapper container for the move stack which handles move ordering
    class MoveOrderer
    {
    public:
        // Initializes the container by generating heuristic scores for all of the moves in the stack within the bounds
        MoveOrderer(EngineV1_1* engine, Move* moveStack, uint32 startMoves, uint32 endMoves, Move* skipThisMove = nullptr);

        // Generates a heuristic guess for how strong a move is based on the current position
        static void generateStrengthGuess(EngineV1_1* engine, Move& move);

        class Iterator {
        public:
            Iterator(Move* start, uint32 size, uint32 currentIndex);

            Move& operator*();

            // Retreives move with the next highest score (selection sort)
            Iterator& operator++();

            bool operator!=(const Iterator& other) const;

        private:
            Move* start;
            uint32 size;
            uint32 idx;
        };

        // Retreives the move with the highest score
        Iterator begin();

        Iterator end();

    private:
        uint32 startMoves;

        uint32 endMoves;

        Move* moveStack;
    };
};