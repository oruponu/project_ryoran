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
const std::vector<Direction> MOVES_KNIGHT = {DIR_KNIGHT_LEFT, DIR_KNIGHT_RIGHT};
const std::vector<Direction> MOVES_SILVER = {DIR_UP_LEFT, DIR_UP, DIR_UP_RIGHT, DIR_DOWN_LEFT, DIR_DOWN_RIGHT};
const std::vector<Direction> MOVES_GOLD = {DIR_UP_LEFT, DIR_UP, DIR_UP_RIGHT, DIR_LEFT, DIR_RIGHT, DIR_DOWN};
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
                int piece_type = piece->get("piece_type");
                if (piece_type >= 0 && piece_type < Shogi::PIECE_TYPE_COUNT) {
                    hand[side][piece_type]++;
                }
            }
        }
    }
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
