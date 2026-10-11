#include "shogi_engine.hpp"
#include "ai_player.hpp"
#include "move_generator.hpp"
#include <godot_cpp/classes/file_access.hpp>
#include <godot_cpp/core/class_db.hpp>
#include <godot_cpp/variant/dictionary.hpp>
#include <godot_cpp/variant/utility_functions.hpp>
#include <algorithm>
#include <optional>
#include <ranges>
#include <span>
#include <string>
#include <utility>

using namespace godot;
using Shogi::Move;
using Shogi::Turn;

namespace {

Dictionary make_move_dictionary(const Move &move) {
	Dictionary result;
	result["from_col"] = move.is_drop ? -1 : move.from_col;
	result["from_row"] = move.is_drop ? -1 : move.from_row;
	result["to_col"] = move.to_col;
	result["to_row"] = move.to_row;
	result["piece_type"] = std::to_underlying(move.piece_type);
	result["is_promotion"] = move.is_promotion;
	result["is_drop"] = move.is_drop;
	return result;
}

Dictionary make_scored_move_dictionary(const Move &move, int score, double win_rate) {
	Dictionary result = make_move_dictionary(move);
	result["score"] = score;
	result["win_rate"] = win_rate;
	return result;
}

BoardState board_from_sfen(const String &sfen) {
	std::string text(sfen.utf8().get_data());
	std::optional<BoardState> board = BoardState::from_sfen(text);
	if (!board.has_value()) {
		UtilityFunctions::push_error(("Invalid SFEN: " + text).c_str());
		return BoardState(Turn::SENTE);
	}
	return *board;
}

void load_zobrist_params_from_file(const String &path) {
	if (BoardState::zobrist_initialized()) {
		return;
	}

	if (!FileAccess::file_exists(path)) {
		UtilityFunctions::print("Zobrist params file not found: " + path);
		return;
	}

	PackedByteArray data = FileAccess::get_file_as_bytes(path);
	if (!BoardState::load_zobrist_params(std::span(data.ptr(), static_cast<size_t>(data.size())))) {
		UtilityFunctions::print("Invalid Zobrist params file format.");
		return;
	}

	UtilityFunctions::print("Zobrist parameters loaded successfully.");
}

} // namespace

ShogiEngine::ShogiEngine() {
	ai_player_.set_logger([](const std::string &message) { UtilityFunctions::print(message.c_str()); });

	if (!is_initialized_) {
		load_zobrist_params_from_file("res://assets/data/zobrist_params.bin");
		load_book_from_file("res://assets/data/book.bin");

		is_initialized_ = true;
	}
}

void ShogiEngine::_bind_methods() {
	ClassDB::bind_method(D_METHOD("get_legal_moves", "sfen"), &ShogiEngine::get_legal_moves);
	ClassDB::bind_method(D_METHOD("inspect_position", "sfen"), &ShogiEngine::inspect_position);

	ClassDB::bind_method(D_METHOD("set_game_history", "hashes", "in_checks"), &ShogiEngine::set_game_history);
	ClassDB::bind_method(D_METHOD("update_state_from_sfen", "sfen"), &ShogiEngine::update_state_from_sfen);
	ClassDB::bind_method(D_METHOD("search_best_move"), &ShogiEngine::search_best_move);
	ClassDB::bind_method(D_METHOD("search_top_moves", "count"), &ShogiEngine::search_top_moves);
	ClassDB::bind_method(D_METHOD("set_time_limit_msec", "msec"), &ShogiEngine::set_time_limit_msec);

	BIND_ENUM_CONSTANT(PIECE_KING);
	BIND_ENUM_CONSTANT(PIECE_ROOK);
	BIND_ENUM_CONSTANT(PIECE_BISHOP);
	BIND_ENUM_CONSTANT(PIECE_GOLD);
	BIND_ENUM_CONSTANT(PIECE_SILVER);
	BIND_ENUM_CONSTANT(PIECE_KNIGHT);
	BIND_ENUM_CONSTANT(PIECE_LANCE);
	BIND_ENUM_CONSTANT(PIECE_PAWN);
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

			Move move(from_col, from_row, to_col, to_row, static_cast<Shogi::PieceType>(piece_type), is_promotion, is_drop,
					is_capture);
			book_[hash].push_back(move);
		}
	}

	UtilityFunctions::print("Book loaded. Total positions: ", total_hashes);
}

Array ShogiEngine::get_legal_moves(const String &sfen) {
	BoardState board = board_from_sfen(sfen);

	Shogi::MoveList move_list;
	MoveGenerator::get_legal_moves(board, move_list);

	Array result;
	for (const Move &move : move_list) {
		result.append(make_move_dictionary(move));
	}
	return result;
}

Dictionary ShogiEngine::inspect_position(const String &sfen) {
	BoardState board = board_from_sfen(sfen);

	Shogi::MoveList move_list;
	MoveGenerator::get_legal_moves(board, move_list);

	Dictionary result;
	result["hash"] = static_cast<int64_t>(board.get_zobrist_hash());
	result["in_check"] = MoveGenerator::is_king_in_check(board, board.get_turn_to_move());
	result["has_legal_move"] = !move_list.is_empty();
	return result;
}

void ShogiEngine::update_state_from_sfen(const String &sfen) {
	current_state_ = board_from_sfen(sfen);
}

void ShogiEngine::set_game_history(const PackedInt64Array &hashes, const PackedByteArray &in_checks) {
	std::vector<uint64_t> history_hashes;
	std::vector<bool> history_in_checks;
	history_hashes.reserve(hashes.size());
	history_in_checks.reserve(in_checks.size());
	for (int i = 0; i < hashes.size(); ++i) {
		history_hashes.push_back(static_cast<uint64_t>(hashes[i]));
	}
	for (int i = 0; i < in_checks.size(); ++i) {
		history_in_checks.push_back(in_checks[i] != 0);
	}
	ai_player_.set_game_history(history_hashes, history_in_checks);
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
		for (const Move &move : book_moves | std::views::take(std::max(count, 0))) {
			result.append(make_scored_move_dictionary(move, 0, 0.5));
		}
		return result;
	}

	Array result;
	for (const ScoredMove &entry : ai_player_.search_top_moves(current_state_, count)) {
		result.append(make_scored_move_dictionary(entry.move, entry.score, entry.win_rate));
	}
	return result;
}

void ShogiEngine::set_time_limit_msec(int msec) {
	if (msec <= 0) {
		return;
	}
	ai_player_.set_time_limit_usec(static_cast<uint64_t>(msec) * 1000);
}
