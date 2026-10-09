#pragma once

#include "ai_player.hpp"
#include "board_state.hpp"
#include <godot_cpp/classes/ref_counted.hpp>
#include <godot_cpp/variant/array.hpp>
#include <godot_cpp/variant/dictionary.hpp>
#include <godot_cpp/variant/packed_byte_array.hpp>
#include <godot_cpp/variant/packed_int64_array.hpp>
#include <godot_cpp/variant/string.hpp>
#include <godot_cpp/variant/typed_array.hpp>
#include <godot_cpp/variant/vector2i.hpp>
#include <unordered_map>
#include <vector>

class ShogiEngine : public godot::RefCounted {
	GDCLASS(ShogiEngine, godot::RefCounted);

private:
	inline static std::unordered_map<uint64_t, std::vector<Shogi::Move>> book_;
	inline static bool is_initialized_;

	BoardState current_state_;
	AIPlayer ai_player_;

	static void load_book_from_file(const godot::String &path);

protected:
	static void _bind_methods();

public:
	ShogiEngine();
	~ShogiEngine() {}

	[[nodiscard]] godot::TypedArray<godot::Vector2i> get_legal_moves(const godot::String &sfen, int col, int row);
	[[nodiscard]] godot::TypedArray<godot::Vector2i> get_legal_drops(const godot::String &sfen, int piece_type);
	[[nodiscard]] bool is_king_in_check(const godot::String &sfen, bool is_enemy);
	[[nodiscard]] bool has_any_legal_move(const godot::String &sfen);
	[[nodiscard]] int64_t get_position_hash(const godot::String &sfen);
	[[nodiscard]] bool is_dead_end(int piece_type, bool is_enemy, int to_row);
	[[nodiscard]] bool can_promote(int piece_type, bool is_promoted, bool is_enemy, int from_row, int to_row);

	void set_game_history(const godot::PackedInt64Array &hashes, const godot::PackedByteArray &in_checks);
	void update_state_from_sfen(const godot::String &sfen);
	[[nodiscard]] godot::Dictionary search_best_move();
	[[nodiscard]] godot::Array search_top_moves(int count);
	void set_time_limit_msec(int msec);
};
