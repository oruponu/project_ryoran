#include "board_state.hpp"
#include <array>
#include <godot_cpp/classes/file_access.hpp>
#include <godot_cpp/classes/node.hpp>
#include <godot_cpp/variant/utility_functions.hpp>

using namespace godot;

namespace {

struct Direction {
    int dx;
    int dy;

    constexpr bool operator==(const Direction &other) const { return dx == other.dx && dy == other.dy; }
};

constexpr Direction DIR_UP = {0, -1};
constexpr Direction DIR_UP_RIGHT = {1, -1};
constexpr Direction DIR_RIGHT = {1, 0};
constexpr Direction DIR_DOWN_RIGHT = {1, 1};
constexpr Direction DIR_DOWN = {0, 1};
constexpr Direction DIR_DOWN_LEFT = {-1, 1};
constexpr Direction DIR_LEFT = {-1, 0};
constexpr Direction DIR_UP_LEFT = {-1, -1};
constexpr Direction DIR_KNIGHT_LEFT = {-1, -2};
constexpr Direction DIR_KNIGHT_RIGHT = {1, -2};

constexpr std::array MOVES_PAWN = {DIR_UP};
constexpr std::array MOVES_KNIGHT = {DIR_KNIGHT_LEFT, DIR_KNIGHT_RIGHT};
constexpr std::array MOVES_SILVER = {DIR_UP_LEFT, DIR_UP, DIR_UP_RIGHT, DIR_DOWN_LEFT, DIR_DOWN_RIGHT};
constexpr std::array MOVES_GOLD = {DIR_UP_LEFT, DIR_UP, DIR_UP_RIGHT, DIR_LEFT, DIR_RIGHT, DIR_DOWN};
constexpr std::array MOVES_KING = {DIR_UP_LEFT, DIR_UP,        DIR_UP_RIGHT, DIR_LEFT,
                                   DIR_RIGHT,   DIR_DOWN_LEFT, DIR_DOWN,     DIR_DOWN_RIGHT};

uint64_t z_board[2][Shogi::PIECE_TYPE_COUNT][2][Shogi::BOARD_COLS][Shogi::BOARD_ROWS];
uint64_t z_hand[2][Shogi::PIECE_TYPE_COUNT][20];
uint64_t z_turn_enemy;
bool z_initialized = false;

} // namespace

BoardState::BoardState(Shogi::Side side_to_move) : side_to_move(side_to_move) {
    // 盤面を初期化
    for (int i = 0; i < Shogi::BOARD_SIZE; ++i) {
        board[i] = Cell();
    }

    // 持ち駒を初期化
    for (Shogi::Side side : {Shogi::PLAYER, Shogi::ENEMY}) {
        for (int piece_type = 0; piece_type < Shogi::PIECE_TYPE_COUNT; ++piece_type) {
            hand[side][piece_type] = 0;
        }
    }

    zobrist_hash = calculate_zobrist_hash();
}

BoardState::BoardState(Node *main_node, Shogi::Side side_to_move) : BoardState(side_to_move) {
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

    for (Shogi::Side side : {Shogi::PLAYER, Shogi::ENEMY}) {
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

    zobrist_hash = calculate_zobrist_hash();
}

void BoardState::load_zobrist_params(const String &path) {
    if (z_initialized) {
        return;
    }

    if (!FileAccess::file_exists(path)) {
        UtilityFunctions::print("Zobrist params file not found: " + path);
        return;
    }

    Ref<FileAccess> file = FileAccess::open(path, FileAccess::READ);
    if (file->get_32() != 0x5A4F4252) { // "ZOBR"
        UtilityFunctions::print("Invalid Zobrist params file format.");
        return;
    }

    // 盤上の駒
    for (Shogi::Side side : {Shogi::PLAYER, Shogi::ENEMY}) {
        for (int piece_type = 0; piece_type < Shogi::PIECE_TYPE_COUNT; ++piece_type) {
            for (int is_promoted = 0; is_promoted < 2; ++is_promoted) {
                for (int col = 0; col < Shogi::BOARD_COLS; ++col) {
                    for (int row = 0; row < Shogi::BOARD_ROWS; ++row) {
                        z_board[side][piece_type][is_promoted][col][row] = file->get_64();
                    }
                }
            }
        }
    }

    // 持ち駒
    for (Shogi::Side side : {Shogi::PLAYER, Shogi::ENEMY}) {
        for (int piece_type = 0; piece_type < Shogi::PIECE_TYPE_COUNT; ++piece_type) {
            for (int n = 0; n < 20; ++n) {
                z_hand[side][piece_type][n] = file->get_64();
            }
        }
    }

    z_turn_enemy = file->get_64();
    z_initialized = true;

    UtilityFunctions::print("Zobrist parameters loaded successfully.");
}

uint64_t BoardState::calculate_zobrist_hash() const {
    uint64_t hash = 0;

    // 盤上の駒
    for (int col = 0; col < Shogi::BOARD_COLS; ++col) {
        for (int row = 0; row < Shogi::BOARD_ROWS; ++row) {
            const Cell &cell = get_cell({col, row});
            if (!cell.is_empty()) {
                int is_promoted = cell.is_promoted ? 1 : 0;
                hash ^= z_board[cell.side][cell.type][is_promoted][col][row];
            }
        }
    }

    // 持ち駒
    for (Shogi::Side side : {Shogi::PLAYER, Shogi::ENEMY}) {
        for (int piece_type = 0; piece_type < Shogi::PIECE_TYPE_COUNT; ++piece_type) {
            int count = hand[side][piece_type];
            if (count > 0) {
                int index = (count >= 20) ? 19 : count;
                hash ^= z_hand[side][piece_type][index];
            }
        }
    }

    if (side_to_move == Shogi::ENEMY) {
        hash ^= z_turn_enemy;
    }

    return hash;
}

bool BoardState::is_valid_move(Shogi::Coord from, Shogi::Coord to) const {
    // 盤面の範囲外には移動不可
    if (!to.is_valid()) {
        return false;
    }

    // 現在地と同じ場所には移動不可
    if (from == to) {
        return false;
    }

    const Cell &piece = get_cell(from);
    bool is_enemy = (piece.side == Shogi::ENEMY);

    // ルールで認められていない場所には移動不可
    if (!can_move_geometry(piece.type, is_enemy, piece.is_promoted, from, to)) {
        return false;
    }

    if (piece.type != Shogi::KNIGHT) {
        if (is_path_blocked(from, to)) {
            return false;
        }
    }

    // 味方の駒がある場所には移動不可
    const Cell &target = get_cell(to);
    if (!target.is_empty() && target.side == piece.side) {
        return false;
    }

    return true;
}

bool BoardState::is_valid_drop(int piece_type, bool is_enemy, Shogi::Coord to) const {
    // 盤面の範囲外には配置不可
    if (!to.is_valid()) {
        return false;
    }

    // すでに駒がある場所には配置不可
    if (!get_cell(to).is_empty()) {
        return false;
    }

    Shogi::Side side = is_enemy ? Shogi::ENEMY : Shogi::PLAYER;
    if (get_hand_count(side, piece_type) <= 0) {
        return false;
    }

    // 行き所のない場所には配置不可
    if (is_dead_end(piece_type, is_enemy, to.row)) {
        return false;
    }

    // 二歩になる場所には配置不可
    if (is_nifu(piece_type, side, to.col)) {
        return false;
    }

    return true;
}

bool BoardState::is_legal_move(Shogi::Coord from, Shogi::Coord to) const {
    if (!is_valid_move(from, to)) {
        return false;
    }

    BoardState next_state = *this;
    Cell piece = next_state.get_cell(from);
    next_state.set_cell(to, piece.type, piece.side, piece.is_promoted);
    next_state.clear_cell(from);

    // 王手放置になる手を除外
    if (next_state.is_king_in_check(piece.side)) {
        return false;
    }

    return true;
}

bool BoardState::is_legal_drop(int piece_type, bool is_enemy, Shogi::Coord to) const {
    if (!is_valid_drop(piece_type, is_enemy, to)) {
        return false;
    }

    BoardState next_state = *this;
    Shogi::Side side = is_enemy ? Shogi::ENEMY : Shogi::PLAYER;
    next_state.set_cell(to, piece_type, side, false);
    next_state.hand[side][piece_type]--;

    // 王手放置になる手を除外
    if (next_state.is_king_in_check(side)) {
        return false;
    }

    return true;
}

bool BoardState::can_move_geometry(int piece_type, bool is_enemy, bool is_promoted, Shogi::Coord from,
                                   Shogi::Coord to) const {
    int dx = to.col - from.col;
    int dy = to.row - from.row;

    if (is_enemy) {
        dx = -dx;
        dy = -dy;
    }

    int effective_type = piece_type;

    if (is_promoted) {
        switch (piece_type) {
        case Shogi::SILVER:
        case Shogi::KNIGHT:
        case Shogi::LANCE:
        case Shogi::PAWN:
            effective_type = Shogi::GOLD;
            break;
        default:
            break;
        }
    }

    int abs_dx = std::abs(dx);
    int abs_dy = std::abs(dy);

    switch (effective_type) {
    case Shogi::ROOK:
        if (dx == 0 || dy == 0) {
            return true;
        }
        if (is_promoted && abs_dx <= 1 && abs_dy <= 1) {
            return true;
        }

        return false;
    case Shogi::BISHOP:
        if (abs_dx == abs_dy) {
            return true;
        }
        if (is_promoted && abs_dx + abs_dy <= 1) {
            return true;
        }
        return false;
    case Shogi::LANCE:
        return (dx == 0 && dy < 0);
    default:
        Direction move_dir = {dx, dy};

        auto check_moves = [&move_dir](const auto &moves) {
            for (const auto &def : moves) {
                if (def == move_dir) {
                    return true;
                }
            }
            return false;
        };

        switch (effective_type) {
        case Shogi::KING:
            return check_moves(MOVES_KING);
        case Shogi::GOLD:
            return check_moves(MOVES_GOLD);
        case Shogi::SILVER:
            return check_moves(MOVES_SILVER);
        case Shogi::KNIGHT:
            return check_moves(MOVES_KNIGHT);
        case Shogi::PAWN:
            return check_moves(MOVES_PAWN);
        default:
            return false;
        }
    }
}

bool BoardState::is_path_blocked(Shogi::Coord from, Shogi::Coord to) const {
    int dx = to.col - from.col;
    int dy = to.row - from.row;
    int steps = std::max(std::abs(dx), std::abs(dy));

    if (steps <= 1) {
        return false;
    }

    int step_x = (dx == 0) ? 0 : (dx > 0 ? 1 : -1);
    int step_y = (dy == 0) ? 0 : (dy > 0 ? 1 : -1);

    for (int i = 1; i < steps; ++i) {
        Shogi::Coord check{from.col + step_x * i, from.row + step_y * i};
        if (!get_cell(check).is_empty()) {
            return true;
        }
    }

    return false;
}

bool BoardState::is_dead_end(int piece_type, bool is_enemy, int to_row) const {
    int relative_row = is_enemy ? (Shogi::BOARD_ROWS - 1 - to_row) : to_row;
    switch (piece_type) {
    case Shogi::PAWN:
    case Shogi::LANCE:
        return relative_row == 0;
    case Shogi::KNIGHT:
        return relative_row <= 1;
    default:
        return false;
    }
}

bool BoardState::is_nifu(int piece_type, Shogi::Side side, int col) const {
    if (piece_type != Shogi::PAWN) {
        return false;
    }

    for (int row = 0; row < Shogi::BOARD_ROWS; ++row) {
        const Cell &cell = get_cell({col, row});
        if (!cell.is_empty() && cell.side == side && cell.type == Shogi::PAWN && !cell.is_promoted) {
            return true;
        }
    }

    return false;
}

bool BoardState::is_king_in_check(Shogi::Side side) const {
    if (auto king_pos = find_king_position(side); !king_pos) {
        return false;
    } else {
        Shogi::Side enemy_side = (side == Shogi::PLAYER) ? Shogi::ENEMY : Shogi::PLAYER;

        for (int col = 0; col < Shogi::BOARD_COLS; ++col) {
            for (int row = 0; row < Shogi::BOARD_ROWS; ++row) {
                Shogi::Coord coord{col, row};
                if (const Cell &cell = get_cell(coord);
                    !cell.is_empty() && cell.side == enemy_side && is_valid_move(coord, *king_pos)) {
                    return true;
                }
            }
        }

        return false;
    }
}

uint64_t BoardState::get_zobrist_hash() const { return zobrist_hash; }

std::optional<Shogi::Coord> BoardState::find_king_position(Shogi::Side side) const {
    for (int col = 0; col < Shogi::BOARD_COLS; ++col) {
        for (int row = 0; row < Shogi::BOARD_ROWS; ++row) {
            Shogi::Coord coord{col, row};
            const Cell &cell = get_cell(coord);
            if (cell.side == side && cell.type == Shogi::KING) {
                return coord;
            }
        }
    }

    return std::nullopt;
}

const Cell &BoardState::get_cell(Shogi::Coord coord) const {
    if (!coord.is_valid()) {
        // 範囲外のアクセスなら空のセルを返す
        static Cell empty_cell;
        return empty_cell;
    }

    return board[coord.col * Shogi::BOARD_ROWS + coord.row];
}

void BoardState::set_cell(Shogi::Coord coord, int type, Shogi::Side side, bool is_promoted) {
    if (!coord.is_valid()) {
        return;
    }

    Cell old_cell = get_cell(coord);
    if (!old_cell.is_empty()) {
        int old_is_promoted = old_cell.is_promoted ? 1 : 0;
        zobrist_hash ^= z_board[old_cell.side][old_cell.type][old_is_promoted][coord.col][coord.row];
    }

    int new_is_promoted = is_promoted ? 1 : 0;
    zobrist_hash ^= z_board[side][type][new_is_promoted][coord.col][coord.row];

    int index = coord.col * Shogi::BOARD_ROWS + coord.row;
    board[index] = Cell(type, side, is_promoted);
}

void BoardState::clear_cell(Shogi::Coord coord) {
    if (!coord.is_valid()) {
        return;
    }

    Cell old_cell = get_cell(coord);
    if (!old_cell.is_empty()) {
        int old_is_promoted = old_cell.is_promoted ? 1 : 0;
        zobrist_hash ^= z_board[old_cell.side][old_cell.type][old_is_promoted][coord.col][coord.row];
    }

    int index = coord.col * Shogi::BOARD_ROWS + coord.row;
    board[index] = Cell();
}

int BoardState::get_hand_count(Shogi::Side side, int piece_type) const {
    if (side < 0 || side >= 2 || piece_type < 0 || piece_type >= Shogi::PIECE_TYPE_COUNT) {
        return 0;
    }
    return hand[side][piece_type];
}

void BoardState::apply_move(const Shogi::Move &move) {
    Shogi::Side current_side = side_to_move;
    Shogi::Side opponent_side = (side_to_move == Shogi::PLAYER) ? Shogi::ENEMY : Shogi::PLAYER;

    int from_idx = move.from_col * Shogi::BOARD_ROWS + move.from_row;
    int to_idx = move.to_col * Shogi::BOARD_ROWS + move.to_row;

    if (move.is_drop) {
        int piece_type = move.piece_type;
        int count = hand[current_side][piece_type];

        int idx_old = (count >= 20) ? 19 : count;
        int idx_new = ((count - 1) >= 20) ? 19 : (count - 1);
        zobrist_hash ^= z_hand[current_side][piece_type][idx_old];
        zobrist_hash ^= z_hand[current_side][piece_type][idx_new];
        if (hand[current_side][move.piece_type] > 0) {
            hand[current_side][move.piece_type]--;
        }

        zobrist_hash ^= z_board[current_side][piece_type][0][move.to_col][move.to_row];
        board[to_idx] = Cell(piece_type, current_side, false);
    } else {
        Cell source = get_cell({move.from_col, move.from_row});
        Cell target = get_cell({move.to_col, move.to_row});

        int src_is_promoted = source.is_promoted ? 1 : 0;
        zobrist_hash ^= z_board[current_side][source.type][src_is_promoted][move.from_col][move.from_row];

        if (!target.is_empty()) {
            int tgt_is_promoted = target.is_promoted ? 1 : 0;
            zobrist_hash ^= z_board[opponent_side][target.type][tgt_is_promoted][move.to_col][move.to_row];

            int captured_type = target.type;
            int count = hand[current_side][captured_type];
            int idx_old = (count >= 20) ? 19 : count;
            int idx_new = ((count + 1) >= 20) ? 19 : (count + 1);
            zobrist_hash ^= z_hand[current_side][captured_type][idx_old];
            zobrist_hash ^= z_hand[current_side][captured_type][idx_new];
            hand[current_side][captured_type]++;
        }

        bool is_promoted = move.is_promotion || source.is_promoted;
        int new_is_promoted = is_promoted ? 1 : 0;
        zobrist_hash ^= z_board[current_side][source.type][new_is_promoted][move.to_col][move.to_row];

        board[to_idx] = Cell(source.type, current_side, is_promoted);
        board[from_idx] = Cell();
    }

    zobrist_hash ^= z_turn_enemy;
    side_to_move = opponent_side;
}

void BoardState::print_board() const {
    UtilityFunctions::print("--- Board State ---");
    for (int row = 0; row < Shogi::BOARD_ROWS; ++row) {
        String line = "";
        for (int col = 0; col < Shogi::BOARD_COLS; ++col) {
            const Cell &cell = get_cell({col, row});
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
