#include "shogi_engine.hpp"
#include "board_state.hpp"
#include <godot_cpp/classes/node.hpp>
#include <godot_cpp/classes/time.hpp>
#include <godot_cpp/core/class_db.hpp>
#include <godot_cpp/variant/utility_functions.hpp>

using namespace godot;

ShogiEngine::ShogiEngine() {}
ShogiEngine::~ShogiEngine() {}

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

    ClassDB::bind_method(D_METHOD("get_next_move", "main_node"), &ShogiEngine::get_next_move);

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

Dictionary ShogiEngine::get_next_move(Node2D *main_node) {
    BoardState board;
    board.init_from_main(main_node);

    board.print_board();

    int side = is_enemy_side ? Shogi::ENEMY : Shogi::PLAYER;

    std::vector<Shogi::Move> moves;

    for (int col = 0; col < Shogi::BOARD_COLS; ++col) {
        for (int row = 0; row < Shogi::BOARD_ROWS; ++row) {
            const Cell &cell = board.get_cell(col, row);

            // 自駒でないならスキップ
            if (cell.is_empty() || cell.side != side) {
                continue;
            }

            for (int t_col = 0; t_col < Shogi::BOARD_COLS; ++t_col) {
                for (int t_row = 0; t_row < Shogi::BOARD_ROWS; ++t_row) {
                    if (board.is_legal_move(col, row, t_col, t_row)) {
                        bool is_capture = !board.get_cell(t_col, t_row).is_empty();
                        bool can_promote = false;
                        bool must_promote = false;

                        if (!cell.is_promoted && cell.type != Shogi::KING && cell.type != Shogi::GOLD) {
                            int zone_min = is_enemy_side ? 6 : 0;
                            int zone_max = is_enemy_side ? 8 : 2;
                            bool from_in_zone = (row >= zone_min && row <= zone_max);
                            bool to_in_zone = (t_row >= zone_min && t_row <= zone_max);

                            if (from_in_zone || to_in_zone) {
                                can_promote = true;
                            }
                        }

                        if (board.is_dead_end(cell.type, is_enemy_side, t_row)) {
                            must_promote = true;
                        }

                        if (!must_promote) {
                            moves.emplace_back(col, row, t_col, t_row, cell.type, false, false, is_capture);
                        }

                        if (can_promote) {
                            moves.emplace_back(col, row, t_col, t_row, cell.type, true, false, is_capture);
                        }
                    }
                }
            }
        }
    }

    for (int piece_type = 0; piece_type < Shogi::PIECE_TYPE_COUNT; ++piece_type) {
        if (board.get_hand_count(side, piece_type) > 0) {
            for (int t_col = 0; t_col < Shogi::BOARD_COLS; ++t_col) {
                for (int t_row = 0; t_row < Shogi::BOARD_ROWS; ++t_row) {
                    if (board.is_legal_drop(piece_type, is_enemy_side, t_col, t_row)) {
                        moves.emplace_back(0, 0, t_col, t_row, piece_type, false, true, false);
                    }
                }
            }
        }
    }

    if (moves.empty()) {
        // 投了
        return Dictionary();
    }

    int index = (int)(Time::get_singleton()->get_ticks_usec() % moves.size());
    const Shogi::Move &selected = moves[index];

    Dictionary result;

    if (selected.is_drop) {
        String stand_name = is_enemy_side ? "enemy_piece_stand" : "player_piece_stand";
        Node *stand = Object::cast_to<Node>(main_node->get(stand_name));
        Array children = stand->get_children();

        for (int i = 0; i < children.size(); ++i) {
            Object *piece = children[i];
            Variant v_type = piece->get("piece_type");
            if (v_type.get_type() == Variant::INT && (int)v_type == selected.piece_type) {
                result["piece"] = piece;
                break;
            }
        }
    } else {
        result["piece"] = main_node->call("get_piece", selected.from_col, selected.from_row);
    }

    result["to_col"] = selected.to_col;
    result["to_row"] = selected.to_row;
    result["is_promotion"] = selected.is_promotion;
    result["is_drop"] = selected.is_drop;

    return result;
}
