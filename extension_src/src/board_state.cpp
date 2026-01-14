#include "board_state.hpp"
#include <godot_cpp/classes/node.hpp>
#include <godot_cpp/variant/utility_functions.hpp>

using namespace godot;

namespace {

struct Direction {
    int dx;
    int dy;
};

const Direction DIR_UP = {0, -1};
const Direction DIR_UP_RIGHT = {1, -1};
const Direction DIR_RIGHT = {1, 0};
const Direction DIR_DOWN_RIGHT = {1, 1};
const Direction DIR_DOWN = {0, 1};
const Direction DIR_DOWN_LEFT = {-1, 1};
const Direction DIR_LEFT = {-1, 0};
const Direction DIR_UP_LEFT = {-1, -1};
const Direction DIR_KNIGHT_LEFT = {-1, -2};
const Direction DIR_KNIGHT_RIGHT = {1, -2};

const std::vector<Direction> MOVES_PAWN = {DIR_UP};
const std::vector<Direction> MOVES_LANCE_SLIDE = {DIR_UP};
const std::vector<Direction> MOVES_KNIGHT = {DIR_KNIGHT_LEFT, DIR_KNIGHT_RIGHT};
const std::vector<Direction> MOVES_SILVER = {DIR_UP_LEFT, DIR_UP, DIR_UP_RIGHT, DIR_DOWN_LEFT, DIR_DOWN_RIGHT};
const std::vector<Direction> MOVES_GOLD = {DIR_UP_LEFT, DIR_UP, DIR_UP_RIGHT, DIR_LEFT, DIR_RIGHT, DIR_DOWN};
const std::vector<Direction> MOVES_BISHOP_SLIDE = {DIR_UP_LEFT, DIR_UP_RIGHT, DIR_DOWN_LEFT, DIR_DOWN_RIGHT};
const std::vector<Direction> MOVES_ROOK_SLIDE = {DIR_UP, DIR_RIGHT, DIR_DOWN, DIR_LEFT};
const std::vector<Direction> MOVES_KING = {DIR_UP_LEFT, DIR_UP,        DIR_UP_RIGHT, DIR_LEFT,
                                           DIR_RIGHT,   DIR_DOWN_LEFT, DIR_DOWN,     DIR_DOWN_RIGHT};
const std::vector<Direction> &MOVES_PROMOTED = MOVES_GOLD;

} // namespace

BoardState::BoardState() {
    // 盤面を初期化
    for (int i = 0; i < Shogi::BOARD_SIZE; ++i) {
        board[i] = Cell();
    }

    // 持ち駒を初期化
    for (int side = 0; side < 2; ++side) {
        for (int piece_type = 0; piece_type < Shogi::PIECE_TYPE_COUNT; ++piece_type) {
            hand[side][piece_type] = 0;
        }
    }
}

void BoardState::init_from_main(Node *main_node) {
    if (main_node == nullptr) {
        return;
    }

    // 盤上の駒を読み込み
    Array board_grid = main_node->get("board_grid");

    for (int col = 0; col < Shogi::BOARD_COLS; ++col) {
        if (col >= board_grid.size()) {
            break;
        }

        Array row_array = board_grid[col];

        for (int row = 0; row < Shogi::BOARD_ROWS; ++row) {
            if (row >= row_array.size()) {
                break;
            }

            Variant cell_data = row_array[row];
            Object *piece = Object::cast_to<Object>(cell_data);

            int index = col * Shogi::BOARD_ROWS + row;

            if (piece != nullptr) {
                int piece_type = piece->get("piece_type");
                bool is_enemy = piece->get("is_enemy");
                bool is_promoted = piece->get("is_promoted");

                board[index] = Cell(piece_type, is_enemy ? Shogi::ENEMY : Shogi::PLAYER, is_promoted);
            } else {
                board[index] = Cell();
            }
        }
    }

    // 持ち駒を読み込み
    Node *stands[2];
    stands[Shogi::PLAYER] = Object::cast_to<Node>(main_node->get("player_piece_stand"));
    stands[Shogi::ENEMY] = Object::cast_to<Node>(main_node->get("enemy_piece_stand"));

    for (int side = 0; side < 2; ++side) {
        if (stands[side] == nullptr) {
            continue;
        }

        Array children = stands[side]->get_children();
        for (int i = 0; i < children.size(); ++i) {
            Object *piece = Object::cast_to<Object>(children[i]);
            if (piece != nullptr) {
                Variant v_type = piece->get("piece_type");
                if (v_type.get_type() == Variant::INT) {
                    int piece_type = v_type;
                    if (piece_type >= 0 && piece_type < Shogi::PIECE_TYPE_COUNT) {
                        hand[side][piece_type]++;
                    }
                }
            }
        }
    }
}

std::vector<Shogi::Move> BoardState::get_legal_moves(int side) const {
    std::vector<Shogi::Move> moves;
    moves.reserve(100);

    // 進行方向
    int dy_sign = (side == Shogi::PLAYER) ? 1 : -1;

    // 盤上の駒
    for (int col = 0; col < Shogi::BOARD_COLS; ++col) {
        for (int row = 0; row < Shogi::BOARD_ROWS; ++row) {
            const Cell &cell = get_cell(col, row);

            // 自駒でなければスキップ
            if (cell.is_empty() || cell.side != side) {
                continue;
            }

            const std::vector<Direction> *step_dirs = nullptr;
            const std::vector<Direction> *slide_dirs = nullptr;

            if (cell.is_promoted) {
                switch (cell.type) {
                case Shogi::ROOK:
                    slide_dirs = &MOVES_ROOK_SLIDE;
                    step_dirs = &MOVES_KING;
                    break;
                case Shogi::BISHOP:
                    slide_dirs = &MOVES_BISHOP_SLIDE;
                    step_dirs = &MOVES_KING;
                    break;
                default:
                    step_dirs = &MOVES_GOLD;
                    break;
                }
            } else {
                switch (cell.type) {
                case Shogi::KING:
                    step_dirs = &MOVES_KING;
                    break;
                case Shogi::ROOK:
                    slide_dirs = &MOVES_ROOK_SLIDE;
                    break;
                case Shogi::BISHOP:
                    slide_dirs = &MOVES_BISHOP_SLIDE;
                    break;
                case Shogi::GOLD:
                    step_dirs = &MOVES_GOLD;
                    break;
                case Shogi::SILVER:
                    step_dirs = &MOVES_SILVER;
                    break;
                case Shogi::KNIGHT:
                    step_dirs = &MOVES_KNIGHT;
                    break;
                case Shogi::LANCE:
                    slide_dirs = &MOVES_LANCE_SLIDE;
                    break;
                case Shogi::PAWN:
                    step_dirs = &MOVES_PAWN;
                    break;
                }
            }

            if (step_dirs) {
                for (const auto &dir : *step_dirs) {
                    int tx = col + dir.dx;
                    int ty = row + dir.dy * dy_sign;

                    if (!is_valid_coord(tx, ty)) {
                        continue;
                    }

                    const Cell &target = get_cell(tx, ty);

                    // 味方の駒がいれば移動不可
                    if (!target.is_empty() && target.side == side) {
                        continue;
                    }

                    bool is_capture = !target.is_empty();

                    bool can_promote = false;
                    if (!cell.is_promoted && cell.type != Shogi::KING && cell.type != Shogi::GOLD) {
                        if ((side == Shogi::PLAYER && (ty <= 2 || row <= 2)) ||
                            (side == Shogi::ENEMY && (ty >= 6 || row >= 6))) {
                            can_promote = true;
                        }
                    }

                    moves.emplace_back(col, row, tx, ty, cell.type, false, false, is_capture);
                    if (can_promote) {
                        moves.emplace_back(col, row, tx, ty, cell.type, true, false, is_capture);
                    }
                }
            }

            if (slide_dirs) {
                for (const auto &dir : *slide_dirs) {
                    int tx = col;
                    int ty = row;

                    while (true) {
                        tx += dir.dx;
                        ty += dir.dy * dy_sign;

                        if (!is_valid_coord(tx, ty)) {
                            break;
                        }

                        const Cell &target = get_cell(tx, ty);

                        if (!target.is_empty()) {
                            if (target.side != side) {
                                bool can_promote = false;
                                if (!cell.is_promoted) {
                                    if ((side == Shogi::PLAYER && (ty <= 2 || row <= 2)) ||
                                        (side == Shogi::ENEMY && (ty >= 6 || row >= 6))) {
                                        can_promote = true;
                                    }
                                }

                                moves.emplace_back(col, row, tx, ty, cell.type, false, false, true);
                                if (can_promote) {
                                    moves.emplace_back(col, row, tx, ty, cell.type, true, false, true);
                                }
                            }

                            break;
                        }

                        bool can_promote = false;
                        if (!cell.is_promoted) {
                            if ((side == Shogi::PLAYER && (ty <= 2 || row <= 2)) ||
                                (side == Shogi::ENEMY && (ty >= 6 || row >= 6))) {
                                can_promote = true;
                            }
                        }

                        moves.emplace_back(col, row, tx, ty, cell.type, false, false, false);
                        if (can_promote) {
                            moves.emplace_back(col, row, tx, ty, cell.type, true, false, false);
                        }
                    }
                }
            }
        }
    }

    // 持ち駒
    for (int piece_type = 0; piece_type < Shogi::PIECE_TYPE_COUNT; ++piece_type) {
        if (get_hand_count(side, piece_type) > 0) {
            bool is_pawn = (piece_type == Shogi::PAWN);

            for (int col = 0; col < Shogi::BOARD_COLS; ++col) {
                // 二歩になるマスには打てない
                if (is_pawn && has_pawn_on_column(side, col)) {
                    continue;
                }

                for (int row = 0; row < Shogi::BOARD_ROWS; ++row) {
                    const Cell &target = get_cell(col, row);

                    // 駒があるマスには打てない
                    if (!target.is_empty()) {
                        continue;
                    }

                    if (is_pawn) {
                        // TODO: 香車と桂馬の打ち場所制限も追加
                        // 行き所のないマスには打てない
                        if ((side == Shogi::PLAYER && row == 0) ||
                            (side == Shogi::ENEMY && row == Shogi::BOARD_ROWS - 1)) {
                            continue;
                        }
                    }

                    moves.emplace_back(-1, -1, col, row, piece_type, false, true, false);
                }
            }
        }
    }

    return moves;
}

const Cell &BoardState::get_cell(int col, int row) const {
    if (col < 0 || col >= Shogi::BOARD_COLS || row < 0 || row >= Shogi::BOARD_ROWS) {
        // 範囲外のアクセスなら空のセルを返す
        static Cell empty_cell;
        return empty_cell;
    }

    return board[col * Shogi::BOARD_ROWS + row];
}

int BoardState::get_hand_count(int side, int piece_type) const {
    if (side < 0 || side >= 2 || piece_type < 0 || piece_type >= Shogi::PIECE_TYPE_COUNT) {
        return 0;
    }
    return hand[side][piece_type];
}

bool BoardState::has_pawn_on_column(int side, int col) const {
    for (int row = 0; row < Shogi::BOARD_ROWS; ++row) {
        const Cell &cell = get_cell(col, row);
        if (!cell.is_empty() && cell.side == side && cell.type == Shogi::PAWN && !cell.is_promoted) {
            return true;
        }
    }

    return false;
}

void BoardState::print_board() const {
    UtilityFunctions::print("--- Board State ---");
    for (int row = 0; row < Shogi::BOARD_ROWS; ++row) {
        String line = "";
        for (int col = 0; col < Shogi::BOARD_COLS; ++col) {
            const Cell &cell = get_cell(col, row);
            if (cell.is_empty()) {
                line += ". ";
            } else {
                String piece_str = String::num_int64(cell.type);
                if (cell.is_promoted) {
                    piece_str += "+";
                }
                if (cell.side == Shogi::ENEMY) {
                    piece_str = piece_str.to_upper();
                }
                line += piece_str + " ";
            }
        }
        UtilityFunctions::print(line);
    }

    UtilityFunctions::print("Player Hand:");
    for (int piece_type = 0; piece_type < Shogi::PIECE_TYPE_COUNT; ++piece_type) {
        UtilityFunctions::print("Type " + String::num_int64(piece_type) + ": " +
                                String::num_int64(hand[Shogi::PLAYER][piece_type]));
    }

    UtilityFunctions::print("Enemy Hand:");
    for (int piece_type = 0; piece_type < Shogi::PIECE_TYPE_COUNT; ++piece_type) {
        UtilityFunctions::print("Type " + String::num_int64(piece_type) + ": " +
                                String::num_int64(hand[Shogi::ENEMY][piece_type]));
    }
}
