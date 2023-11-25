#include <iostream>
#include <vector>
#include <string>
#include <memory>
#include "../chess.hpp"
#include "engine_v2.hpp"

int main()
{
    /*
    std::string fen = "rnbqk1nr/pppp1ppp/8/4p3/1b1P4/5N2/PPP1PPPP/RNBQKB1R w KQkq - 0 1";
    EngineV2 engine(fen);

    std::vector<StandardMove> moves = engine.legalMoves();

    std::cout << "MOVES for " << fen << ": \n";
    std::cout << "number of moves: " << moves.size() << std::endl;
    for (StandardMove &move : moves) {
        std::cout << " *** " << move << std::endl;
    }
    */

    std::unique_ptr<StandardEngine> engine = std::make_unique<EngineV2>();
    engine->perft(4, true);
    return 0;
}

// Maybe the optional parameter in move constructor was kinda a cheecky fix