#pragma once

#include <iostream>
#include <chrono>
#include "PerftTestableEngine.h"

namespace perft
{
    /**
     * Runs a full perft test tailored for testing accuracy on a given engine and prints the results out to the console
    */
    void testAccuracy(PerftTestableEngine& engine);

    /**
     * Runs a full perft test tailored for testing speed on a given engine and prints the results out to the console
    */
    void testSpeed(PerftTestableEngine& engine, int depth = 4);

    void testSearchEfficiency(PerftTestableEngine& engine, int depth, int numTests);

    void testSearchSpeed(PerftTestableEngine& engine, std::chrono::milliseconds thinkTime, int numTests);
}