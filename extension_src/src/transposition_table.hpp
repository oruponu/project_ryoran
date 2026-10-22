#pragma once

#include "shogi_utils.hpp"
#include <cstddef>
#include <cstdint>
#include <optional>
#include <vector>

enum class TTFlag : uint8_t {
	EXACT,
	LOWER_BOUND,
	UPPER_BOUND
};

struct TTData {
	int score;
	int depth;
	TTFlag flag;
	uint16_t move;
};

class TranspositionTable {
	struct Entry {
		uint64_t key;
		int32_t score;
		uint16_t move;
		uint8_t depth; // depth + 1; 0 means empty
		uint8_t gen_flag; // upper 6 bits: generation, lower 2 bits: flag
	};

	static constexpr int BUCKET_SIZE = 4;
	static constexpr int GENERATION_MASK = 63;

	struct alignas(64) Bucket {
		Entry entries[BUCKET_SIZE];
	};

	static_assert(sizeof(Entry) == 16);
	static_assert(sizeof(Bucket) == 64);

	std::vector<Bucket> buckets_;
	uint64_t bucket_mask_ = 0;
	uint8_t generation_ = 0;

	[[nodiscard]] int age(const Entry &entry) const;

public:
	void allocate(size_t bytes);
	[[nodiscard]] std::optional<TTData> probe(uint64_t key) const;
	void store(uint64_t key, int score, int depth, TTFlag flag, uint16_t move);
	void new_search();
	[[nodiscard]] int hashfull() const;
};

[[nodiscard]] uint16_t pack_move(const Shogi::Move &move);
