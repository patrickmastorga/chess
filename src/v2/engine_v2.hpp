#ifndef ENGINE_V2_H
#define ENGINE_V2_H

#include <chrono>
#include <iostream>
#include <iomanip>
#include <string>
#include <stdexcept>
#include <optional>
#include <cstdint>

#include "../chess.hpp"
#include "board.hpp"
#include "precomputed.hpp"

#define MAX_DEPTH = 32
#define MOVE_STACK_SIZE = 1500

typedef std::int_fast16_t _int;
typedef std::uint_fast64_t uint64;


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
            moves.emplace_back(move.start(), move.target(), move.promotion());
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
        return StandardMove(0, 0, 0);
    }

    void inputMove(StandardMove &move) override
    {
        if (gameOver().has_value()) {
            throw std::runtime_error("Game is over, cannot input move!");
        }

        for (Board::Move &legalMove : enginePositionMoves) {
            if (legalMove == move) {
                board.makeMove(legalMove);
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

    std::uint64_t perft(int depth, bool printOut=false) noexcept override
    {
        Board::Move moveStack[1500];
        
        if (!printOut) {
            return perft_h(depth, moveStack, 0);
        }
        
        if (depth == 0) {
            return 1ULL;
        }

        std::cout << "PERFT TEST\nFEN: " << board.asFEN() << std::endl;

        std::uint64_t nodes = 0;

        for (size_t i = 0; i < enginePositionMoves.size(); ++i) {
            std::uint64_t subnodes = 0;
            
            std::cout << std::setw(2) << i << " *** " << enginePositionMoves[i] << ": ";
            std::cout.flush();

            if (board.makeMove(enginePositionMoves[i])) {
                subnodes = perft_h(depth - 1, moveStack, 0);
                nodes += subnodes;
                board.unmakeMove(enginePositionMoves[i]);
            }

            std::cout << subnodes << std::endl;
        }

        std::cout << "TOTAL: " << nodes << std::endl;
        return nodes;
    }

private:
    // Legal moves for the current position stored in the engine
    std::vector<Board::Move> enginePositionMoves;

    // Member for storing the current position being analysed
    Board board;

    // PRIVATE METHODS
    // returns number of total positions a certain depth away
    std::uint64_t perft_h(_int depth, Board::Move *moveStack, _int startMoves)
    {
        if (depth == 0) {
            return 1ULL;
        }

        _int endMoves = startMoves;
        board.generatePseudoLegalMoves(moveStack, endMoves);

        std::uint64_t nodes = 0;

        for (_int i = startMoves; i < endMoves; ++i) {
            if (board.makeMove(moveStack[i])) {
                nodes += perft_h(depth - 1, moveStack, endMoves);
                board.unmakeMove(moveStack[i]);
            }
        }

        return nodes;
    }

    _int search_std(_int depth, Board::Move *moveStack, _int startMoves)
    {
        // TODO
        return 0;
    }

    _int evaluate()
    {
        // TODO
        return 0;
    }
};

/*

Search ideas:
 - IGNORE EN-PASSANT targets in zobrist hashing (not even needed for three-fold repition - easy fix)

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

#endif