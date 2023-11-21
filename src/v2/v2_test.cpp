#include <iostream>

#include "engine_v2.hpp"

int main()
{
    EngineV2 engine;

    std::vector<StandardMove> legalMoves = engine.generateLegalMoves();

    std::cout << "Starting Position Legal Moves" << std::endl;

    std::cout << "Number of moves: " << legalMoves.size() << std::endl;

    for (StandardMove& move : legalMoves) {
        std::cout << " *** " << move;
    }
}