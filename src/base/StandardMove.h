#pragma once
#include <iostream>
/**
 * Class for represening a chess move
 */
class StandardMove
{
public:
    /**
     * Starting square of the move [0, 63] -> [a1, h8]
     */
    int startSquare;

    /**
     * Ending square of the move [0, 63] -> [a1, h8]
     */
    int targetSquare;

    /**
     * In case of promotion, what is the indentity of the promoted peice
     * 0 - none; 2 - knight; 3 - bishop; 4 - rook; 5 - queen
     */
    int promotion;

    /**
     * Constructor for Standard Move
    */
    StandardMove(int start, int target, int promotion = 0);

    StandardMove();

    /**
     * Override equality operator
     */
    bool operator==(const StandardMove& other) const;

    /**
     * Override stream insertion operator to display info about the move
     */
    friend std::ostream& operator<<(std::ostream& os, const StandardMove& obj);
};
