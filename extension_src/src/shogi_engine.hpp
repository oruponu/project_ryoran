#ifndef SHOGI_ENGINE_HPP
#define SHOGI_ENGINE_HPP

#include "board_state.hpp"
#include <godot_cpp/classes/node2d.hpp>
#include <godot_cpp/classes/ref_counted.hpp>
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
    inline static std::unordered_map<uint64_t, std::vector<Shogi::Move>> book;
    inline static bool is_initialized;

    BoardState current_state;
    Shogi::Side side_to_move = Shogi::PLAYER;

    static void load_book_from_file(const String &path);

  protected:
    static void _bind_methods();

  public:
    ShogiEngine();
    ~ShogiEngine() {}

    void set_is_enemy_side(bool is_enemy_side) { side_to_move = is_enemy_side ? Shogi::ENEMY : Shogi::PLAYER; }
    bool get_is_enemy_side() const { return side_to_move == Shogi::ENEMY; }

    bool is_legal_move(Node2D *main_node, Object *piece_obj, int target_col, int target_row);
    bool is_legal_drop(Node2D *main_node, Object *piece_obj, int target_col, int target_row);
    TypedArray<Vector2i> get_legal_moves(Node2D *main_node, Object *piece_obj);
    TypedArray<Vector2i> get_legal_drops(Node2D *main_node, Object *piece_obj);
    bool is_king_safe_after_move(Node2D *main_node, Object *piece_obj, int target_col, int target_row);
    bool is_king_in_check(Node2D *main_node, bool is_enemy);

    void update_state(Node2D *main_node);
    Dictionary search_best_move();
};

#endif
