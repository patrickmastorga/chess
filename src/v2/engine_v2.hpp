#include "./chess.hpp"
#include "board.hpp"
#include <chrono>
#include <string>
#include <stdexcept>


class EngineV2 : public StandardEngine
{
public:
    EngineV2() : board() {}

    void loadStartingPosition() override
    {
        Board newBoard;
        board = newBoard;
    }

    void loadFEN(std::string &fenString) override
    {
        Board newBoard(fenString);
        board = newBoard;
    }
    
    std::vector<StandardMove> generateLegalMoves() override
    {
        std::vector<Board::Move> pseudoLegalMoves = board.pseudoLegalMoves();
        std::vector<StandardMove> legalMoves;

        for (const Board::Move &move : pseudoLegalMoves) {
            if (board.isLegal(move)) {
                legalMoves.emplace_back(move.startSquare, move.targetSquare, move.flags & Board::Move::PROMOTION);
            }
        }

        return legalMoves;
    }

    StandardMove computerMove(std::chrono::milliseconds thinkTime) override
    {
        // TODO
    }

    void inputMove(StandardMove &move) override
    {
        // TODO
    }


    bool inCheck() override
    {
        // TODO
    }

    bool isDraw() override
    {
        // TODO
    }

private:
    /**
     * Member for storing the current position being analysed
     */
    Board board;

};