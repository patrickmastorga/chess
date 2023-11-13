#include <cstdint>

typedef std::uint_fast8_t uint8;
typedef std::int_fast8_t int8;
typedef std::uint_fast16_t uint16;
typedef std::int_fast16_t int16;
typedef std::uint_fast32_t uint32;
typedef std::uint_fast64_t uint64;


class TranspositionTable
{
    struct EncodedMove
    {
        uint16 data;
    };
};