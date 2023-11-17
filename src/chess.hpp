struct StandardMove
{
    int startSquare;

    int targetSquare;

    StandardMove(int start, int target) : startSquare(start), targetSquare(target) {}
};

class StandardEngine
{
public:
    virtual void loadStartingPosition() = 0;

    virtual void loadFEN() = 0;

    virtual StandardMove computerMove() = 0;

    virtual void inputMove() = 0;
};