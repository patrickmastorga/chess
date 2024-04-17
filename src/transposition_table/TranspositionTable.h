#pragma once
#include <cstdint>


class TranspositionTable
{
public:
	struct Entry
	{
		// | 5 bits depth (MAX 31!!) | 3 bits eval_type | 24 bits key (most significant 24 from hash) |
		std::uint32_t info;

		// Eval
		std::int16_t eval;

		// Best move | 8 bits start square | 8 bits target square |
		std::uint16_t move;

		static constexpr std::uint32_t EXACT_VALUE = (1 << 24);
		static constexpr std::uint32_t LOWER_BOUND = (1 << 25);
		static constexpr std::uint32_t UPPER_BOUND = (1 << 26);

		std::uint_fast8_t depth() const;

		bool isHit(std::uint_fast64_t zobrist) const;

		Entry();

		Entry(std::uint_fast64_t zobrist, std::uint_fast8_t depth, std::int16_t eval, std::uint32_t evalType, std::uint_fast8_t start, std::uint_fast8_t target);
	};

	static constexpr std::uint_fast64_t NUM_ENTRIES = (1 << 20);

	// Construct a new transposition table
	TranspositionTable();

	// Clear the entries in the transposition table
	void clear();

	Entry getEntry(std::uint_fast64_t zobrist);

	void storeEntry(Entry entry, std::uint_fast64_t zobrist);

private:
	Entry entries[NUM_ENTRIES];
};