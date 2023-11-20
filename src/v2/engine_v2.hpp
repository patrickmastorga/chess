#include "./chess.hpp"
#include "board.hpp"
#include <chrono>
#include <string>
#include <stdexcept>


class EngineV2 : public StandardEngine
{
public:
    void loadStartingPosition() override
    {
        // TODO
    }

    std::vector<StandardMove> generateLegalMoves() override
    {
        // TODO
    }

    void loadFEN(std::string &fenString) override
    {
        // TODO
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