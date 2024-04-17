#include "TranspositionTable.h"
#include <cstdint>


std::uint_fast8_t TranspositionTable::Entry::depth() const
{
	return static_cast<std::uint_fast8_t>(info >> 27);
}

bool TranspositionTable::Entry::isHit(std::uint_fast64_t zobrist) const
{
	static constexpr std::uint32_t key_mask = (1 << 24) - 1;
	return info && (static_cast<std::uint32_t>(zobrist >> 40) == (info & key_mask));
}

TranspositionTable::Entry::Entry() : info(0), eval(0), move(0) {}

TranspositionTable::Entry::Entry(std::uint_fast64_t zobrist, std::uint_fast8_t depth, std::int16_t eval, std::uint32_t evalType, std::uint_fast8_t start, std::uint_fast8_t target) : eval(eval)
{
	move = (static_cast<std::uint16_t>(start) << 8) | target;
	info = (static_cast<std::uint32_t>(depth) << 27) | evalType | static_cast<std::uint32_t>(zobrist >> 40);
}

TranspositionTable::TranspositionTable()
{
	clear();
}

void TranspositionTable::clear()
{
	// Reset each entry to zero
	for (int i = 0; i < NUM_ENTRIES; i++) {
		entries[i] = Entry();
	}
}

TranspositionTable::Entry TranspositionTable::getEntry(std::uint_fast64_t zobrist)
{
	return entries[zobrist % NUM_ENTRIES];
}

void TranspositionTable::storeEntry(Entry entry, std::uint_fast64_t zobrist)
{
	entries[zobrist % NUM_ENTRIES] = entry;
}
