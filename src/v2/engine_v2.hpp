#include "./chess.hpp"
#include "board.hpp"

class EngineV2 : public StandardEngine
{
public:
    void loadStartingPosition() override
    {
        // TODO
    }

    void loadFEN() override
    {
        // TODO
    }

    StandardMove computerMove() override
    {
        // TODO
    }

    void inputMove() override
    {
        // TODO
    }

private:
    Board board;
};