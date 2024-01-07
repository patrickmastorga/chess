#include <iostream>
#include <random>
#include <cstdint>

using namespace std;

typedef uint_fast64_t uint64;

int main() {
    // Create a random number generator
    random_device rd;
    mt19937_64 generator(rd());  // 64-bit Mersenne Twister engine

    // Define a distribution over the entire range of uint64_t
    uniform_int_distribution<uint64> distribution;

    uint64 randomBits;

    cout << "WHITE PEICES" << endl;

    for (int p = 0; p < 6; ++p) {
        cout << "{";
        for (int i = 0; i < 64; ++i) {
            if (i % 8 == 0) {
                cout << endl << "\t";
            }
            randomBits = distribution(generator);
            std::cout << randomBits << "ULL";
            if (i < 63) {
                cout << ", ";
            }
        }
        cout << endl << "}," << endl;
    }

    cout << "BLACK PEICES" << endl;

    for (int p = 0; p < 6; ++p) {
        cout << "{";
        for (int i = 0; i < 64; ++i) {
            if (i % 8 == 0) {
                cout << endl << "\t";
            }
            randomBits = distribution(generator);
            std::cout << randomBits << "ULL";
            if (i < 63) {
                cout << ", ";
            }
        }
        cout << endl << "}," << endl;
    }

    cout << "TURN KEY" << endl;

    randomBits = distribution(generator);
    std::cout << randomBits << "ULL" << endl;

    cout << "KINGSIDE CASTLING" << endl;

    randomBits = distribution(generator);
    std::cout << "{" << randomBits << "ULL" << ", ";
    randomBits = distribution(generator);
    std::cout << randomBits << "ULL" << "}" << endl;

    cout << "QUEENSIDE CASTLING" << endl;

    randomBits = distribution(generator);
    std::cout << "{" << randomBits << "ULL" << ", ";
    randomBits = distribution(generator);
    std::cout << randomBits << "ULL" << "}" << endl;

    cout << "EN PASSANT" << endl;

    cout << "{";
    for (int i = 0; i < 8; ++i) {
        randomBits = distribution(generator);
        std::cout << randomBits << "ULL";
        if (i < 7) {
            cout << ", ";
        }
    }
    cout << "}" << endl;

    return 0;
}
