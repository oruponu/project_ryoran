#include "ai_player.hpp"
#include "board_state.hpp"
#include <godot_cpp/classes/node.hpp>
#include <godot_cpp/classes/time.hpp>
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

    int side = is_enemy_side ? Shogi::ENEMY : Shogi::PLAYER;
    std::vector<Shogi::Move> moves = current_board.get_legal_moves(side);

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
            if ((int)piece->get("piece_type") == selected.piece_type) {
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
