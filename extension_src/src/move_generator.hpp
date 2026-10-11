#pragma once

#include "board_state.hpp"

struct PinMasks {
	// ピンされている味方駒
	Bitboard pinned;

	// ピンされた駒の移動可能範囲
	Bitboard valid_ray_masks[Shogi::BOARD_SIZE];
};

class MoveGenerator {
private:
	static Bitboard get_checkers(const BoardState &board, Shogi::Turn turn);
	static PinMasks calculate_pin_masks(const BoardState &board, Shogi::Turn turn);
	[[nodiscard]] static Bitboard attackers_to(const BoardState &board, int index, Shogi::Turn turn,
			const Bitboard &occupancy);
	[[nodiscard]] static bool is_nifu(const BoardState &board, Shogi::PieceType piece_type, Shogi::Turn turn, int col);
	[[nodiscard]] static bool is_uchifuzume(BoardState &board, Shogi::Coord to);

public:
	[[nodiscard]] static bool is_dead_end(Shogi::PieceType piece_type, bool is_gote, int to_row);
	[[nodiscard]] static bool can_promote(Shogi::PieceType piece_type, bool is_promoted, bool is_gote, int from_row, int to_row);
	[[nodiscard]] static bool is_king_in_check(const BoardState &board, Shogi::Turn turn);
	[[nodiscard]] static int see(const BoardState &board, const Shogi::Move &move);
	static void get_legal_moves(BoardState &board, Shogi::MoveList &move_list, bool only_captures = false);
};
