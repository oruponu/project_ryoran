#pragma once

#include "board_state.hpp"

struct PinMasks {
	// ピンされている味方駒
	Bitboard pinned;

	// ピンされた駒の移動可能範囲
	Bitboard valid_ray_masks[Shogi::BOARD_SIZE];
};

struct CheckInfo {
	bool has_opponent_king = false;
	Bitboard check_squares[2][Shogi::PIECE_TYPE_COUNT];
	Bitboard discovered_candidates;
	Bitboard discovered_lines[8];
};

class MoveGenerator {
private:
	static PinMasks calculate_pin_masks(const BoardState &board, Shogi::Turn turn);
	[[nodiscard]] static Bitboard attackers_to(const BoardState &board, int index, Shogi::Turn turn,
			const Bitboard &occupancy);
	[[nodiscard]] static bool is_nifu(const BoardState &board, Shogi::PieceType piece_type, Shogi::Turn turn, int col);
	[[nodiscard]] static bool is_uchifuzume(BoardState &board, Shogi::Coord to);

public:
	[[nodiscard]] static bool is_dead_end(Shogi::PieceType piece_type, Shogi::Turn turn, int to_row);
	[[nodiscard]] static bool can_promote(Shogi::PieceType piece_type, bool is_promoted, Shogi::Turn turn, int from_row, int to_row);
	[[nodiscard]] static Bitboard get_checkers(const BoardState &board, Shogi::Turn turn);
	[[nodiscard]] static bool is_king_in_check(const BoardState &board, Shogi::Turn turn);
	[[nodiscard]] static CheckInfo get_check_info(const BoardState &board);
	[[nodiscard]] static bool gives_check(const BoardState &board, const CheckInfo &check_info, const Shogi::Move &move);
	[[nodiscard]] static int see(const BoardState &board, const Shogi::Move &move);
	static void get_legal_moves(BoardState &board, Shogi::MoveList &move_list, bool only_captures = false);
	static void get_legal_moves(BoardState &board, Shogi::MoveList &move_list, bool only_captures, const Bitboard &checkers);
};
