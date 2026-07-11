#include "shogi_engine.hpp"
#include "ai_player.hpp"
#include "move_generator.hpp"
#include <godot_cpp/classes/file_access.hpp>
#include <godot_cpp/classes/node.hpp>
#include <godot_cpp/classes/time.hpp>
#include <godot_cpp/core/class_db.hpp>
#include <godot_cpp/variant/utility_functions.hpp>

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
	ClassDB::bind_method(D_METHOD("set_is_enemy_side", "is_enemy_side"), &ShogiEngine::set_is_enemy_side);
	ClassDB::bind_method(D_METHOD("get_is_enemy_side"), &ShogiEngine::get_is_enemy_side);
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "is_enemy_side"), "set_is_enemy_side", "get_is_enemy_side");

	ClassDB::bind_method(D_METHOD("is_legal_move", "main_node", "piece_obj", "target_col", "target_row"),
			&ShogiEngine::is_legal_move);
	ClassDB::bind_method(D_METHOD("is_legal_drop", "main_node", "piece_obj", "target_col", "target_row"),
			&ShogiEngine::is_legal_drop);
	ClassDB::bind_method(D_METHOD("get_legal_moves", "main_node", "piece_obj"), &ShogiEngine::get_legal_moves);
	ClassDB::bind_method(D_METHOD("get_legal_drops", "main_node", "piece_obj"), &ShogiEngine::get_legal_drops);
	ClassDB::bind_method(D_METHOD("is_king_safe_after_move", "main_node", "piece_obj", "target_col", "target_row"),
			&ShogiEngine::is_king_safe_after_move);
	ClassDB::bind_method(D_METHOD("is_dead_end", "main_node", "piece_obj", "to_row"), &ShogiEngine::is_dead_end);
	ClassDB::bind_method(D_METHOD("is_king_in_check", "main_node", "is_enemy"), &ShogiEngine::is_king_in_check);
	ClassDB::bind_method(D_METHOD("get_position_hash", "main_node", "is_enemy"), &ShogiEngine::get_position_hash);
	ClassDB::bind_method(D_METHOD("get_position_hash_sfen", "sfen"), &ShogiEngine::get_position_hash_sfen);

	ClassDB::bind_method(D_METHOD("set_game_history", "hashes", "in_checks"), &ShogiEngine::set_game_history);
	ClassDB::bind_method(D_METHOD("update_state", "main_node"), &ShogiEngine::update_state);
	ClassDB::bind_method(D_METHOD("search_best_move"), &ShogiEngine::search_best_move);
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

bool ShogiEngine::is_legal_move(Node2D *main_node, Object *piece_obj, int target_col, int target_row) {
	if (!piece_obj) {
		return false;
	}

	BoardState board(main_node, turn_to_move_);
	int current_col = piece_obj->get("current_col");
	int current_row = piece_obj->get("current_row");

	return MoveGenerator::is_legal_move(board, { current_col, current_row }, { target_col, target_row });
}

bool ShogiEngine::is_legal_drop(Node2D *main_node, Object *piece_obj, int target_col, int target_row) {
	if (!piece_obj) {
		return false;
	}

	int piece_type = piece_obj->get("piece_type");
	bool is_enemy = piece_obj->get("is_enemy");

	BoardState board(main_node, is_enemy ? Turn::GOTE : Turn::SENTE);

	return MoveGenerator::is_legal_drop(board, static_cast<PieceType>(piece_type), is_enemy, { target_col, target_row });
}

TypedArray<Vector2i> ShogiEngine::get_legal_moves(Node2D *main_node, Object *piece_obj) {
	TypedArray<Vector2i> result;
	if (!piece_obj) {
		return result;
	}

	BoardState board(main_node, turn_to_move_);
	int current_col = piece_obj->get("current_col");
	int current_row = piece_obj->get("current_row");
	Coord from{ current_col, current_row };

	for (int col = 0; col < Shogi::BOARD_COLS; ++col) {
		for (int row = 0; row < Shogi::BOARD_ROWS; ++row) {
			Coord to{ col, row };
			if (MoveGenerator::is_legal_move(board, from, to)) {
				result.append(Vector2i(col, row));
			}
		}
	}

	return result;
}

TypedArray<Vector2i> ShogiEngine::get_legal_drops(Node2D *main_node, Object *piece_obj) {
	TypedArray<Vector2i> result;
	if (!piece_obj) {
		return result;
	}

	int piece_type = piece_obj->get("piece_type");
	bool is_enemy = piece_obj->get("is_enemy");

	BoardState board(main_node, is_enemy ? Turn::GOTE : Turn::SENTE);

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

bool ShogiEngine::is_king_safe_after_move(Node2D *main_node, Object *piece_obj, int target_col, int target_row) {
	if (!piece_obj) {
		return false;
	}

	BoardState board(main_node, turn_to_move_);
	int piece_type = piece_obj->get("piece_type");
	int current_col = piece_obj->get("current_col");
	int current_row = piece_obj->get("current_row");
	bool is_enemy = piece_obj->get("is_enemy");
	bool is_promoted = piece_obj->get("is_promoted");

	Turn turn = is_enemy ? Turn::GOTE : Turn::SENTE;
	Coord from{ current_col, current_row };
	Coord to{ target_col, target_row };

	if (from.is_valid()) {
		board.clear_cell(from);
	}

	board.set_cell(to, static_cast<PieceType>(piece_type), turn, is_promoted);

	return !MoveGenerator::is_king_in_check(board, turn);
}

bool ShogiEngine::is_dead_end(Node2D *main_node, Object *piece_obj, int to_row) {
	if (!piece_obj) {
		return false;
	}

	int piece_type = piece_obj->get("piece_type");
	bool is_enemy = piece_obj->get("is_enemy");

	return MoveGenerator::is_dead_end(static_cast<PieceType>(piece_type), is_enemy, to_row);
}

bool ShogiEngine::is_king_in_check(Node2D *main_node, bool is_enemy) {
	BoardState board(main_node, turn_to_move_);

	Turn turn = is_enemy ? Turn::GOTE : Turn::SENTE;

	return MoveGenerator::is_king_in_check(board, turn);
}

int64_t ShogiEngine::get_position_hash(Node2D *main_node, bool is_enemy) {
	// 千日手判定用の局面のハッシュ値
	Turn turn = is_enemy ? Turn::GOTE : Turn::SENTE;
	BoardState board(main_node, turn);
	return static_cast<int64_t>(board.get_zobrist_hash());
}

int64_t ShogiEngine::get_position_hash_sfen(const String &sfen) {
	BoardState board(std::string(sfen.utf8().get_data()));
	return static_cast<int64_t>(board.get_zobrist_hash());
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

void ShogiEngine::update_state(Node2D *main_node) {
	current_state_ = BoardState(main_node, turn_to_move_);
}

Dictionary ShogiEngine::search_best_move() {
	// 定跡を参照
	uint64_t hash = current_state_.get_zobrist_hash();
	auto it = book_.find(hash);
	if (it != book_.end()) {
		const std::vector<Move> &book_moves = it->second;
		if (!book_moves.empty()) {
			const Move &best_move = book_moves[0];

			UtilityFunctions::print("Using Book Move. Hash: ", String::num_uint64(hash));

			Dictionary result;
			result["from_col"] = best_move.from_col;
			result["from_row"] = best_move.from_row;
			result["to_col"] = best_move.to_col;
			result["to_row"] = best_move.to_row;
			result["piece_type"] = static_cast<int>(best_move.piece_type);
			result["is_promotion"] = best_move.is_promotion;
			result["is_drop"] = best_move.is_drop;
			result["win_rate"] = 0.5;
			return result;
		}
	}

	return ai_player_.search_best_move(current_state_);
}
