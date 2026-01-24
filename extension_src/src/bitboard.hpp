#ifndef BITBOARD_HPP
#define BITBOARD_HPP

#include "shogi_utils.hpp"
#include <cstdint>
#include <godot_cpp/variant/utility_functions.hpp>
#include <string>

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

    Bitboard operator~() const { return Bitboard(~lower_, ~upper_); }

    bool operator==(const Bitboard &rhs) const { return lower_ == rhs.lower_ && upper_ == rhs.upper_; }

    bool operator!=(const Bitboard &rhs) const { return !(*this == rhs); }

    int lsb() const {
        if (lower_ != 0) {
            unsigned long index;
#ifdef _MSC_VER
            _BitScanForward64(&index, lower_);
#else
            index = __builtin_ctzll(lower_);
#endif
            return static_cast<int>(index);
        }
        if (upper_ != 0) {
            unsigned long index;
#ifdef _MSC_VER
            _BitScanForward64(&index, upper_);
#else
            index = __builtin_ctzll(upper_);
#endif
            return static_cast<int>(index) + 64;
        }
        return -1;
    }

    int msb() const {
        if (upper_ != 0) {
            unsigned long index;
#ifdef _MSC_VER
            _BitScanReverse64(&index, upper_);
#else
            index = 63 - __builtin_clzll(upper_);
#endif
            return static_cast<int>(index) + 64;
        }
        if (lower_ != 0) {
            unsigned long index;
#ifdef _MSC_VER
            _BitScanReverse64(&index, lower_);
#else
            index = 63 - __builtin_clzll(lower_);
#endif
            return static_cast<int>(index);
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

    void print() const {
        using namespace godot;
        String output = "\n------------------\n";
        for (int row = 0; row < Shogi::BOARD_ROWS; ++row) {
            String line = "";
            for (int col = 0; col < Shogi::BOARD_COLS; ++col) {
                int index = col * Shogi::BOARD_ROWS + row;
                if (is_set(index)) {
                    line += "1 ";
                } else {
                    line += ". ";
                }
            }
            output += line + "\n";
        }
        UtilityFunctions::print(output);
    }
};

#endif
