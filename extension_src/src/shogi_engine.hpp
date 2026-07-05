#pragma once

#include "ai_player.hpp"
#include "board_state.hpp"
#include <godot_cpp/classes/node2d.hpp>
#include <godot_cpp/classes/ref_counted.hpp>
#include <godot_cpp/variant/packed_byte_array.hpp>
#include <godot_cpp/variant/packed_int64_array.hpp>
#include <vector>

using namespace godot;

struct MoveData {
	Object *piece;
	int from_col;
	int from_row;
	int to_col;
	int to_row;
	bool is_promotion;
	bool is_drop;
	int piece_type;
};

class ShogiEngine : public RefCounted {
	GDCLASS(ShogiEngine, RefCounted);

private:
	inline static std::unordered_map<uint64_t, std::vector<Shogi::Move>> book_;
	inline static bool is_initialized_;

	BoardState current_state_;
	Shogi::Turn turn_to_move_ = Shogi::Turn::SENTE;
	AIPlayer ai_player_;

	static void load_book_from_file(const String &path);

protected:
	static void _bind_methods();

public:
	ShogiEngine();
	~ShogiEngine() {}

	void set_is_enemy_side(bool is_enemy_side) {
		turn_to_move_ = is_enemy_side ? Shogi::Turn::GOTE : Shogi::Turn::SENTE;
	}
	[[nodiscard]] bool get_is_enemy_side() const { return turn_to_move_ == Shogi::Turn::GOTE; }

	[[nodiscard]] bool is_legal_move(Node2D *main_node, Object *piece_obj, int target_col, int target_row);
	[[nodiscard]] bool is_legal_drop(Node2D *main_node, Object *piece_obj, int target_col, int target_row);
	[[nodiscard]] TypedArray<Vector2i> get_legal_moves(Node2D *main_node, Object *piece_obj);
	[[nodiscard]] TypedArray<Vector2i> get_legal_drops(Node2D *main_node, Object *piece_obj);
	[[nodiscard]] bool is_king_safe_after_move(Node2D *main_node, Object *piece_obj, int target_col, int target_row);
	[[nodiscard]] bool is_dead_end(Node2D *main_node, Object *piece_obj, int to_row);
	[[nodiscard]] bool is_king_in_check(Node2D *main_node, bool is_enemy);
	[[nodiscard]] int64_t get_position_hash(Node2D *main_node, bool is_enemy);

	void set_game_history(const PackedInt64Array &hashes, const PackedByteArray &in_checks);
	void update_state(Node2D *main_node);
	[[nodiscard]] Dictionary search_best_move();
};
