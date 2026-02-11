#pragma once

#include "bitboard.hpp"
#include "shogi_utils.hpp"

class AttackTable {
  private:
    inline static bool initialized_;

    inline static Bitboard attacks_pawn_[2][Shogi::BOARD_SIZE];
    inline static Bitboard attacks_knight_[2][Shogi::BOARD_SIZE];
    inline static Bitboard attacks_silver_[2][Shogi::BOARD_SIZE];
    inline static Bitboard attacks_gold_[2][Shogi::BOARD_SIZE];
    inline static Bitboard attacks_king_[Shogi::BOARD_SIZE];

    inline static Bitboard rays_[8][Shogi::BOARD_SIZE];

  public:
    static void initialize();

    static Bitboard get_ray(int square, int direction) { return rays_[direction][square]; }

    static const Bitboard &get_pawn_attacks(int square, Shogi::Turn turn) {
        return attacks_pawn_[static_cast<int>(turn)][square];
    }

    static const Bitboard &get_knight_attacks(int square, Shogi::Turn turn) {
        return attacks_knight_[static_cast<int>(turn)][square];
    }

    static const Bitboard &get_silver_attacks(int square, Shogi::Turn turn) {
        return attacks_silver_[static_cast<int>(turn)][square];
    }

    static const Bitboard &get_gold_attacks(int square, Shogi::Turn turn) {
        return attacks_gold_[static_cast<int>(turn)][square];
    }

    static const Bitboard &get_king_attacks(int square) { return attacks_king_[square]; }

    static Bitboard get_lance_attacks(int square, Shogi::Turn turn, const Bitboard &occupancy);
    static Bitboard get_bishop_attacks(int square, const Bitboard &occupancy);
    static Bitboard get_rook_attacks(int square, const Bitboard &occupancy);

    static Bitboard get_promoted_bishop_attacks(int square, const Bitboard &occupancy) {
        return get_bishop_attacks(square, occupancy) | get_king_attacks(square);
    }

    static Bitboard get_promoted_rook_attacks(int square, const Bitboard &occupancy) {
        return get_rook_attacks(square, occupancy) | get_king_attacks(square);
    }
};
