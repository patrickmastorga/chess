#include "NNUE.h"

#include <vector>
#include <fstream>

#include <iostream>
#include <stdexcept>

#include <immintrin.h>


// WEIGHTS
alignas(4) std::int16_t NNUE::SPARSE_LINEAR_WEIGHT[INPUT_SIZE * HIDDEN_1_SIZE];

alignas(4) std::int16_t NNUE::SPARSE_LINEAR_BIAS[HIDDEN_1_SIZE];

alignas(4) std::int8_t NNUE::LINEAR_1_WEIGHT[HIDDEN_1_SIZE * HIDDEN_2_SIZE];

alignas(4) std::int32_t NNUE::LINEAR_1_BIAS[HIDDEN_2_SIZE];

alignas(4) std::int8_t NNUE::LINEAR_2_WEIGHT[HIDDEN_2_SIZE];

std::int32_t NNUE::LINEAR_2_BIAS;

NNUE::NNUE()
{
    float* buffer = new float[INPUT_SIZE * HIDDEN_1_SIZE];
    std::ifstream file;

    // Sparse linear weights
    file.open("../NNUE/model-parameters/sparse_linear.weight.bin", std::ios::binary);
    if (!file.is_open()) {
        throw std::runtime_error("Cannot find sparse linear weights");
    }
    file.read(reinterpret_cast<char*>(buffer), sizeof(float) * INPUT_SIZE * HIDDEN_1_SIZE);
    file.close();

    for (int i = 0; i < INPUT_SIZE * HIDDEN_1_SIZE; i++) {
        SPARSE_LINEAR_WEIGHT[i] = static_cast<std::uint16_t>(buffer[i] * 127);
    }


    // Sparse linear bias
    file.open("../NNUE/model-parameters/sparse_linear.bias.bin", std::ios::binary);
    if (!file.is_open()) {
        throw std::runtime_error("Cannot find sparse linear bias");
    }
    file.read(reinterpret_cast<char*>(buffer), sizeof(float) * HIDDEN_1_SIZE);
    file.close();

    for (int i = 0; i < HIDDEN_1_SIZE; i++) {
        SPARSE_LINEAR_BIAS[i] = static_cast<std::uint16_t>(buffer[i] * 127);
    }


    // Linear 1 weights
    file.open("../NNUE/model-parameters/linear1.weight.bin", std::ios::binary);
    if (!file.is_open()) {
        throw std::runtime_error("Cannot find linear 1 weight");
    }
    file.read(reinterpret_cast<char*>(buffer), sizeof(float) * HIDDEN_1_SIZE * HIDDEN_2_SIZE);
    file.close();

    for (int i = 0; i < HIDDEN_1_SIZE * HIDDEN_2_SIZE; i++) {
        LINEAR_1_WEIGHT[i] = static_cast<std::uint8_t>(buffer[i] * 64);
    }


    // Linear 1 bias
    file.open("../NNUE/model-parameters/linear1.bias.bin", std::ios::binary);
    if (!file.is_open()) {
        throw std::runtime_error("Cannot find linear 1 bias");
    }
    file.read(reinterpret_cast<char*>(buffer), sizeof(float) * HIDDEN_2_SIZE);
    file.close();

    for (int i = 0; i < HIDDEN_2_SIZE; i++) {
        LINEAR_1_BIAS[i] = static_cast<std::uint32_t>(buffer[i] * 127 * 64);
    }


    // Linear 2 weights
    file.open("../NNUE/model-parameters/linear2.weight.bin", std::ios::binary);
    if (!file.is_open()) {
        throw std::runtime_error("Cannot find linear 2 weight");
    }
    file.read(reinterpret_cast<char*>(buffer), sizeof(float) * HIDDEN_2_SIZE);
    file.close();

    for (int i = 0; i < HIDDEN_2_SIZE; i++) {
        LINEAR_1_WEIGHT[i] = static_cast<std::uint8_t>(buffer[i]);
    }


    // Linear 2 bias
    file.open("../NNUE/model-parameters/linear2.bias.bin", std::ios::binary);
    if (!file.is_open()) {
        throw std::runtime_error("Cannot find linear 2 bias");
    }
    file.read(reinterpret_cast<char*>(buffer), sizeof(float));
    file.close();

    LINEAR_2_BIAS = static_cast<std::uint32_t>(buffer[0] * 127);

    delete[] buffer;
}

void NNUE::refreshAccumulator(Accumulator& output, std::vector<std::int_fast16_t>& activeFeatures)
{
    __m256i accumulator = _mm256_load_si256((__m256i*)SPARSE_LINEAR_BIAS);

    // Add the weights (vector by vector) for the active features
    for (std::int_fast16_t a : activeFeatures) {
        accumulator = _mm256_add_epi16(accumulator, _mm256_load_si256((__m256i*)&SPARSE_LINEAR_WEIGHT[a * HIDDEN_1_SIZE]));
    }

    _mm256_store_si256((__m256i*)output.vec, accumulator);
}

void NNUE::updateAccumulator(Accumulator& input, Accumulator& output, std::vector<std::int_fast16_t>& removedFeatures, std::vector<std::int_fast16_t>& addedFeatures)
{
    __m256i accumulator = _mm256_load_si256((__m256i*)input.vec);

    // Subtract the weights (vector by vector) for the removed features
    for (std::int_fast16_t r : removedFeatures) {
        accumulator = _mm256_sub_epi16(accumulator, _mm256_load_si256((__m256i*)&SPARSE_LINEAR_WEIGHT[r * HIDDEN_1_SIZE]));
    }

    // Add the weights (vector by vector) for the added features
    for (std::int_fast16_t a : addedFeatures) {
        accumulator = _mm256_add_epi16(accumulator, _mm256_load_si256((__m256i*)&SPARSE_LINEAR_WEIGHT[a * HIDDEN_1_SIZE]));
    }

    _mm256_store_si256((__m256i*)output.vec, accumulator);
}

std::int_fast32_t NNUE::foward(Accumulator input)
{
    // Hidden layer 1
    alignas(4) std::int8_t hidden1[HIDDEN_1_SIZE];

    // Hidden layer 2
    alignas(4) std::int32_t temp[HIDDEN_2_SIZE];
    alignas(4) std::int8_t hidden2[HIDDEN_2_SIZE];

    // Foward pass
    crelu(input, hidden1);
    linear1(hidden1, temp);
    crelu(temp, hidden1);
    return linear2(hidden2);
}

void NNUE::crelu(Accumulator& input, std::int8_t* output)
{
    // Split input into two halves
    __m128i in0 = _mm_load_si128((__m128i*)&input.vec[0]);
    __m128i in1 = _mm_load_si128((__m128i*)&input.vec[HIDDEN_1_SIZE / 2]);
    
    // Compute the clamping (packs clamps from above, max clamps from below) and store result
    __m128i result = _mm_packs_epi16(in0, in1);
    result = _mm_max_epi8(result, _mm_setzero_si128());

    _mm_store_si128((__m128i*)output, result);
}

void NNUE::linear1(std::int8_t* input, std::int32_t* output)
{
    // Input vector
    __m128i vec = _mm_load_si128((__m128i*)input);

    __m128i zero = _mm_setzero_si128();
    
    for (int i = 0; i < HIDDEN_2_SIZE / 4; i++) {

        // Each sum reg has the dot product split into 4 32 bit ints
        __m128i sum0 = _mm_dpbusd_epi32(zero, vec, _mm_load_si128((__m128i*)&LINEAR_1_WEIGHT[(i * 4 + 0) * HIDDEN_1_SIZE]));
        __m128i sum1 = _mm_dpbusd_epi32(zero, vec, _mm_load_si128((__m128i*)&LINEAR_1_WEIGHT[(i * 4 + 1) * HIDDEN_1_SIZE]));
        __m128i sum2 = _mm_dpbusd_epi32(zero, vec, _mm_load_si128((__m128i*)&LINEAR_1_WEIGHT[(i * 4 + 2) * HIDDEN_1_SIZE]));
        __m128i sum3 = _mm_dpbusd_epi32(zero, vec, _mm_load_si128((__m128i*)&LINEAR_1_WEIGHT[(i * 4 + 3) * HIDDEN_1_SIZE]));

        sum0 = _mm_hadd_epi32(sum0, sum1);
        sum2 = _mm_hadd_epi32(sum2, sum3);

        // sum0 contains the dot products of the input to the four rows being processed (as int32)
        sum0 = _mm_hadd_epi32(sum0, sum2);

        // Apply bias, divide by scaling factor, and store result
        sum0 = _mm_add_epi32(sum0, _mm_load_si128((__m128i*)&LINEAR_1_BIAS[i * 4]));
        sum0 = _mm_srai_epi32(sum0, 6);
        _mm_store_si128((__m128i*)&output[i * 4], sum0);
    }
}

void NNUE::crelu(std::int32_t* input, std::int8_t* output)
{
    // Split input into quarters
    __m128i in0 = _mm_load_si128((__m128i*) & input[0]);
    __m128i in1 = _mm_load_si128((__m128i*) & input[HIDDEN_1_SIZE / 4]);
    __m128i in2 = _mm_load_si128((__m128i*) & input[HIDDEN_1_SIZE / 2]);
    __m128i in3 = _mm_load_si128((__m128i*) & input[HIDDEN_1_SIZE * 3 / 4]);

    in0 = _mm_packs_epi32(in0, in1);
    in2 = _mm_packs_epi32(in2, in3);

    // Compute the clamping (packs clamps from above, max clamps from below) and store result
    __m128i result = _mm_packs_epi16(in0, in2);
    result = _mm_max_epi8(result, _mm_setzero_si128());

    _mm_store_si128((__m128i*)output, result);
}

std::int_fast32_t NNUE::linear2(std::int8_t* input)
{
    // Input and weight vectors
    __m128i vec = _mm_load_si128((__m128i*)input);
    __m128i weights = _mm_load_si128((__m128i*)LINEAR_1_WEIGHT);

    // Compute dot product of two vectors
    __m128i dot = _mm_maddubs_epi16(vec, weights);
    dot = _mm_hadd_epi16(dot, dot);
    dot = _mm_hadd_epi32(dot, dot);

    std::int_fast32_t out = _mm_extract_epi32(dot, 0) + LINEAR_2_BIAS;

    // Divide output by 128 to account for scaling
    return out >> 7;
}
