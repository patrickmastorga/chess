#include "./chess.hpp"
#include "board.hpp"
#include <chrono>
#include <string>
#include <stdexcept>


class EngineV2 : public StandardEngine
{
public:
    EngineV2(std::string &fenString) : board(fenString) {}
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
        std::vector<StandardMove> legalMoves = generateLegalMoves();

        if (isDraw()) {
            throw std::runtime_error("Game is drawn! Cannot input move!");
        }

        if (legalMoves.size() == 0 && inCheck()) {
            throw std::runtime_error("Player to move is in checkmate Cannot input move!");
        }

        for (StandardMove &legalMove : legalMoves) {
            if (move == legalMove) {
                board.makeMove(Board::Move(&board, move.startSquare, move.targetSquare, move.promotion));
                return;
            }
        }

        throw std::runtime_error("inputted move is not legal!");
    }

    bool inCheck() override
    {
        return board.inCheck();
    }

    bool isDraw() override
    {
        return board.isDraw();
    }

    int perft(int depth) override
    {
        // TODO
    }

private:
    /**
     * Member for storing the current position being analysed
     */
    Board board;

};