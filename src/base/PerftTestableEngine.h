#pragma once
#include "StandardEngine.h"
#include <cstdint>
#include <chrono>

class PerftTestableEngine : public StandardEngine
{
public:
    /**
     * Runs a move generation test on the current position
     * @param printOut if set true, prints a per move readout to the console
     * @return number of total positions a certain depth away
     */
    virtual std::uint64_t perft(int depth, bool printOut = false) noexcept = 0;

    virtual std::uint64_t search_perft(int depth) noexcept = 0;

    virtual std::uint64_t search_perft(std::chrono::milliseconds thinkTime) noexcept = 0;
};