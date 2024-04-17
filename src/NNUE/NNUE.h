#pragma once

#include <cstdint>
#include <vector>

class NNUE
{
public:
	static constexpr int INPUT_SIZE = 768;
	static constexpr int HIDDEN_1_SIZE = 16;
	static constexpr int HIDDEN_2_SIZE = 16;

	NNUE();

	struct alignas(4) Accumulator
	{
		std::int16_t vec[NNUE::HIDDEN_1_SIZE];
	};

	// Generate new accumulator from active features
	void refreshAccumulator(Accumulator& output, std::vector<std::uint_fast16_t>& activeFeatures);

	// Update the accumulator for a move
	void updateAccumulatorMove   (Accumulator& input, Accumulator& output, std::uint_fast16_t rem1, std::uint_fast16_t add1);
	void updateAccumulatorCapture(Accumulator& input, Accumulator& output, std::uint_fast16_t rem1, std::uint_fast16_t rem2, std::uint_fast16_t add1);

	// Evaluates the neural network and returns the evaluation
	std::int_fast32_t foward(Accumulator &input);

private:
	// Clamps every entry to [0, 127] in the input array and puts the result in the output array
	void crelu(Accumulator& input, std::int8_t* output);

	// Evaluates the first linear layer and outputs the result in the output array
	void linear1(std::int8_t* input, std::int32_t* output);

	// Clamps every entry to [0, 127] in the input array and puts the result in the output array
	void crelu(std::int32_t* input, std::int8_t* output);

	// Evaluates the second layer and outputs the result into the output
	std::int_fast32_t linear2(std::int8_t* input);


	// WEIGHTS
	static alignas(4) std::int16_t SPARSE_LINEAR_WEIGHT[INPUT_SIZE * HIDDEN_1_SIZE];

	static alignas(4) std::int16_t SPARSE_LINEAR_BIAS[HIDDEN_1_SIZE];

	static alignas(4) std::int8_t LINEAR_1_WEIGHT[HIDDEN_1_SIZE * HIDDEN_2_SIZE];

	static alignas(4) std::int32_t LINEAR_1_BIAS[HIDDEN_2_SIZE];

	static alignas(4) std::int8_t LINEAR_2_WEIGHT[HIDDEN_2_SIZE];

	static std::int_fast32_t LINEAR_2_BIAS;
};
