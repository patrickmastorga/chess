#ifndef PERFT_H
#define PERFT_H

#include <iostream>
#include <iomanip>
#include <fstream>
#include <string>
#include <chrono>
#include <stdexcept>
#include <jsoncpp/json/json.h>

#include "../chess.hpp"

// ANSI escape codes for text colors
#define RED_TEXT "\033[31m"
#define GREEN_TEXT "\033[32m"
#define RESET_TEXT "\033[0m"

// https://gist.github.com/peterellisjones/8c46c28141c162d1d8a0f0badbc9cff9

namespace Perft
{
    /**
     * Runs a full perft test tailored for testing accuracy on a given engine and prints the results out to the console
    */
    void testAccuracy(StandardEngine &engine)
    {
        // Open the JSON file
        std::ifstream file("../perft/accuracy_test_suite.json");

        // Check if the file is open
        if (!file.is_open()) {
            throw std::runtime_error("Error opening file!");
        }

        // Parse the JSON content
        Json::Value root;
        Json::CharReaderBuilder readerBuilder;
        std::string errors;

        if (!Json::parseFromStream(readerBuilder, file, &root, &errors)) {
            throw std::runtime_error("Failed to parse JSON: " + errors);
        }

        // Close the file
        file.close();

        // Check if the root is an array
        if (!root.isArray()) {
            throw std::runtime_error("Root is not an array!");
        }

        std::cout << "PERFT SUITE" << std::endl;
        auto startTotal = std::chrono::high_resolution_clock::now();

        // Loop over array elements
        for (const auto& arrayElement : root) {
            // Check if each element is an object
            if (!arrayElement.isObject()) {
                throw std::runtime_error("Array element is not an object!");
            }

            // Extract data from the object
            int depth = arrayElement["depth"].asInt();
            int nodes = arrayElement["nodes"].asInt();
            std::string fen = arrayElement["fen"].asString();

            // Display test data
            engine.loadFEN(fen);
            std::cout << "running test depth: " << depth << std::endl;
            std::cout << "position fen " << fen << std::endl;

            // Run test
            auto startPerft = std::chrono::high_resolution_clock::now();
            int out = engine.perft(depth);
            auto endPerft = std::chrono::high_resolution_clock::now();
            auto durationPerft = std::chrono::duration_cast<std::chrono::milliseconds>(endPerft - startPerft);

            // Display test result
            if (out == nodes) {
                std::cout << "result: " << GREEN_TEXT << out << RESET_TEXT << " SUCCESS" << std::endl;
            } else {
                std::cout << "result: " << RED_TEXT << out << RESET_TEXT << " FAIL" << std::endl;
            }
            std::cout << "time: " << durationPerft.count() << " millis\n" << std::endl << std::endl;
        }

        auto endTotal = std::chrono::high_resolution_clock::now();
        auto durationTotal = std::chrono::duration_cast<std::chrono::milliseconds>(endTotal - startTotal);
        std::cout << "TOTAL TIME: " << durationTotal.count() << " millis" << std::endl;
    }

    void testSpeed(StandardEngine &engine, int depth=4)
    {
        // Open the file
        std::ifstream file("../perft/preformace_test_suite.txt");

        std::cout << "PREFORMANCE TEST:" <<std::endl;
        auto startTotal = std::chrono::high_resolution_clock::now();

        std::string fen;
        while(std::getline(file, fen)) {
            // Display test data
            engine.loadFEN(fen);
            std::cout << "position fen " << fen << " depth " << depth;
            std::cout.flush();

            // Run test
            auto startPerft = std::chrono::high_resolution_clock::now();
            int out = engine.perft(depth);
            auto endPerft = std::chrono::high_resolution_clock::now();
            auto durationPerft = std::chrono::duration_cast<std::chrono::milliseconds>(endPerft - startPerft);

            // Display test result
            /*
            if (out == nodes) {
                std::cout << "result: " << GREEN_TEXT << out << RESET_TEXT << " SUCCESS" << std::endl;
            } else {
                std::cout << "result: " << RED_TEXT << out << RESET_TEXT << " FAIL" << std::endl;
            }
            */
            std::cout << " time " << durationPerft.count() << " millis" << std::endl;
        }

        auto endTotal = std::chrono::high_resolution_clock::now();
        auto durationTotal = std::chrono::duration_cast<std::chrono::milliseconds>(endTotal - startTotal);
        std::cout << std::endl << "TOTAL TIME: " << durationTotal.count() << " millis" << std::endl;
    }
}

#endif