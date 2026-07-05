#pragma once

#include "board_state.hpp"

class MoveGenerator {
private:
	static Bitboard get_checkers(const BoardState &board, Shogi::Turn turn);
	static PinMasks calculate_pin_masks(const BoardState &board, Shogi::Turn turn);
	[[nodiscard]] static Bitboard attackers_to(const BoardState &board, int square, Shogi::Turn side,
			const Bitboard &occupancy);
	[[nodiscard]] static bool is_valid_move(const BoardState &board, Shogi::Coord from, Shogi::Coord to);
	[[nodiscard]] static bool is_valid_drop(const BoardState &board, Shogi::PieceType piece_type, bool is_enemy,
			Shogi::Coord to);
	[[nodiscard]] static bool is_nifu(const BoardState &board, Shogi::PieceType piece_type, Shogi::Turn turn, int col);
	[[nodiscard]] static bool is_uchifuzume(BoardState &board, Shogi::Turn turn, Shogi::Coord to);

public:
	[[nodiscard]] static bool is_legal_move(BoardState &board, Shogi::Coord from, Shogi::Coord to);
	[[nodiscard]] static bool is_legal_drop(BoardState &board, Shogi::PieceType piece_type, bool is_enemy,
			Shogi::Coord to);
	[[nodiscard]] static bool is_dead_end(Shogi::PieceType piece_type, bool is_enemy, int to_row);
	[[nodiscard]] static bool is_king_in_check(const BoardState &board, Shogi::Turn turn);
	[[nodiscard]] static int see(const BoardState &board, const Shogi::Move &move);
	static void get_legal_moves(BoardState &board, Shogi::MoveList &move_list, bool only_captures = false);
};
