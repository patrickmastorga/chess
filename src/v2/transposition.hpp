#include <cstdint>

#include "board.hpp"

typedef std::uint_fast8_t uint8;
typedef std::int_fast8_t int8;
typedef std::uint_fast16_t uint16;
typedef std::int_fast16_t int16;
typedef std::uint_fast32_t uint32;
typedef std::uint_fast64_t uint64;


class TranspositionTable
{
    /**
     * Condensed version of a move
     */
    struct EncodedMove
    {
        /**
         * Contains data about hte move including the start square and the end square
         */
        uint16 data;

        /**
         * Constructs an encoded move from a given start and end square [0, 63] -> [a1, h8]
         */
        EncodedMove(uint8 start, uint8 target) : data(static_cast<uint16>(start) << 8 + target) {}
    };
};