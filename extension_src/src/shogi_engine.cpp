#include "shogi_engine.hpp"
#include "ai_player.hpp"
#include <godot_cpp/classes/node.hpp>
#include <godot_cpp/classes/time.hpp>
#include <godot_cpp/core/class_db.hpp>
#include <godot_cpp/variant/utility_functions.hpp>

using namespace godot;

void ShogiEngine::_bind_methods() {
    ClassDB::bind_static_method("ShogiEngine",
                                D_METHOD("is_legal_move", "main_node", "piece_obj", "target_col", "target_row"),
                                &ShogiEngine::is_legal_move);
    ClassDB::bind_static_method("ShogiEngine",
                                D_METHOD("is_legal_drop", "main_node", "piece_obj", "target_col", "target_row"),
                                &ShogiEngine::is_legal_drop);
    ClassDB::bind_static_method("ShogiEngine", D_METHOD("get_legal_moves", "main_node", "piece_obj"),
                                &ShogiEngine::get_legal_moves);
    ClassDB::bind_static_method("ShogiEngine", D_METHOD("get_legal_drops", "main_node", "piece_obj"),
                                &ShogiEngine::get_legal_drops);
    ClassDB::bind_static_method(
        "ShogiEngine", D_METHOD("is_king_safe_after_move", "main_node", "piece_obj", "target_col", "target_row"),
        &ShogiEngine::is_king_safe_after_move);
    ClassDB::bind_static_method("ShogiEngine", D_METHOD("is_king_in_check", "main_node", "is_enemy"),
                                &ShogiEngine::is_king_in_check);

    ClassDB::bind_method(D_METHOD("update_state", "main_node"), &ShogiEngine::update_state);
    ClassDB::bind_method(D_METHOD("search_best_move"), &ShogiEngine::search_best_move);

    ClassDB::bind_method(D_METHOD("set_is_enemy_side", "is_enemy"), &ShogiEngine::set_is_enemy_side);
    ClassDB::bind_method(D_METHOD("get_is_enemy_side"), &ShogiEngine::get_is_enemy_side);
    ADD_PROPERTY(PropertyInfo(Variant::BOOL, "is_enemy_side"), "set_is_enemy_side", "get_is_enemy_side");
}

void ShogiEngine::set_is_enemy_side(bool is_enemy) { is_enemy_side = is_enemy; }

bool ShogiEngine::get_is_enemy_side() const { return is_enemy_side; }

bool ShogiEngine::is_legal_move(Node2D *main_node, Object *piece_obj, int target_col, int target_row) {
    if (!piece_obj) {
        return false;
    }

    BoardState board;
    board.init_from_main(main_node);

    int current_col = piece_obj->get("current_col");
    int current_row = piece_obj->get("current_row");

    return board.is_legal_move(current_col, current_row, target_col, target_row);
}

bool ShogiEngine::is_legal_drop(Node2D *main_node, Object *piece_obj, int target_col, int target_row) {
    if (!piece_obj) {
        return false;
    }

    BoardState board;
    board.init_from_main(main_node);

    int piece_type = piece_obj->get("piece_type");
    bool is_enemy = piece_obj->get("is_enemy");

    return board.is_legal_drop(piece_type, is_enemy, target_col, target_row);
}

TypedArray<Vector2i> ShogiEngine::get_legal_moves(Node2D *main_node, Object *piece_obj) {
    TypedArray<Vector2i> result;
    if (!piece_obj) {
        return result;
    }

    BoardState board;
    board.init_from_main(main_node);

    int current_col = piece_obj->get("current_col");
    int current_row = piece_obj->get("current_row");

    for (int col = 0; col < Shogi::BOARD_COLS; ++col) {
        for (int row = 0; row < Shogi::BOARD_ROWS; ++row) {
            if (board.is_legal_move(current_col, current_row, col, row)) {
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

    BoardState board;
    board.init_from_main(main_node);

    int piece_type = piece_obj->get("piece_type");
    bool is_enemy = piece_obj->get("is_enemy");

    for (int col = 0; col < Shogi::BOARD_COLS; ++col) {
        for (int row = 0; row < Shogi::BOARD_ROWS; ++row) {
            if (board.is_legal_drop(piece_type, is_enemy, col, row)) {
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

    BoardState board;
    board.init_from_main(main_node);

    int piece_type = piece_obj->get("piece_type");
    int current_col = piece_obj->get("current_col");
    int current_row = piece_obj->get("current_row");
    bool is_enemy = piece_obj->get("is_enemy");
    bool is_promoted = piece_obj->get("is_promoted");

    int side = is_enemy ? Shogi::ENEMY : Shogi::PLAYER;

    if (current_col != -1 && current_row != -1) {
        board.clear_cell(current_col, current_row);
    }

    board.set_cell(target_col, target_row, piece_type, side, is_promoted);

    return !board.is_king_in_check(side);
}

bool ShogiEngine::is_king_in_check(Node2D *main_node, bool is_enemy) {
    BoardState board;
    board.init_from_main(main_node);

    int side = is_enemy ? Shogi::ENEMY : Shogi::PLAYER;

    return board.is_king_in_check(side);
}

void ShogiEngine::update_state(Node2D *main_node) {
    current_state = BoardState();
    current_state.init_from_main(main_node);
}

Dictionary ShogiEngine::search_best_move() {
    AIPlayer ai_player(is_enemy_side);
    return ai_player.search_best_move(current_state);
}
