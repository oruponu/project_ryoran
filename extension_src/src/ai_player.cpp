#include "ai_player.hpp"
#include <godot_cpp/classes/time.hpp>
#include <godot_cpp/variant/utility_functions.hpp>
#include <vector>

using namespace godot;

Dictionary AIPlayer::get_next_move(Node2D *main_node) {
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
