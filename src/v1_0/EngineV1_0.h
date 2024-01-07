#pragma once
#include "StandardEngine.h"
#include "StandardMove.h"

typedef std::int_fast16_t int16;
typedef std::int_fast8_t int8;
typedef std::int_fast8_t uint8;
typedef std::uint_fast32_t uint32;
typedef std::uint_fast64_t uint64;

class EngineV1_0 : public StandardEngine
{
public:
    EngineV1_0(std::string& fenString);

    EngineV1_0();

    void loadStartingPosition() override;

    void loadFEN(const std::string& fenString) override;

    std::vector<StandardMove> getLegalMoves() noexcept override;

    int colorToMove() noexcept override;

    StandardMove computerMove(std::chrono::milliseconds thinkTime) override;

    void inputMove(const StandardMove& move) override;

    std::optional<int> gameOver() noexcept override;

    bool inCheck() const noexcept override;

    std::string asFEN() const noexcept override;

    std::uint64_t perft(int depth, bool printOut) noexcept;

private:
    // DEFINITIONS
    static constexpr int16 WHITE = 0b0000;
    static constexpr int16 BLACK = 0b1000;
    static constexpr int16 PAWN = 0b001;
    static constexpr int16 KNIGHT = 0b010;
    static constexpr int16 BISHOP = 0b011;
    static constexpr int16 ROOK = 0b100;
    static constexpr int16 QUEEN = 0b101;
    static constexpr int16 KING = 0b110;


    // MOVE STRUCT
    // struct for containing info about a move
    class Move
    {
    public:
        // Starting square of the move [0, 63] -> [a1, h8]
        inline int16 start() const noexcept;

        // Ending square of the move [0, 63] -> [a1, h8]
        inline int16 target() const noexcept;

        // Peice and color of moving peice (peice that is on the starting square of the move
        inline int16 moving() const noexcept;

        // Peice and color of captured peice
        inline int16 captured() const noexcept;

        // Returns the color of the whose move is
        inline int16 color() const noexcept;

        // Returs the color who the move is being played against
        inline int16 enemy() const noexcept;

        // In case of promotion, returns the peice value of the promoted peice
        inline int16 promotion() const noexcept;

        // Returns true if move is castling move
        inline bool isEnPassant() const noexcept;

        // Returns true if move is en passant move
        inline bool isCastling() const noexcept;

        // Returns true of move is guarrenteed to be legal in the posiiton it was generated in
        inline bool legalFlagSet() const noexcept;

        // Sets the legal flag of the move
        inline void setLegalFlag() noexcept;

        inline int16 earlygamePositionalMaterialChange() noexcept;

        inline int16 endgamePositionalMaterialChange() noexcept;

        // Heuristic geuss for the strength of the move (used for move ordering)
        int16 strengthGuess;

        // Override equality operator with other move
        inline bool operator==(const Move& other) const;

        // Override equality operator with standard move
        inline bool operator==(const StandardMove& other) const;

        // Override stream insertion operator to display info about the move
        std::string toString();

        // CONSTRUCTORS
        // Construct a new Move object from the given board and given flags (en_passant, castle, promotion, etc.)
        Move(const EngineV1_0* board, int16 start, int16 target, int16 givenFlags);

        Move();

        // FLAGS
        static constexpr int16 NONE = 0b00000000;
        static constexpr int16 PROMOTION = 0b00000111;
        static constexpr int16 LEGAL = 0b00001000;
        static constexpr int16 EN_PASSANT = 0b00010000;
        static constexpr int16 CASTLE = 0b00100000;
    private:
        // Starting square of the move [0, 63] -> [a1, h8]
        int16 startSquare;

        // Ending square of the move [0, 63] -> [a1, h8]
        int16 targetSquare;

        // Peice and color of moving peice (peice that is on the starting square of the move
        int16 movingPeice;

        // Peice and color of captured peice
        int16 capturedPeice;

        // 8 bit flags of move
        int16 flags;

        bool posmatInit;

        int16 earlyPosmat;
        int16 endPosmat;

        void initializePosmat() noexcept;
    };

    // BOARD MEMBERS
    // color and peice type at every square (index [0, 63] -> [a1, h8])
    int16 peices[64];

    // contains the halfmove number when the kingside castling rights were lost for white or black (index 0 and 1)
    int16 kingsideCastlingRightsLost[2];

    // contains the halfmove number when the queenside castling rights were lost for white or black (index 0 and 1)
    int16 queensideCastlingRightsLost[2];

    // file where a pawn has just moved two squares over
    int16 eligibleEnPassantSquare[500];

    // number of half moves since pawn move or capture (half move is one player taking a turn) (used for 50 move rule)
    int16 halfmovesSincePawnMoveOrCapture[250];
    int16 hmspmocIndex;

    // total half moves since game start (half move is one player taking a turn)
    int16 totalHalfmoves;

    // index of the white and black king (index 0 and 1)
    int16 kingIndex[2];

    //std::forward_list<uint32> positionHistory;
    // array of 32 bit hashes of the positions used for checking for repititions
    uint32 positionHistory[500];

    // zobrist hash of the current position
    uint64 zobrist;

    // number of peices on the board for either color and for every peice
    int16 numPeices[15];

    // number of total on the board for either color 
    int16 numTotalPeices[2];


    // SEARCH/EVALUATION MEMBERS
    // Legal moves for the current position stored in the engine
    std::vector<Move> enginePositionMoves;

    // Inbalance of peice placement, used for evaluation function 
    int16 material_stage_weight;
    int16 earlygamePositionalMaterialInbalance;
    int16 endgamePositionalMaterialInbalance;


    // BOARD METHODS
    // Initialize engine members for position
    void initializeFen(const std::string& fenString);

    // Generates pseudo-legal moves for the current position
    // Populates the stack starting from the given index
    // Doesnt generate all pseudo legal moves, omits moves that are guarenteed to be illegal
    void generatePseudoLegalMoves(Move* stack, int16& idx) const;

    // Generates legal moves for the current position
    // Not as fast as pseudoLegalMoves() for searching
    std::vector<Move> legalMoves();

    // update the board based on the inputted move (must be pseudo legal)
    // returns true if move was legal and process completed
    bool makeMove(Move& move);

    // update the board to reverse the inputted move (must have just been move previously played)
    void unmakeMove(Move& move);

    // returns true if the last move has put the game into a forced draw (threefold repitition / 50 move rule / insufficient material)
    bool isDraw() const;

    // returns true if the last move played has led to a draw by threefold repitition
    bool isDrawByThreefoldRepitition() const noexcept;

    // returns true if the last move played has led to a draw by the fifty move rule
    inline bool isDrawByFiftyMoveRule() const noexcept;

    // returns true if there isnt enough material on the board to deliver checkmate
    bool isDrawByInsufficientMaterial() const noexcept;

    // returns true if the last move played has led to a repitition
    bool repititionOcurred() const noexcept;

    // return true if the king belonging to the inputted color is currently being attacked
    bool inCheck(int16 c) const;

    // return true if inputted pseudo legal move is legal in the current position
    bool isLegal(Move& move);

    // @param move pseudo legal castling move (castling rights are not lost and king is not in check)
    // @return true if the castling move is legal in the current position
    bool castlingMoveIsLegal(Move& move);


    //SEARCH/EVAL METHODS
    // returns number of total positions a certain depth away
    std::uint64_t perft_h(int16 depth, Move* moveStack, int16 startMoves);

    // Standard minimax search
    int16 search_std(int16 plyFromRoot, int16 depth, Move* moveStack, int16 startMoves);

    // Static evaluation function
    int16 evaluate();

    // returns a heuristic evaluation of the position based on the total material and positions of the peices on the board
    inline int16 lazyEvaluation() const noexcept;
};