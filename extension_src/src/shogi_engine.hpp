#pragma once

#include "ai_player.hpp"
#include "board_state.hpp"
#include <godot_cpp/classes/ref_counted.hpp>
#include <godot_cpp/variant/array.hpp>
#include <godot_cpp/variant/packed_byte_array.hpp>
#include <godot_cpp/variant/packed_int64_array.hpp>
#include <godot_cpp/variant/typed_array.hpp>
#include <godot_cpp/variant/vector2i.hpp>
#include <string>
#include <vector>

using namespace godot;

class ShogiEngine : public RefCounted {
	GDCLASS(ShogiEngine, RefCounted);

private:
	inline static std::unordered_map<uint64_t, std::vector<Shogi::Move>> book_;
	inline static bool is_initialized_;

	BoardState current_state_;
	AIPlayer ai_player_;

	static void load_book_from_file(const String &path);

protected:
	static void _bind_methods();

public:
	ShogiEngine();
	~ShogiEngine() {}

	[[nodiscard]] TypedArray<Vector2i> get_legal_moves(const String &sfen, int col, int row);
	[[nodiscard]] TypedArray<Vector2i> get_legal_drops(const String &sfen, int piece_type);
	[[nodiscard]] bool is_king_in_check(const String &sfen, bool is_enemy);
	[[nodiscard]] bool has_any_legal_move(const String &sfen);
	[[nodiscard]] int64_t get_position_hash(const String &sfen);
	[[nodiscard]] bool is_dead_end(int piece_type, bool is_enemy, int to_row);
	[[nodiscard]] bool can_promote(int piece_type, bool is_promoted, bool is_enemy, int from_row, int to_row);

	void set_game_history(const PackedInt64Array &hashes, const PackedByteArray &in_checks);
	void update_state_from_sfen(const String &sfen);
	[[nodiscard]] Dictionary search_best_move();
	[[nodiscard]] Array search_top_moves(int count);
};
