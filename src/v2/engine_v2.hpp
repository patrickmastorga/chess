#include <chrono>
#include <string>
#include <stdexcept>
#include <optional>

#include "chess.hpp"
#include "board.hpp"


class EngineV2 : public StandardEngine
{
public:
    EngineV2(std::string &fenString) : board(fenString)
    {
        enginePositionMoves = board.legalMoves();
    }

    EngineV2() : board()
    {
        enginePositionMoves = board.legalMoves();
    }

    void loadStartingPosition() noexcept override
    {
        board = Board();
        enginePositionMoves = board.legalMoves();
    }

    void loadFEN(std::string &fenString) override
    {
        board = Board(fenString);
        enginePositionMoves = board.legalMoves();
    }
    
    std::vector<StandardMove> legalMoves() noexcept override
    {
        std::vector<StandardMove> moves;

        for (Board::Move &move : enginePositionMoves) {
            moves.emplace_back(move.startSquare, move.targetSquare, move.flags & Board::Move::PROMOTION);
        }

        return moves;
    }

    int colorToMove() noexcept override
    {
        return 1 - 2 * (board.getTotalHalfmoves() % 2);
    }
    
    StandardMove computerMove(std::chrono::milliseconds thinkTime) override
    {
        if (gameOver().has_value()) {
            throw std::runtime_error("Game is over, cannot get computer move!");
        }
        // TODO
    }

    void inputMove(StandardMove &move) override
    {
        if (gameOver().has_value()) {
            throw std::runtime_error("Game is over, cannot input move!");
        }

        for (Board::Move &legalMove : enginePositionMoves) {
            if (legalMove == move) {
                if (!board.makeMove(legalMove)) {
                    throw std::runtime_error("This shouldn't happen!");
                }
                enginePositionMoves = board.legalMoves();
                return;
            }
        }

        throw std::runtime_error("inputted move is not legal in the current position!");
    }

    std::optional<int> gameOver() noexcept override
    {
        if (board.isDraw()) {
            return 0;
        }

        if (enginePositionMoves.size() == 0) {
            return inCheck() ? -colorToMove() : 0;
        }

        return std::nullopt;
    }
    
    bool inCheck() const noexcept override
    {
        return board.inCheck();
    }

    int perft(int depth) noexcept override
    {
        // IMPROVMENT IDEAS:
        // - IGNORE EN-PASSANT targets in zobrist hashing (not even needed for three-fold repition - easy fix)
        // TODO
    }

private:
    // Legal moves for the current position stored in the engine
    std::vector<Board::Move> enginePositionMoves;

    // Member for storing the current position being analysed
    Board board;

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