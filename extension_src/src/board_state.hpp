#pragma once

#include "bitboard.hpp"
#include "evaluator.hpp"
#include "shogi_utils.hpp"
#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <utility>

struct Cell {
	Shogi::PieceType piece_type;
	Shogi::Turn turn;
	bool is_promoted;

	[[nodiscard]] bool is_empty() const { return piece_type == Shogi::PieceType::EMPTY; }

	Cell() : piece_type(Shogi::PieceType::EMPTY), turn(Shogi::Turn::SENTE), is_promoted(false) {}
	Cell(Shogi::PieceType piece_type, Shogi::Turn turn, bool is_promoted) : piece_type(piece_type), turn(turn), is_promoted(is_promoted) {}

	[[nodiscard]] bool operator==(const Cell &other) const {
		if (piece_type == Shogi::PieceType::EMPTY && other.piece_type == Shogi::PieceType::EMPTY) {
			return true;
		}

		return piece_type == other.piece_type && turn == other.turn && is_promoted == other.is_promoted;
	}
};

class BoardState {
	friend class MoveGenerator;
	friend class Evaluator;

private:
	// すべての駒のBitboard
	Bitboard bitboard_all_;

	// 手番別のBitboard
	Bitboard bitboard_side_[2];

	// 手番別・駒種別のBitboard
	Bitboard bitboard_piece_[2][Shogi::PIECE_TYPE_COUNT];

	// 手番別の成り駒のBitboard
	Bitboard bitboard_promoted_[2];

	Cell board_[Shogi::BOARD_SIZE];
	int hand_[2][Shogi::PIECE_TYPE_COUNT];

	Shogi::Turn turn_to_move_;
	uint64_t zobrist_hash_;
	int score_;
	std::optional<Shogi::Coord> king_position_[2];
	uint16_t pawn_columns_[2];

	void build_bitboard();
	void add_piece_to_bitboard(int index, Shogi::Turn turn, Shogi::PieceType piece_type, bool is_promoted);
	void remove_piece_from_bitboard(int index, Shogi::Turn turn, Shogi::PieceType piece_type, bool is_promoted);

	[[nodiscard]] uint64_t calculate_zobrist_hash() const;

	void update_king_position_cache();
	void update_pawn_columns_cache();

	bool parse_sfen(const std::string &sfen);

public:
	BoardState(Shogi::Turn turn_to_move = Shogi::Turn::SENTE);
	[[nodiscard]] static std::optional<BoardState> from_sfen(const std::string &sfen);

	[[nodiscard]] Shogi::Turn get_turn_to_move() const { return turn_to_move_; }
	[[nodiscard]] int get_score() const { return score_ + Evaluator::calculate_spatial_score(*this); }

	[[nodiscard]] bool operator==(const BoardState &other) const {
		if (turn_to_move_ != other.turn_to_move_) {
			return false;
		}

		for (int i = 0; i < Shogi::BOARD_SIZE; ++i) {
			if (board_[i] != other.board_[i]) {
				return false;
			}
		}

		for (Shogi::Turn turn : { Shogi::Turn::SENTE, Shogi::Turn::GOTE }) {
			for (int piece_type_index = 0; piece_type_index < Shogi::PIECE_TYPE_COUNT; ++piece_type_index) {
				if (hand_[std::to_underlying(turn)][piece_type_index] != other.hand_[std::to_underlying(turn)][piece_type_index]) {
					return false;
				}
			}
		}

		return true;
	}

	static bool load_zobrist_params(std::span<const uint8_t> data);
	[[nodiscard]] static bool zobrist_initialized();

	[[nodiscard]] uint64_t get_zobrist_hash() const;

	[[nodiscard]] std::optional<Shogi::Coord> get_king_position(Shogi::Turn turn) const;

	// 盤面の操作
	[[nodiscard]] const Cell &get_cell(Shogi::Coord coord) const;
	void set_cell(Shogi::Coord coord, Shogi::PieceType piece_type, Shogi::Turn turn, bool is_promoted);
	void clear_cell(Shogi::Coord coord);
	[[nodiscard]] int get_hand_count(Shogi::Turn turn, Shogi::PieceType piece_type) const;
	void set_hand_count(Shogi::Turn turn, Shogi::PieceType piece_type, int count);
	[[nodiscard]] Shogi::UndoInfo apply_move(const Shogi::Move &move);
	void undo_move(const Shogi::UndoInfo &undo_info);

	// Null Move Pruning用
	uint64_t make_null_move();
	void undo_null_move(uint64_t prev_hash);
};
