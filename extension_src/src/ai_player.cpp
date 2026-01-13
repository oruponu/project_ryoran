#include "ai_player.hpp"
#include "board_state.hpp"
#include <godot_cpp/classes/node.hpp>
#include <godot_cpp/core/class_db.hpp>
#include <godot_cpp/variant/utility_functions.hpp>

using namespace godot;

AIPlayer::AIPlayer() {}
AIPlayer::~AIPlayer() {}

void AIPlayer::_bind_methods() {
    ClassDB::bind_method(D_METHOD("get_next_move", "main_node"), &AIPlayer::get_next_move);

    ClassDB::bind_method(D_METHOD("set_is_enemy_side", "is_enemy"), &AIPlayer::set_is_enemy_side);
    ClassDB::bind_method(D_METHOD("get_is_enemy_side"), &AIPlayer::get_is_enemy_side);
    ADD_PROPERTY(PropertyInfo(Variant::BOOL, "is_enemy_side"), "set_is_enemy_side", "get_is_enemy_side");
}

void AIPlayer::set_is_enemy_side(bool is_enemy) { is_enemy_side = is_enemy; }

bool AIPlayer::get_is_enemy_side() const { return is_enemy_side; }

Dictionary AIPlayer::get_next_move(Node2D *main_node) {
    BoardState current_board;
    current_board.init_from_main(main_node);

    current_board.print_board();

    std::vector<MoveData> moves = _get_legal_moves(main_node);

    if (moves.empty()) {
        // 投了
        return Dictionary();
    }

    int index = UtilityFunctions::randi() % moves.size();
    MoveData selected = moves[index];

    Dictionary result;
    result["piece"] = selected.piece;
    result["to_col"] = selected.to_col;
    result["to_row"] = selected.to_row;
    result["is_promotion"] = selected.is_promotion;
    result["is_drop"] = selected.is_drop;

    return result;
}

std::vector<MoveData> AIPlayer::_get_legal_moves(Node2D *main_node) {
    std::vector<MoveData> moves;

    // 盤上の駒
    Array pieces;
    for (int col = 0; col < Shogi::BOARD_COLS; ++col) {
        for (int row = 0; row < Shogi::BOARD_ROWS; ++row) {
            Object *piece = main_node->call("get_piece", col, row);
            if (piece != nullptr) {
                bool is_enemy = piece->get("is_enemy");
                if (is_enemy == is_enemy_side) {
                    pieces.append(piece);
                }
            }
        }
    }

    // 持ち駒
    Node *stand = is_enemy_side ? Object::cast_to<Node>(main_node->get("enemy_piece_stand"))
                                : Object::cast_to<Node>(main_node->get("player_piece_stand"));
    if (stand != nullptr) {
        pieces.append_array(stand->get_children());
    }

    // 各駒の合法手を取得
    for (int i = 0; i < pieces.size(); ++i) {
        Object *piece = pieces[i];
        if (!piece) {
            continue;
        }

        int current_col = piece->get("current_col");
        int current_row = piece->get("current_row");
        bool is_drop = (current_col == -1 && current_row == -1);

        Array coords = is_drop ? piece->call("get_legal_drop") : piece->call("get_legal_moves");

        for (int j = 0; j < coords.size(); ++j) {
            Vector2 coord = coords[j];
            if (main_node->call("is_king_safe_after_move", piece, coord.x, coord.y)) {
                MoveData move_data;
                move_data.piece = piece;
                move_data.from_col = current_col;
                move_data.from_row = current_row;
                move_data.to_col = (int)coord.x;
                move_data.to_row = (int)coord.y;
                move_data.is_drop = is_drop;
                move_data.is_promotion = false;

                if (!is_drop && !((bool)piece->get("is_promoted"))) {
                    int type = piece->get("piece_type");
                    if (type != 0 && type != 3) {
                        if (is_enemy_side && (move_data.to_row >= 6 || current_row >= 6)) {
                            move_data.is_promotion = true;
                        }

                        if (!is_enemy_side && (move_data.to_row <= 2 || current_row <= 2)) {
                            move_data.is_promotion = true;
                        }
                    }
                }

                moves.push_back(move_data);
            }
        }
    }

    return moves;
}
