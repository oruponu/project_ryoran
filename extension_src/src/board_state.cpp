#include "board_state.hpp"
#include <algorithm>
#include <array>
#include <godot_cpp/classes/file_access.hpp>
#include <godot_cpp/classes/node.hpp>
#include <godot_cpp/variant/utility_functions.hpp>

using namespace godot;
using Shogi::Coord;
using Shogi::Move;
using Shogi::PieceType;
using Shogi::Turn;

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

uint64_t g_zobrist_board[2][Shogi::PIECE_TYPE_COUNT][2][Shogi::BOARD_COLS][Shogi::BOARD_ROWS];
uint64_t g_zobrist_hand[2][Shogi::PIECE_TYPE_COUNT][20];
uint64_t g_zobrist_turn_enemy;
bool g_zobrist_initialized = false;

} // namespace

BoardState::BoardState(Turn turn_to_move) : turn_to_move_(turn_to_move) {
    // 盤面を初期化
    for (int i = 0; i < Shogi::BOARD_SIZE; ++i) {
        board_[i] = Cell();
    }

    // 持ち駒を初期化
    for (Turn turn : {Turn::SENTE, Turn::GOTE}) {
        for (int piece_type = 0; piece_type < Shogi::PIECE_TYPE_COUNT; ++piece_type) {
            hand_[static_cast<int>(turn)][piece_type] = 0;
        }
    }

    zobrist_hash_ = calculate_zobrist_hash();
}

BoardState::BoardState(Node *main_node, Turn turn_to_move) : BoardState(turn_to_move) {
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

                board_[index] =
                    Cell(static_cast<PieceType>(piece_type), is_enemy ? Turn::GOTE : Turn::SENTE, is_promoted);
            } else {
                board_[index] = Cell();
            }
        }
    }

    // 持ち駒を読み込み
    Node *stands[2];
    stands[static_cast<int>(Turn::SENTE)] = Object::cast_to<Node>(main_node->get("player_piece_stand"));
    stands[static_cast<int>(Turn::GOTE)] = Object::cast_to<Node>(main_node->get("enemy_piece_stand"));

    for (Turn turn : {Turn::SENTE, Turn::GOTE}) {
        if (stands[static_cast<int>(turn)] == nullptr) {
            continue;
        }

        Array children = stands[static_cast<int>(turn)]->get_children();
        for (int i = 0; i < children.size(); ++i) {
            Object *piece = Object::cast_to<Object>(children[i]);
            if (piece != nullptr) {
                Variant v_type = piece->get("piece_type");
                if (v_type.get_type() == Variant::INT) {
                    int piece_type = v_type;
                    if (piece_type >= 0 && piece_type < Shogi::PIECE_TYPE_COUNT) {
                        hand_[static_cast<int>(turn)][piece_type]++;
                    }
                }
            }
        }
    }

    zobrist_hash_ = calculate_zobrist_hash();
}

void BoardState::load_zobrist_params(const String &path) {
    if (g_zobrist_initialized) {
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
    for (Turn turn : {Turn::SENTE, Turn::GOTE}) {
        for (int piece_type = 0; piece_type < Shogi::PIECE_TYPE_COUNT; ++piece_type) {
            for (int is_promoted = 0; is_promoted < 2; ++is_promoted) {
                for (int col = 0; col < Shogi::BOARD_COLS; ++col) {
                    for (int row = 0; row < Shogi::BOARD_ROWS; ++row) {
                        g_zobrist_board[static_cast<int>(turn)][piece_type][is_promoted][col][row] = file->get_64();
                    }
                }
            }
        }
    }

    // 持ち駒
    for (Turn turn : {Turn::SENTE, Turn::GOTE}) {
        for (int piece_type = 0; piece_type < Shogi::PIECE_TYPE_COUNT; ++piece_type) {
            for (int n = 0; n < 20; ++n) {
                g_zobrist_hand[static_cast<int>(turn)][piece_type][n] = file->get_64();
            }
        }
    }

    g_zobrist_turn_enemy = file->get_64();
    g_zobrist_initialized = true;

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
                hash ^=
                    g_zobrist_board[static_cast<int>(cell.turn)][static_cast<int>(cell.type)][is_promoted][col][row];
            }
        }
    }

    // 持ち駒
    for (Turn turn : {Turn::SENTE, Turn::GOTE}) {
        for (int piece_type = 0; piece_type < Shogi::PIECE_TYPE_COUNT; ++piece_type) {
            int count = hand_[static_cast<int>(turn)][piece_type];
            if (count > 0) {
                int index = (count >= 20) ? 19 : count;
                hash ^= g_zobrist_hand[static_cast<int>(turn)][piece_type][index];
            }
        }
    }

    if (turn_to_move_ == Turn::GOTE) {
        hash ^= g_zobrist_turn_enemy;
    }

    return hash;
}

bool BoardState::is_valid_move(Coord from, Coord to) const {
    // 盤面の範囲外には移動不可
    if (!to.is_valid()) {
        return false;
    }

    // 現在地と同じ場所には移動不可
    if (from == to) {
        return false;
    }

    const Cell &piece = get_cell(from);
    bool is_enemy = (piece.turn == Turn::GOTE);

    // ルールで認められていない場所には移動不可
    if (!can_move_geometry(piece.type, is_enemy, piece.is_promoted, from, to)) {
        return false;
    }

    if (piece.type != PieceType::KNIGHT) {
        if (is_path_blocked(from, to)) {
            return false;
        }
    }

    // 味方の駒がある場所には移動不可
    const Cell &target = get_cell(to);
    if (!target.is_empty() && target.turn == piece.turn) {
        return false;
    }

    return true;
}

bool BoardState::is_valid_drop(PieceType piece_type, bool is_enemy, Coord to) const {
    // 盤面の範囲外には配置不可
    if (!to.is_valid()) {
        return false;
    }

    // すでに駒がある場所には配置不可
    if (!get_cell(to).is_empty()) {
        return false;
    }

    Turn turn = is_enemy ? Turn::GOTE : Turn::SENTE;
    if (get_hand_count(turn, piece_type) <= 0) {
        return false;
    }

    // 行き所のない場所には配置不可
    if (is_dead_end(piece_type, is_enemy, to.row)) {
        return false;
    }

    // 二歩になる場所には配置不可
    if (is_nifu(piece_type, turn, to.col)) {
        return false;
    }

    return true;
}

bool BoardState::is_legal_move(Coord from, Coord to) const {
    if (!is_valid_move(from, to)) {
        return false;
    }

    BoardState next_state = *this;
    Cell piece = next_state.get_cell(from);
    next_state.set_cell(to, piece.type, piece.turn, piece.is_promoted);
    next_state.clear_cell(from);

    // 王手放置になる手を除外
    if (next_state.is_king_in_check(piece.turn)) {
        return false;
    }

    return true;
}

bool BoardState::is_legal_drop(PieceType piece_type, bool is_enemy, Coord to) const {
    if (!is_valid_drop(piece_type, is_enemy, to)) {
        return false;
    }

    BoardState next_state = *this;
    Turn turn = is_enemy ? Turn::GOTE : Turn::SENTE;
    next_state.set_cell(to, piece_type, turn, false);
    next_state.hand_[static_cast<int>(turn)][static_cast<int>(piece_type)]--;

    // 王手放置になる手を除外
    if (next_state.is_king_in_check(turn)) {
        return false;
    }

    return true;
}

bool BoardState::can_move_geometry(PieceType piece_type, bool is_enemy, bool is_promoted, Coord from, Coord to) const {
    int dx = to.col - from.col;
    int dy = to.row - from.row;

    if (is_enemy) {
        dx = -dx;
        dy = -dy;
    }

    PieceType effective_type = piece_type;

    if (is_promoted) {
        switch (piece_type) {
        case PieceType::SILVER:
        case PieceType::KNIGHT:
        case PieceType::LANCE:
        case PieceType::PAWN:
            effective_type = PieceType::GOLD;
            break;
        default:
            break;
        }
    }

    int abs_dx = std::abs(dx);
    int abs_dy = std::abs(dy);

    switch (effective_type) {
    case PieceType::ROOK:
        if (dx == 0 || dy == 0) {
            return true;
        }
        if (is_promoted && abs_dx <= 1 && abs_dy <= 1) {
            return true;
        }

        return false;
    case PieceType::BISHOP:
        if (abs_dx == abs_dy) {
            return true;
        }
        if (is_promoted && abs_dx + abs_dy <= 1) {
            return true;
        }
        return false;
    case PieceType::LANCE:
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
        case PieceType::KING:
            return check_moves(MOVES_KING);
        case PieceType::GOLD:
            return check_moves(MOVES_GOLD);
        case PieceType::SILVER:
            return check_moves(MOVES_SILVER);
        case PieceType::KNIGHT:
            return check_moves(MOVES_KNIGHT);
        case PieceType::PAWN:
            return check_moves(MOVES_PAWN);
        default:
            return false;
        }
    }
}

bool BoardState::is_path_blocked(Coord from, Coord to) const {
    int dx = to.col - from.col;
    int dy = to.row - from.row;
    int steps = std::max(std::abs(dx), std::abs(dy));

    if (steps <= 1) {
        return false;
    }

    int step_x = (dx == 0) ? 0 : (dx > 0 ? 1 : -1);
    int step_y = (dy == 0) ? 0 : (dy > 0 ? 1 : -1);

    for (int i = 1; i < steps; ++i) {
        Coord check{from.col + step_x * i, from.row + step_y * i};
        if (!get_cell(check).is_empty()) {
            return true;
        }
    }

    return false;
}

bool BoardState::is_dead_end(PieceType piece_type, bool is_enemy, int to_row) const {
    int relative_row = is_enemy ? (Shogi::BOARD_ROWS - 1 - to_row) : to_row;
    switch (piece_type) {
    case PieceType::PAWN:
    case PieceType::LANCE:
        return relative_row == 0;
    case PieceType::KNIGHT:
        return relative_row <= 1;
    default:
        return false;
    }
}

bool BoardState::is_nifu(PieceType piece_type, Turn turn, int col) const {
    if (piece_type != PieceType::PAWN) {
        return false;
    }

    for (int row = 0; row < Shogi::BOARD_ROWS; ++row) {
        const Cell &cell = get_cell({col, row});
        if (!cell.is_empty() && cell.turn == turn && cell.type == PieceType::PAWN && !cell.is_promoted) {
            return true;
        }
    }

    return false;
}

bool BoardState::is_king_in_check(Turn turn) const {
    if (auto king_pos = find_king_position(turn); !king_pos) {
        return false;
    } else {
        Turn enemy_side = (turn == Turn::SENTE) ? Turn::GOTE : Turn::SENTE;

        for (int col = 0; col < Shogi::BOARD_COLS; ++col) {
            for (int row = 0; row < Shogi::BOARD_ROWS; ++row) {
                Coord coord{col, row};
                if (const Cell &cell = get_cell(coord);
                    !cell.is_empty() && cell.turn == enemy_side && is_valid_move(coord, *king_pos)) {
                    return true;
                }
            }
        }

        return false;
    }
}

uint64_t BoardState::get_zobrist_hash() const { return zobrist_hash_; }

std::optional<Coord> BoardState::find_king_position(Turn turn) const {
    for (int col = 0; col < Shogi::BOARD_COLS; ++col) {
        for (int row = 0; row < Shogi::BOARD_ROWS; ++row) {
            Coord coord{col, row};
            const Cell &cell = get_cell(coord);
            if (cell.turn == turn && cell.type == PieceType::KING) {
                return coord;
            }
        }
    }

    return std::nullopt;
}

const Cell &BoardState::get_cell(Coord coord) const {
    if (!coord.is_valid()) {
        // 範囲外のアクセスなら空のセルを返す
        static Cell empty_cell;
        return empty_cell;
    }

    return board_[coord.col * Shogi::BOARD_ROWS + coord.row];
}

void BoardState::set_cell(Coord coord, PieceType type, Turn turn, bool is_promoted) {
    if (!coord.is_valid()) {
        return;
    }

    Cell old_cell = get_cell(coord);
    if (!old_cell.is_empty()) {
        int old_is_promoted = old_cell.is_promoted ? 1 : 0;
        zobrist_hash_ ^= g_zobrist_board[static_cast<int>(old_cell.turn)][static_cast<int>(old_cell.type)]
                                        [old_is_promoted][coord.col][coord.row];
    }

    int new_is_promoted = is_promoted ? 1 : 0;
    zobrist_hash_ ^=
        g_zobrist_board[static_cast<int>(turn)][static_cast<int>(type)][new_is_promoted][coord.col][coord.row];

    int index = coord.col * Shogi::BOARD_ROWS + coord.row;
    board_[index] = Cell(type, turn, is_promoted);
}

void BoardState::clear_cell(Coord coord) {
    if (!coord.is_valid()) {
        return;
    }

    Cell old_cell = get_cell(coord);
    if (!old_cell.is_empty()) {
        int old_is_promoted = old_cell.is_promoted ? 1 : 0;
        zobrist_hash_ ^= g_zobrist_board[static_cast<int>(old_cell.turn)][static_cast<int>(old_cell.type)]
                                        [old_is_promoted][coord.col][coord.row];
    }

    int index = coord.col * Shogi::BOARD_ROWS + coord.row;
    board_[index] = Cell();
}

int BoardState::get_hand_count(Turn turn, PieceType piece_type) const {
    int side_idx = static_cast<int>(turn);
    int type_idx = static_cast<int>(piece_type);
    if (side_idx < 0 || side_idx >= 2 || type_idx < 0 || type_idx >= Shogi::PIECE_TYPE_COUNT) {
        return 0;
    }
    return hand_[side_idx][type_idx];
}

void BoardState::apply_move(const Move &move) {
    Turn current_side = turn_to_move_;
    Turn opponent_side = (turn_to_move_ == Turn::SENTE) ? Turn::GOTE : Turn::SENTE;

    int from_idx = move.from_col * Shogi::BOARD_ROWS + move.from_row;
    int to_idx = move.to_col * Shogi::BOARD_ROWS + move.to_row;

    if (move.is_drop) {
        PieceType piece_type = move.piece_type;
        int count = hand_[static_cast<int>(current_side)][static_cast<int>(piece_type)];

        int idx_old = std::clamp(count, 0, 19);
        int idx_new = std::clamp(count - 1, 0, 19);
        zobrist_hash_ ^= g_zobrist_hand[static_cast<int>(current_side)][static_cast<int>(piece_type)][idx_old];
        zobrist_hash_ ^= g_zobrist_hand[static_cast<int>(current_side)][static_cast<int>(piece_type)][idx_new];
        if (hand_[static_cast<int>(current_side)][static_cast<int>(move.piece_type)] > 0) {
            hand_[static_cast<int>(current_side)][static_cast<int>(move.piece_type)]--;
        }

        zobrist_hash_ ^=
            g_zobrist_board[static_cast<int>(current_side)][static_cast<int>(piece_type)][0][move.to_col][move.to_row];
        board_[to_idx] = Cell(piece_type, current_side, false);
    } else {
        Cell source = get_cell({move.from_col, move.from_row});
        Cell target = get_cell({move.to_col, move.to_row});

        int src_is_promoted = source.is_promoted ? 1 : 0;
        zobrist_hash_ ^= g_zobrist_board[static_cast<int>(current_side)][static_cast<int>(source.type)][src_is_promoted]
                                        [move.from_col][move.from_row];

        if (!target.is_empty()) {
            int tgt_is_promoted = target.is_promoted ? 1 : 0;
            zobrist_hash_ ^= g_zobrist_board[static_cast<int>(opponent_side)][static_cast<int>(target.type)]
                                            [tgt_is_promoted][move.to_col][move.to_row];

            PieceType captured_type = target.type;
            int count = hand_[static_cast<int>(current_side)][static_cast<int>(captured_type)];
            int idx_old = std::clamp(count, 0, 19);
            int idx_new = std::clamp(count + 1, 0, 19);
            zobrist_hash_ ^= g_zobrist_hand[static_cast<int>(current_side)][static_cast<int>(captured_type)][idx_old];
            zobrist_hash_ ^= g_zobrist_hand[static_cast<int>(current_side)][static_cast<int>(captured_type)][idx_new];
            hand_[static_cast<int>(current_side)][static_cast<int>(captured_type)]++;
        }

        bool is_promoted = move.is_promotion || source.is_promoted;
        int new_is_promoted = is_promoted ? 1 : 0;
        zobrist_hash_ ^= g_zobrist_board[static_cast<int>(current_side)][static_cast<int>(source.type)][new_is_promoted]
                                        [move.to_col][move.to_row];

        board_[to_idx] = Cell(source.type, current_side, is_promoted);
        board_[from_idx] = Cell();
    }

    zobrist_hash_ ^= g_zobrist_turn_enemy;
    turn_to_move_ = opponent_side;
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
                String piece_str = String::num_int64(static_cast<int>(cell.type));
                if (cell.is_promoted) {
                    piece_str += "+";
                }
                if (cell.turn == Turn::GOTE) {
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
                                String::num_int64(hand_[static_cast<int>(Turn::SENTE)][piece_type]));
    }

    UtilityFunctions::print("Enemy Hand:");
    for (int piece_type = 0; piece_type < Shogi::PIECE_TYPE_COUNT; ++piece_type) {
        UtilityFunctions::print("Type " + String::num_int64(piece_type) + ": " +
                                String::num_int64(hand_[static_cast<int>(Turn::GOTE)][piece_type]));
    }
}
