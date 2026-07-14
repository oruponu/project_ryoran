#include "shogi_engine.hpp"
#include "ai_player.hpp"
#include "move_generator.hpp"
#include <godot_cpp/classes/file_access.hpp>
#include <godot_cpp/classes/time.hpp>
#include <godot_cpp/core/class_db.hpp>
#include <godot_cpp/variant/utility_functions.hpp>
#include <string>

using namespace godot;
using Shogi::Coord;
using Shogi::Move;
using Shogi::PieceType;
using Shogi::Turn;

ShogiEngine::ShogiEngine() {
	if (!is_initialized_) {
		std::srand(Time::get_singleton()->get_ticks_usec());

		BoardState::load_zobrist_params("res://assets/data/zobrist_params.bin");
		load_book_from_file("res://assets/data/book.bin");

		is_initialized_ = true;
	}
}

void ShogiEngine::_bind_methods() {
	ClassDB::bind_method(D_METHOD("get_legal_moves", "sfen", "col", "row"), &ShogiEngine::get_legal_moves);
	ClassDB::bind_method(D_METHOD("get_legal_drops", "sfen", "piece_type"), &ShogiEngine::get_legal_drops);
	ClassDB::bind_method(D_METHOD("is_king_in_check", "sfen", "is_enemy"), &ShogiEngine::is_king_in_check);
	ClassDB::bind_method(D_METHOD("has_any_legal_move", "sfen"), &ShogiEngine::has_any_legal_move);
	ClassDB::bind_method(D_METHOD("get_position_hash", "sfen"), &ShogiEngine::get_position_hash);
	ClassDB::bind_method(D_METHOD("is_dead_end", "piece_type", "is_enemy", "to_row"), &ShogiEngine::is_dead_end);
	ClassDB::bind_method(D_METHOD("can_promote", "piece_type", "is_promoted", "is_enemy", "from_row", "to_row"), &ShogiEngine::can_promote);

	ClassDB::bind_method(D_METHOD("set_game_history", "hashes", "in_checks"), &ShogiEngine::set_game_history);
	ClassDB::bind_method(D_METHOD("update_state_from_sfen", "sfen"), &ShogiEngine::update_state_from_sfen);
	ClassDB::bind_method(D_METHOD("search_best_move"), &ShogiEngine::search_best_move);
	ClassDB::bind_method(D_METHOD("search_top_moves", "count"), &ShogiEngine::search_top_moves);
}

void ShogiEngine::load_book_from_file(const String &path) {
	if (!FileAccess::file_exists(path)) {
		return;
	}

	Ref<FileAccess> file = FileAccess::open(path, FileAccess::READ);
	if (file->get_32() != 0x53484F47) { // "SHOG"
		UtilityFunctions::print("Invalid book file format.");
		return;
	}

	int total_hashes = file->get_32();
	book_.clear();

	for (int i = 0; i < total_hashes; ++i) {
		uint64_t hash = file->get_64();
		int move_count = file->get_32();

		for (int j = 0; j < move_count; ++j) {
			int from_col = file->get_8();
			int from_row = file->get_8();
			int to_col = file->get_8();
			int to_row = file->get_8();
			int piece_type = file->get_8();
			bool is_promotion = file->get_8() != 0;
			bool is_drop = file->get_8() != 0;
			bool is_capture = file->get_8() != 0;

			Move move(from_col, from_row, to_col, to_row, static_cast<PieceType>(piece_type), is_promotion, is_drop,
					is_capture);
			book_[hash].push_back(move);
		}
	}

	UtilityFunctions::print("Book loaded. Total positions: ", total_hashes);
}

TypedArray<Vector2i> ShogiEngine::get_legal_moves(const String &sfen, int col, int row) {
	TypedArray<Vector2i> result;

	Coord from{ col, row };
	if (!from.is_valid()) {
		UtilityFunctions::push_error("get_legal_moves: invalid coord (", col, ", ", row, ")");
		return result;
	}

	BoardState board(std::string(sfen.utf8().get_data()));

	for (int to_col = 0; to_col < Shogi::BOARD_COLS; ++to_col) {
		for (int to_row = 0; to_row < Shogi::BOARD_ROWS; ++to_row) {
			Coord to{ to_col, to_row };
			if (MoveGenerator::is_legal_move(board, from, to)) {
				result.append(Vector2i(to_col, to_row));
			}
		}
	}

	return result;
}

TypedArray<Vector2i> ShogiEngine::get_legal_drops(const String &sfen, int piece_type) {
	TypedArray<Vector2i> result;

	if (piece_type < 0 || piece_type >= Shogi::PIECE_TYPE_COUNT) {
		UtilityFunctions::push_error("get_legal_drops: invalid piece_type (", piece_type, ")");
		return result;
	}

	BoardState board(std::string(sfen.utf8().get_data()));
	bool is_enemy = (board.get_turn_to_move() == Turn::GOTE);

	for (int col = 0; col < Shogi::BOARD_COLS; ++col) {
		for (int row = 0; row < Shogi::BOARD_ROWS; ++row) {
			Coord to{ col, row };
			if (MoveGenerator::is_legal_drop(board, static_cast<PieceType>(piece_type), is_enemy, to)) {
				result.append(Vector2i(col, row));
			}
		}
	}

	return result;
}

bool ShogiEngine::is_king_in_check(const String &sfen, bool is_enemy) {
	BoardState board(std::string(sfen.utf8().get_data()));

	Turn turn = is_enemy ? Turn::GOTE : Turn::SENTE;

	return MoveGenerator::is_king_in_check(board, turn);
}

bool ShogiEngine::has_any_legal_move(const String &sfen) {
	BoardState board(std::string(sfen.utf8().get_data()));

	Shogi::MoveList move_list;
	MoveGenerator::get_legal_moves(board, move_list);

	return !move_list.is_empty();
}

int64_t ShogiEngine::get_position_hash(const String &sfen) {
	BoardState board(std::string(sfen.utf8().get_data()));
	return static_cast<int64_t>(board.get_zobrist_hash());
}

bool ShogiEngine::is_dead_end(int piece_type, bool is_enemy, int to_row) {
	if (piece_type < 0 || piece_type >= Shogi::PIECE_TYPE_COUNT) {
		UtilityFunctions::push_error("is_dead_end: invalid piece_type (", piece_type, ")");
		return false;
	}

	return MoveGenerator::is_dead_end(static_cast<PieceType>(piece_type), is_enemy, to_row);
}

bool ShogiEngine::can_promote(int piece_type, bool is_promoted, bool is_enemy, int from_row, int to_row) {
	if (piece_type < 0 || piece_type >= Shogi::PIECE_TYPE_COUNT) {
		UtilityFunctions::push_error("can_promote: invalid piece_type (", piece_type, ")");
		return false;
	}

	return MoveGenerator::can_promote(static_cast<PieceType>(piece_type), is_promoted, is_enemy, from_row, to_row);
}

void ShogiEngine::update_state_from_sfen(const String &sfen) {
	current_state_ = BoardState(std::string(sfen.utf8().get_data()));
}

void ShogiEngine::set_game_history(const PackedInt64Array &hashes, const PackedByteArray &in_checks) {
	std::vector<uint64_t> h;
	std::vector<bool> c;
	h.reserve(hashes.size());
	c.reserve(in_checks.size());
	for (int i = 0; i < hashes.size(); ++i) {
		h.push_back(static_cast<uint64_t>(hashes[i]));
	}
	for (int i = 0; i < in_checks.size(); ++i) {
		c.push_back(in_checks[i] != 0);
	}
	ai_player_.set_game_history(h, c);
}

Dictionary ShogiEngine::search_best_move() {
	Array moves = search_top_moves(1);
	if (moves.is_empty()) {
		return Dictionary();
	}
	return moves[0];
}

Array ShogiEngine::search_top_moves(int count) {
	// 定跡を参照
	uint64_t hash = current_state_.get_zobrist_hash();
	auto it = book_.find(hash);
	if (it != book_.end() && !it->second.empty()) {
		const std::vector<Move> &book_moves = it->second;

		UtilityFunctions::print("Using Book Move. Hash: ", String::num_uint64(hash));

		Array result;
		for (size_t i = 0; i < book_moves.size() && static_cast<int>(i) < count; ++i) {
			result.append(make_move_dictionary(book_moves[i], 0, 0.5));
		}
		return result;
	}

	return ai_player_.search_top_moves(current_state_, count);
}
