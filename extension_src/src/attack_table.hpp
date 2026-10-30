#pragma once

#include "bitboard.hpp"
#include "shogi_utils.hpp"
#include <utility>

class AttackTable {
private:
	inline static bool is_initialized_;

	inline static Bitboard attacks_pawn_[2][Shogi::BOARD_SIZE];
	inline static Bitboard attacks_knight_[2][Shogi::BOARD_SIZE];
	inline static Bitboard attacks_silver_[2][Shogi::BOARD_SIZE];
	inline static Bitboard attacks_gold_[2][Shogi::BOARD_SIZE];
	inline static Bitboard attacks_king_[Shogi::BOARD_SIZE];

	inline static Bitboard rays_[8][Shogi::BOARD_SIZE];

	static constexpr Bitboard make_row_mask(int row) {
		Bitboard mask;
		for (int col = 0; col < Shogi::BOARD_COLS; ++col) {
			mask.set(Shogi::to_index(col, row));
		}
		return mask;
	}

public:
	static void initialize();

	static Bitboard get_ray(int index, int direction) { return rays_[direction][index]; }

	static const Bitboard &get_pawn_attacks(int index, Shogi::Turn turn) {
		return attacks_pawn_[std::to_underlying(turn)][index];
	}

	static Bitboard get_pawn_attacks(const Bitboard &pawns, Shogi::Turn turn) {
		if (turn == Shogi::Turn::SENTE) {
			constexpr Bitboard first_row = make_row_mask(0);
			const Bitboard movable = pawns & ~first_row;
			return Bitboard((movable.lower() >> 1) | (movable.upper() << 63), movable.upper() >> 1);
		}
		constexpr Bitboard last_row = make_row_mask(Shogi::BOARD_ROWS - 1);
		const Bitboard movable = pawns & ~last_row;
		return Bitboard(movable.lower() << 1, (movable.upper() << 1) | (movable.lower() >> 63)) & ~Bitboard();
	}

	static const Bitboard &get_knight_attacks(int index, Shogi::Turn turn) {
		return attacks_knight_[std::to_underlying(turn)][index];
	}

	static const Bitboard &get_silver_attacks(int index, Shogi::Turn turn) {
		return attacks_silver_[std::to_underlying(turn)][index];
	}

	static const Bitboard &get_gold_attacks(int index, Shogi::Turn turn) {
		return attacks_gold_[std::to_underlying(turn)][index];
	}

	static const Bitboard &get_king_attacks(int index) { return attacks_king_[index]; }

	static Bitboard get_lance_attacks(int index, Shogi::Turn turn, const Bitboard &occupancy);
	static Bitboard get_bishop_attacks(int index, const Bitboard &occupancy);
	static Bitboard get_rook_attacks(int index, const Bitboard &occupancy);

	static Bitboard get_promoted_bishop_attacks(int index, const Bitboard &occupancy) {
		return get_bishop_attacks(index, occupancy) | get_king_attacks(index);
	}

	static Bitboard get_promoted_rook_attacks(int index, const Bitboard &occupancy) {
		return get_rook_attacks(index, occupancy) | get_king_attacks(index);
	}

	static Bitboard get_attacks(Shogi::PieceType piece_type, bool is_promoted, int index, Shogi::Turn turn,
			const Bitboard &occupancy) {
		using enum Shogi::PieceType;
		if (is_promoted) {
			switch (piece_type) {
				case BISHOP:
					return get_promoted_bishop_attacks(index, occupancy);
				case ROOK:
					return get_promoted_rook_attacks(index, occupancy);
				default:
					return get_gold_attacks(index, turn);
			}
		}

		switch (piece_type) {
			case PAWN:
				return get_pawn_attacks(index, turn);
			case LANCE:
				return get_lance_attacks(index, turn, occupancy);
			case KNIGHT:
				return get_knight_attacks(index, turn);
			case SILVER:
				return get_silver_attacks(index, turn);
			case GOLD:
				return get_gold_attacks(index, turn);
			case BISHOP:
				return get_bishop_attacks(index, occupancy);
			case ROOK:
				return get_rook_attacks(index, occupancy);
			case KING:
				return get_king_attacks(index);
			default:
				return Bitboard();
		}
	}
};
