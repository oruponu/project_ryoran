#include "transposition_table.hpp"
#include <algorithm>
#include <bit>
#include <utility>

namespace {

constexpr int HASHFULL_SAMPLE_SIZE = 1000;
constexpr int MOVE_FROM_SHIFT = 7;
constexpr int MOVE_PROMOTION_BIT = 1 << 14;

} // namespace

void TranspositionTable::allocate(size_t bytes) {
	if (!buckets_.empty()) {
		return;
	}
	size_t count = std::bit_floor(std::max<size_t>(bytes / sizeof(Bucket), 1));
	buckets_.resize(count);
	bucket_mask_ = count - 1;
}

int TranspositionTable::age(const Entry &entry) const {
	return (generation_ - (entry.gen_flag >> 2)) & GENERATION_MASK;
}

std::optional<TTData> TranspositionTable::probe(uint64_t key) const {
	const Bucket &bucket = buckets_[key & bucket_mask_];
	for (const Entry &entry : bucket.entries) {
		if (entry.depth != 0 && entry.key == key) {
			return TTData{ .score = entry.score, .depth = entry.depth - 1, .flag = static_cast<TTFlag>(entry.gen_flag & 3), .move = entry.move };
		}
	}
	return std::nullopt;
}

void TranspositionTable::store(uint64_t key, int score, int depth, TTFlag flag, uint16_t move) {
	Bucket &bucket = buckets_[key & bucket_mask_];
	const Entry new_entry{ .key = key,
		.score = score,
		.move = move,
		.depth = static_cast<uint8_t>(depth + 1),
		.gen_flag = static_cast<uint8_t>((generation_ << 2) | std::to_underlying(flag)) };

	for (Entry &entry : bucket.entries) {
		if (entry.depth != 0 && entry.key == key) {
			if (new_entry.depth >= entry.depth || age(entry) != 0) {
				entry = new_entry;
			}
			return;
		}
	}

	Entry *victim = nullptr;
	for (Entry &entry : bucket.entries) {
		if (entry.depth == 0) {
			victim = &entry;
			break;
		}
		if (victim == nullptr || age(entry) > age(*victim) || (age(entry) == age(*victim) && entry.depth < victim->depth)) {
			victim = &entry;
		}
	}
	*victim = new_entry;
}

void TranspositionTable::new_search() {
	generation_ = static_cast<uint8_t>((generation_ + 1) & GENERATION_MASK);
}

int TranspositionTable::hashfull() const {
	int sampled = 0;
	int used = 0;
	for (const Bucket &bucket : buckets_) {
		for (const Entry &entry : bucket.entries) {
			if (sampled == HASHFULL_SAMPLE_SIZE) {
				return used * 1000 / sampled;
			}
			++sampled;
			if (entry.depth != 0 && age(entry) == 0) {
				++used;
			}
		}
	}
	return sampled == 0 ? 0 : used * 1000 / sampled;
}

uint16_t pack_move(const Shogi::Move &move) {
	int to = Shogi::to_index(move.to_col, move.to_row);
	int from = move.is_drop ? Shogi::BOARD_SIZE + std::to_underlying(move.piece_type) : Shogi::to_index(move.from_col, move.from_row);
	return static_cast<uint16_t>(to | (from << MOVE_FROM_SHIFT) | (move.is_promotion ? MOVE_PROMOTION_BIT : 0));
}
