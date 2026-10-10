#pragma once

#include <bit>
#include <cstdint>

struct Bitboard {
private:
	// lower_: 0～63番目のマス（1筋から7筋の途中まで）
	// upper_: 64～80番目のマス（7筋の途中から9筋まで）
	uint64_t lower_;
	uint64_t upper_;

public:
	Bitboard() : lower_(0), upper_(0) {}
	constexpr Bitboard(uint64_t lower, uint64_t upper) : lower_(lower), upper_(upper) {}

	Bitboard operator&=(const Bitboard &rhs) {
		lower_ &= rhs.lower_;
		upper_ &= rhs.upper_;
		return *this;
	}

	Bitboard operator|=(const Bitboard &rhs) {
		lower_ |= rhs.lower_;
		upper_ |= rhs.upper_;
		return *this;
	}

	Bitboard operator^=(const Bitboard &rhs) {
		lower_ ^= rhs.lower_;
		upper_ ^= rhs.upper_;
		return *this;
	}

	Bitboard operator&(const Bitboard &rhs) const { return Bitboard(lower_ & rhs.lower_, upper_ & rhs.upper_); }

	Bitboard operator|(const Bitboard &rhs) const { return Bitboard(lower_ | rhs.lower_, upper_ | rhs.upper_); }

	Bitboard operator^(const Bitboard &rhs) const { return Bitboard(lower_ ^ rhs.lower_, upper_ ^ rhs.upper_); }

	Bitboard operator~() const {
		// ビットを反転したあとに盤面の範囲外のビットをマスク
		constexpr uint64_t upper_mask = (1ULL << 17) - 1;
		return Bitboard(~lower_, ~upper_ & upper_mask);
	}

	bool operator==(const Bitboard &rhs) const { return lower_ == rhs.lower_ && upper_ == rhs.upper_; }

	bool operator!=(const Bitboard &rhs) const { return !(*this == rhs); }

	int lsb() const {
		if (lower_ != 0) {
			return std::countr_zero(lower_);
		}
		if (upper_ != 0) {
			return std::countr_zero(upper_) + 64;
		}
		return -1;
	}

	int msb() const {
		if (upper_ != 0) {
			return 127 - std::countl_zero(upper_);
		}
		if (lower_ != 0) {
			return 63 - std::countl_zero(lower_);
		}
		return -1;
	}

	void set(int index) {
		if (index < 64) {
			lower_ |= (1ULL << index);
		} else {
			upper_ |= (1ULL << (index - 64));
		}
	}

	void clear(int index) {
		if (index < 64) {
			lower_ &= ~(1ULL << index);
		} else {
			upper_ &= ~(1ULL << (index - 64));
		}
	}

	bool is_set(int index) const {
		if (index < 64) {
			return (lower_ & (1ULL << index)) != 0;
		} else {
			return (upper_ & (1ULL << (index - 64))) != 0;
		}
	}

	bool is_empty() const { return lower_ == 0 && upper_ == 0; }

	int count() const {
		return std::popcount(lower_) + std::popcount(upper_);
	}
};
