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
};

struct StepMoves {
    Direction dirs[8];
    int count;
};

struct SlideMoves {
    Direction dirs[4];
    int count;
};

// 近接駒の移動方向
constexpr StepMoves STEP_PAWN = {{{0, -1}}, 1};
constexpr StepMoves STEP_KNIGHT = {{{-1, -2}, {1, -2}}, 2};
constexpr StepMoves STEP_SILVER = {{{-1, -1}, {0, -1}, {1, -1}, {-1, 1}, {1, 1}}, 5};
constexpr StepMoves STEP_GOLD = {{{-1, -1}, {0, -1}, {1, -1}, {-1, 0}, {1, 0}, {0, 1}}, 6};
constexpr StepMoves STEP_KING = {{{-1, -1}, {0, -1}, {1, -1}, {-1, 0}, {1, 0}, {-1, 1}, {0, 1}, {1, 1}}, 8};

// 走り駒の移動方向
constexpr SlideMoves SLIDE_LANCE = {{{0, -1}}, 1};
constexpr SlideMoves SLIDE_BISHOP = {{{-1, -1}, {1, -1}, {-1, 1}, {1, 1}}, 4};
constexpr SlideMoves SLIDE_ROOK = {{{0, -1}, {0, 1}, {-1, 0}, {1, 0}}, 4};

// 成り駒の追加移動方向
constexpr StepMoves STEP_PROMOTED_BISHOP = {{{0, -1}, {0, 1}, {-1, 0}, {1, 0}}, 4};
constexpr StepMoves STEP_PROMOTED_ROOK = {{{-1, -1}, {1, -1}, {-1, 1}, {1, 1}}, 4};

uint64_t g_zobrist_board[2][Shogi::PIECE_TYPE_COUNT][2][Shogi::BOARD_COLS][Shogi::BOARD_ROWS];
uint64_t g_zobrist_hand[2][Shogi::PIECE_TYPE_COUNT][20];
uint64_t g_zobrist_turn_enemy;
bool g_zobrist_initialized = false;

} // namespace

BoardState::BoardState(Turn turn_to_move) : turn_to_move_(turn_to_move), score_(0) {
    initialize_attack_tables();

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

    king_pos_[static_cast<int>(Turn::SENTE)] = std::nullopt;
    king_pos_[static_cast<int>(Turn::GOTE)] = std::nullopt;
    pawn_columns_[static_cast<int>(Turn::SENTE)] = 0;
    pawn_columns_[static_cast<int>(Turn::GOTE)] = 0;

    zobrist_hash_ = calculate_zobrist_hash();

    build_bitboard();
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

    update_king_position_cache();
    update_pawn_columns_cache();

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
    score_ = calculate_score();

    build_bitboard();
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

void BoardState::initialize_attack_tables() {
    if (attack_tables_initialized_) {
        return;
    }

    auto set_if_valid = [](Bitboard &bitboard, Coord coord) {
        if (coord.is_valid()) {
            bitboard.set(coord.col * Shogi::BOARD_ROWS + coord.row);
        }
    };

    for (int index = 0; index < Shogi::BOARD_SIZE; ++index) {
        int col = index / Shogi::BOARD_ROWS;
        int row = index % Shogi::BOARD_ROWS;

        for (int side = 0; side < 2; ++side) {
            Shogi::Turn turn = static_cast<Shogi::Turn>(side);
            int sign = (turn == Shogi::Turn::SENTE) ? -1 : 1;

            // 歩
            set_if_valid(attacks_pawn_[side][index], {col, row + sign});

            // 桂馬
            set_if_valid(attacks_knight_[side][index], {col - 1, row + sign * 2});
            set_if_valid(attacks_knight_[side][index], {col + 1, row + sign * 2});
            // 銀
            set_if_valid(attacks_silver_[side][index], {col - 1, row + sign});
            set_if_valid(attacks_silver_[side][index], {col, row + sign});
            set_if_valid(attacks_silver_[side][index], {col + 1, row + sign});
            set_if_valid(attacks_silver_[side][index], {col - 1, row - sign});
            set_if_valid(attacks_silver_[side][index], {col + 1, row - sign});
            // 金
            set_if_valid(attacks_gold_[side][index], {col - 1, row + sign});
            set_if_valid(attacks_gold_[side][index], {col, row + sign});
            set_if_valid(attacks_gold_[side][index], {col + 1, row + sign});
            set_if_valid(attacks_gold_[side][index], {col - 1, row});
            set_if_valid(attacks_gold_[side][index], {col + 1, row});
            set_if_valid(attacks_gold_[side][index], {col, row - sign});
        }

        // 玉
        for (int dx = -1; dx <= 1; ++dx) {
            for (int dy = -1; dy <= 1; ++dy) {
                if (dx == 0 && dy == 0) {
                    continue;
                }
                set_if_valid(attacks_king_[index], {col + dx, row + dy});
            }
        }
    }

    // 走り駒
    int dxs[8] = {0, 1, 1, 1, 0, -1, -1, -1};
    int dys[8] = {-1, -1, 0, 1, 1, 1, 0, -1};
    for (int index = 0; index < Shogi::BOARD_SIZE; ++index) {
        int col = index / Shogi::BOARD_ROWS;
        int row = index % Shogi::BOARD_ROWS;

        for (int dir = 0; dir < 8; ++dir) {
            Bitboard ray;
            Coord coord = {col + dxs[dir], row + dys[dir]};
            while (coord.is_valid()) {
                ray.set(coord.col * Shogi::BOARD_ROWS + coord.row);
                coord.col += dxs[dir];
                coord.row += dys[dir];
            }
            rays_[dir][index] = ray;
        }
    }

    attack_tables_initialized_ = true;
}

Bitboard BoardState::get_lance_attacks(int square, Turn turn, const Bitboard &occupancy) const {
    int direction = (turn == Turn::SENTE) ? 0 : 4; // 0: 上，4: 下
    Bitboard ray = rays_[direction][square];
    Bitboard blockers = ray & occupancy;

    if (!blockers.is_empty()) {
        int blocker_index;
        if (turn == Turn::SENTE) {
            blocker_index = blockers.msb();
        } else {
            blocker_index = blockers.lsb();
        }

        Bitboard ray_from_blocker = rays_[direction][blocker_index];
        return ray & ~ray_from_blocker;
    }

    return ray;
}

Bitboard BoardState::get_bishop_attacks(int square, const Bitboard &occupancy) const {
    Bitboard attacks;
    int directions[4] = {1, 3, 5, 7}; // 1: 右上，3: 右下，5: 左下，7: 左上

    for (int dir = 0; dir < 4; ++dir) {
        int direction = directions[dir];
        Bitboard ray = rays_[direction][square];
        Bitboard blockers = ray & occupancy;

        if (!blockers.is_empty()) {
            int blocker_index;

            // 左下と左上はindexが減る方向
            // 右上と右下はindexが増える方向
            if (direction == 5 || direction == 7) {
                blocker_index = blockers.msb();
            } else {
                blocker_index = blockers.lsb();
            }

            Bitboard ray_from_blocker = rays_[direction][blocker_index];
            attacks |= (ray & ~ray_from_blocker);
        } else {
            attacks |= ray;
        }
    }

    return attacks;
}

Bitboard BoardState::get_rook_attacks(int square, const Bitboard &occupancy) const {
    Bitboard attacks;
    int directions[4] = {0, 2, 4, 6}; // 0: 上，2: 右，4: 下，6: 左

    for (int dir = 0; dir < 4; ++dir) {
        int direction = directions[dir];
        Bitboard ray = rays_[direction][square];
        Bitboard blockers = ray & occupancy;

        if (!blockers.is_empty()) {
            int blocker_index;

            // 上と左はindexが減る方向
            // 下と右はindexが増える方向
            if (direction == 0 || direction == 6) {
                blocker_index = blockers.msb();
            } else {
                blocker_index = blockers.lsb();
            }

            Bitboard ray_from_blocker = rays_[direction][blocker_index];
            attacks |= (ray & ~ray_from_blocker);
        } else {
            attacks |= ray;
        }
    }

    return attacks;
}

void BoardState::build_bitboard() {
    bitboard_all_ = Bitboard();
    bitboard_side_[0] = Bitboard();
    bitboard_side_[1] = Bitboard();
    bitboard_promoted_[0] = Bitboard();
    bitboard_promoted_[1] = Bitboard();

    for (int i = 0; i < 2; i++) {
        for (int j = 0; j < Shogi::PIECE_TYPE_COUNT; j++) {
            bitboard_piece_[i][j] = Bitboard();
        }
    }

    for (int col = 0; col < Shogi::BOARD_COLS; ++col) {
        for (int row = 0; row < Shogi::BOARD_ROWS; ++row) {
            Coord coord{col, row};
            const Cell &cell = get_cell(coord);
            if (cell.is_empty()) {
                continue;
            }

            int index = col * Shogi::BOARD_ROWS + row;
            add_piece_to_bitboard(index, cell.turn, cell.type, cell.is_promoted);
        }
    }
}

void BoardState::add_piece_to_bitboard(int index, Turn turn, PieceType type, bool is_promoted) {
    int turn_indx = static_cast<int>(turn);
    int piece_type_index = static_cast<int>(type);

    bitboard_piece_[turn_indx][piece_type_index].set(index);
    bitboard_side_[turn_indx].set(index);
    bitboard_all_.set(index);
    if (is_promoted) {
        bitboard_promoted_[turn_indx].set(index);
    }
}

void BoardState::remove_piece_from_bitboard(int index, Turn turn, PieceType type, bool is_promoted) {
    int turn_indx = static_cast<int>(turn);
    int piece_type_index = static_cast<int>(type);

    bitboard_piece_[turn_indx][piece_type_index].clear(index);
    bitboard_side_[turn_indx].clear(index);
    bitboard_all_.clear(index);
    if (is_promoted) {
        bitboard_promoted_[turn_indx].clear(index);
    }
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

int BoardState::calculate_score() const {
    int score = 0;

    // 盤上の駒
    for (int col = 0; col < Shogi::BOARD_COLS; ++col) {
        for (int row = 0; row < Shogi::BOARD_ROWS; ++row) {
            const Cell &cell = get_cell({col, row});
            if (cell.is_empty()) {
                continue;
            }

            int piece_score = Shogi::get_piece_score(cell.type, cell.is_promoted, cell.turn, col, row);
            piece_score = Shogi::apply_board_discount(piece_score);

            if (cell.turn == Turn::SENTE) {
                score += piece_score;
            } else {
                score -= piece_score;
            }
        }
    }

    // 持ち駒
    for (Turn turn : {Turn::SENTE, Turn::GOTE}) {
        int sign = (turn == Turn::SENTE) ? 1 : -1;
        for (int piece_type = 0; piece_type < Shogi::PIECE_TYPE_COUNT; ++piece_type) {
            int count = hand_[static_cast<int>(turn)][piece_type];
            if (count > 0) {
                int value = Shogi::PIECE_VALUES[piece_type][0];
                score += count * value * sign;
            }
        }
    }

    return score;
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

bool BoardState::is_legal_move(Coord from, Coord to) {
    if (!is_valid_move(from, to)) {
        return false;
    }

    const Cell from_cell = get_cell(from);
    const Cell to_cell = get_cell(to);
    const int from_idx = from.col * Shogi::BOARD_ROWS + from.row;
    const int to_idx = to.col * Shogi::BOARD_ROWS + to.row;
    const auto old_king_pos = king_pos_[static_cast<int>(from_cell.turn)];

    board_[to_idx] = from_cell;
    board_[from_idx] = Cell();

    if (from_cell.type == PieceType::KING) {
        king_pos_[static_cast<int>(from_cell.turn)] = to;
    }

    // 王手放置になる手を除外
    const bool in_check = is_king_in_check(from_cell.turn);

    board_[from_idx] = from_cell;
    board_[to_idx] = to_cell;
    king_pos_[static_cast<int>(from_cell.turn)] = old_king_pos;

    return !in_check;
}

bool BoardState::is_legal_drop(PieceType piece_type, bool is_enemy, Coord to) {
    if (!is_valid_drop(piece_type, is_enemy, to)) {
        return false;
    }

    const Turn turn = is_enemy ? Turn::GOTE : Turn::SENTE;
    const int to_idx = to.col * Shogi::BOARD_ROWS + to.row;

    board_[to_idx] = Cell(piece_type, turn, false);

    // 王手放置になる手を除外
    const bool in_check = is_king_in_check(turn);

    board_[to_idx] = Cell();

    return !in_check;
}

bool BoardState::can_move_geometry(PieceType piece_type, bool is_enemy, bool is_promoted, Coord from, Coord to) const {
    int dx = to.col - from.col;
    int dy = to.row - from.row;
    int sign = is_enemy ? -1 : 1;

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

    // 走り駒のチェック
    switch (effective_type) {
    case PieceType::LANCE:
        return (dx == 0 && dy * sign < 0);

    case PieceType::BISHOP:
        if (abs_dx == abs_dy && abs_dx > 0) {
            return true;
        }
        if (is_promoted && abs_dx + abs_dy == 1) {
            return true;
        }
        return false;

    case PieceType::ROOK:
        if ((dx == 0 || dy == 0) && (abs_dx + abs_dy > 0)) {
            return true;
        }
        if (is_promoted && abs_dx == 1 && abs_dy == 1) {
            return true;
        }
        return false;

    default:
        break;
    }

    // 近接駒のチェック
    const StepMoves *moves = nullptr;
    switch (effective_type) {
    case PieceType::PAWN:
        moves = &STEP_PAWN;
        break;
    case PieceType::KNIGHT:
        moves = &STEP_KNIGHT;
        break;
    case PieceType::SILVER:
        moves = &STEP_SILVER;
        break;
    case PieceType::GOLD:
        moves = &STEP_GOLD;
        break;
    case PieceType::KING:
        moves = &STEP_KING;
        break;
    default:
        return false;
    }

    for (int i = 0; i < moves->count; ++i) {
        int expected_dx = moves->dirs[i].dx * sign;
        int expected_dy = moves->dirs[i].dy * sign;
        if (dx == expected_dx && dy == expected_dy) {
            return true;
        }
    }

    return false;
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

    return (pawn_columns_[static_cast<int>(turn)] & (1 << col)) != 0;
}

bool BoardState::is_king_in_check(Turn turn) const {
    auto king_pos = get_king_position(turn);
    if (!king_pos) {
        return false;
    }

    const int king_col = king_pos->col;
    const int king_row = king_pos->row;
    const Turn enemy_side = (turn == Turn::SENTE) ? Turn::GOTE : Turn::SENTE;
    const int enemy_forward = (enemy_side == Turn::SENTE) ? -1 : 1;

    // 桂馬の利きをチェック
    const int knight_positions[2][2] = {{-1, -enemy_forward * 2}, {1, -enemy_forward * 2}};
    for (const auto &pos : knight_positions) {
        int col = king_col + pos[0];
        int row = king_row + pos[1];
        if (col >= 0 && col < Shogi::BOARD_COLS && row >= 0 && row < Shogi::BOARD_ROWS) {
            const Cell &cell = board_[col * Shogi::BOARD_ROWS + row];
            if (!cell.is_empty() && cell.turn == enemy_side && cell.type == PieceType::KNIGHT && !cell.is_promoted) {
                return true;
            }
        }
    }

    // 歩の利きをチェック
    {
        int pawn_row = king_row - enemy_forward;
        if (pawn_row >= 0 && pawn_row < Shogi::BOARD_ROWS) {
            const Cell &cell = board_[king_col * Shogi::BOARD_ROWS + pawn_row];
            if (!cell.is_empty() && cell.turn == enemy_side && cell.type == PieceType::PAWN && !cell.is_promoted) {
                return true;
            }
        }
    }

    constexpr int directions[8][2] = {{0, -1}, {0, 1}, {-1, 0}, {1, 0}, {-1, -1}, {1, -1}, {-1, 1}, {1, 1}};
    for (int dir_idx = 0; dir_idx < 8; ++dir_idx) {
        int dx = directions[dir_idx][0];
        int dy = directions[dir_idx][1];
        bool is_diagonal = (dir_idx >= 4);

        for (int dist = 1; dist < 9; ++dist) {
            int col = king_col + dx * dist;
            int row = king_row + dy * dist;

            if (col < 0 || col >= Shogi::BOARD_COLS || row < 0 || row >= Shogi::BOARD_ROWS) {
                break;
            }

            const Cell &cell = board_[col * Shogi::BOARD_ROWS + row];
            if (cell.is_empty()) {
                continue;
            }

            if (cell.turn == turn) {
                break;
            }

            PieceType type = cell.type;
            bool is_promoted = cell.is_promoted;

            if (dist == 1) {
                // 玉の利きをチェック
                if (type == PieceType::KING) {
                    return true;
                }

                // 金の利きをチェック
                bool is_gold_move = (type == PieceType::GOLD) ||
                                    (is_promoted && (type == PieceType::SILVER || type == PieceType::KNIGHT ||
                                                     type == PieceType::LANCE || type == PieceType::PAWN));
                if (is_gold_move) {
                    int move_dx = -dx;
                    int move_dy = -dy;
                    if (enemy_side == Turn::GOTE) {
                        move_dx = -move_dx;
                        move_dy = -move_dy;
                    }

                    bool is_backward_diagonal = (std::abs(move_dx) == 1 && move_dy == 1);
                    if (!is_backward_diagonal) {
                        return true;
                    }
                }

                // 銀の利きをチェック
                if (type == PieceType::SILVER && !is_promoted) {
                    int move_dx = -dx;
                    int move_dy = -dy;
                    if (enemy_side == Turn::GOTE) {
                        move_dx = -move_dx;
                        move_dy = -move_dy;
                    }

                    bool is_silver_move = (move_dy == -1) || (std::abs(move_dx) == 1 && move_dy == 1);
                    if (is_silver_move) {
                        return true;
                    }
                }

                // 竜（斜め1マス）の利きをチェック
                if (type == PieceType::ROOK && is_promoted && is_diagonal) {
                    return true;
                }

                // 馬（縦横1マス）の利きをチェック
                if (type == PieceType::BISHOP && is_promoted && !is_diagonal) {
                    return true;
                }
            }

            if (is_diagonal) {
                // 角の利きをチェック
                if (type == PieceType::BISHOP) {
                    return true;
                }
            } else {
                // 飛車の利きをチェック
                if (type == PieceType::ROOK) {
                    return true;
                }

                // 香車の利きをチェック
                if (type == PieceType::LANCE && !is_promoted && dx == 0) {
                    int lance_forward = (enemy_side == Turn::SENTE) ? -1 : 1;
                    if (dy == -lance_forward) {
                        return true;
                    }
                }
            }

            break;
        }
    }

    return false;
}

std::vector<Move> BoardState::get_legal_moves(bool only_captures) {
    constexpr std::array HAND_PIECE_TYPES = {
        PieceType::PAWN, PieceType::LANCE,  PieceType::KNIGHT, PieceType::SILVER,
        PieceType::GOLD, PieceType::BISHOP, PieceType::ROOK,
    };

    std::vector<Move> moves;
    moves.reserve(128);

    const bool is_enemy_turn = (turn_to_move_ == Turn::GOTE);
    const int sign = is_enemy_turn ? -1 : 1;
    const int zone_min = is_enemy_turn ? 6 : 0;
    const int zone_max = is_enemy_turn ? 8 : 2;

    auto try_add_move = [&](int from_col, int from_row, int to_col, int to_row, const Cell &cell) -> bool {
        Coord from{from_col, from_row};
        Coord to{to_col, to_row};

        if (!to.is_valid()) {
            return false;
        }

        const Cell &target = get_cell(to);

        // 味方の駒がある場所には移動不可
        if (!target.is_empty() && target.turn == turn_to_move_) {
            return false;
        }

        if (!is_legal_move(from, to)) {
            return false;
        }

        bool is_capture = !target.is_empty();
        if (only_captures && !is_capture) {
            return true;
        }

        bool can_promote = false;
        bool must_promote = false;

        if (!cell.is_promoted && cell.type != PieceType::KING && cell.type != PieceType::GOLD) {
            bool from_in_zone = (from_row >= zone_min && from_row <= zone_max);
            bool to_in_zone = (to_row >= zone_min && to_row <= zone_max);
            if (from_in_zone || to_in_zone) {
                can_promote = true;
            }
        }

        if (is_dead_end(cell.type, is_enemy_turn, to_row)) {
            must_promote = true;
        }

        if (!must_promote) {
            moves.emplace_back(from_col, from_row, to_col, to_row, cell.type, false, false, is_capture);
        }

        if (can_promote) {
            moves.emplace_back(from_col, from_row, to_col, to_row, cell.type, true, false, is_capture);
        }

        return true;
    };

    // 近接駒の移動を追加
    auto add_step_moves = [&](int col, int row, const Cell &cell, const StepMoves &pattern) {
        for (int i = 0; i < pattern.count; ++i) {
            int to_col = col + pattern.dirs[i].dx * sign;
            int to_row = row + pattern.dirs[i].dy * sign;
            try_add_move(col, row, to_col, to_row, cell);
        }
    };

    // 近接駒の移動を追加（玉と成り駒の追加移動）
    auto add_step_moves_no_sign = [&](int col, int row, const Cell &cell, const StepMoves &pattern) {
        for (int i = 0; i < pattern.count; ++i) {
            int to_col = col + pattern.dirs[i].dx;
            int to_row = row + pattern.dirs[i].dy;
            try_add_move(col, row, to_col, to_row, cell);
        }
    };

    // 走り駒の移動を追加
    auto add_slide_moves = [&](int col, int row, const Cell &cell, const SlideMoves &pattern, bool apply_sign) {
        for (int i = 0; i < pattern.count; ++i) {
            int dx = pattern.dirs[i].dx * (apply_sign ? sign : 1);
            int dy = pattern.dirs[i].dy * (apply_sign ? sign : 1);

            for (int dist = 1; dist < 9; ++dist) {
                int to_col = col + dx * dist;
                int to_row = row + dy * dist;
                Coord to{to_col, to_row};

                if (!to.is_valid()) {
                    break;
                }

                const Cell &target = get_cell(to);
                bool blocked = !target.is_empty();

                if (blocked && target.turn == turn_to_move_) {
                    break;
                }

                try_add_move(col, row, to_col, to_row, cell);

                if (blocked) {
                    break;
                }
            }
        }
    };

    for (int idx = 0; idx < Shogi::BOARD_SIZE; ++idx) {
        const Cell &cell = board_[idx];

        if (cell.is_empty() || cell.turn != turn_to_move_) {
            continue;
        }

        int col = idx / Shogi::BOARD_ROWS;
        int row = idx % Shogi::BOARD_ROWS;

        PieceType piece_type = cell.type;
        bool is_promoted = cell.is_promoted;

        switch (piece_type) {
        case PieceType::PAWN:
            if (is_promoted) {
                add_step_moves(col, row, cell, STEP_GOLD);
            } else {
                add_step_moves(col, row, cell, STEP_PAWN);
            }
            break;

        case PieceType::LANCE:
            if (is_promoted) {
                add_step_moves(col, row, cell, STEP_GOLD);
            } else {
                add_slide_moves(col, row, cell, SLIDE_LANCE, true);
            }
            break;

        case PieceType::KNIGHT:
            if (is_promoted) {
                add_step_moves(col, row, cell, STEP_GOLD);
            } else {
                add_step_moves(col, row, cell, STEP_KNIGHT);
            }
            break;

        case PieceType::SILVER:
            if (is_promoted) {
                add_step_moves(col, row, cell, STEP_GOLD);
            } else {
                add_step_moves(col, row, cell, STEP_SILVER);
            }
            break;

        case PieceType::GOLD:
            add_step_moves(col, row, cell, STEP_GOLD);
            break;

        case PieceType::BISHOP:
            add_slide_moves(col, row, cell, SLIDE_BISHOP, false);
            if (is_promoted) {
                add_step_moves_no_sign(col, row, cell, STEP_PROMOTED_BISHOP);
            }
            break;

        case PieceType::ROOK:
            add_slide_moves(col, row, cell, SLIDE_ROOK, false);
            if (is_promoted) {
                add_step_moves_no_sign(col, row, cell, STEP_PROMOTED_ROOK);
            }
            break;

        case PieceType::KING:
            add_step_moves_no_sign(col, row, cell, STEP_KING);
            break;

        default:
            break;
        }
    }

    if (!only_captures) {
        for (PieceType piece_type : HAND_PIECE_TYPES) {
            if (get_hand_count(turn_to_move_, piece_type) > 0) {
                for (int t_col = 0; t_col < Shogi::BOARD_COLS; ++t_col) {
                    for (int t_row = 0; t_row < Shogi::BOARD_ROWS; ++t_row) {
                        Coord to{t_col, t_row};
                        if (is_legal_drop(piece_type, is_enemy_turn, to)) {
                            moves.emplace_back(0, 0, t_col, t_row, piece_type, false, true, false);
                        }
                    }
                }
            }
        }
    }

    return moves;
}

uint64_t BoardState::get_zobrist_hash() const { return zobrist_hash_; }

std::optional<Coord> BoardState::get_king_position(Turn turn) const { return king_pos_[static_cast<int>(turn)]; }

void BoardState::update_king_position_cache() {
    king_pos_[static_cast<int>(Turn::SENTE)] = std::nullopt;
    king_pos_[static_cast<int>(Turn::GOTE)] = std::nullopt;

    for (int col = 0; col < Shogi::BOARD_COLS; ++col) {
        for (int row = 0; row < Shogi::BOARD_ROWS; ++row) {
            const Cell &cell = board_[col * Shogi::BOARD_ROWS + row];
            if (cell.type == PieceType::KING) {
                king_pos_[static_cast<int>(cell.turn)] = Coord{col, row};
            }
        }
    }
}

void BoardState::update_pawn_columns_cache() {
    pawn_columns_[static_cast<int>(Turn::SENTE)] = 0;
    pawn_columns_[static_cast<int>(Turn::GOTE)] = 0;

    for (int col = 0; col < Shogi::BOARD_COLS; ++col) {
        for (int row = 0; row < Shogi::BOARD_ROWS; ++row) {
            const Cell &cell = board_[col * Shogi::BOARD_ROWS + row];
            if (cell.type == PieceType::PAWN && !cell.is_promoted) {
                pawn_columns_[static_cast<int>(cell.turn)] |= (1 << col);
            }
        }
    }
}

const Cell &BoardState::get_cell(Coord coord) const { return board_[coord.col * Shogi::BOARD_ROWS + coord.row]; }

void BoardState::set_cell(Coord coord, PieceType type, Turn turn, bool is_promoted) {
    if (!coord.is_valid()) {
        return;
    }

    int index = coord.col * Shogi::BOARD_ROWS + coord.row;
    Cell old_cell = get_cell(coord);
    if (!old_cell.is_empty()) {
        int old_is_promoted = old_cell.is_promoted ? 1 : 0;
        zobrist_hash_ ^= g_zobrist_board[static_cast<int>(old_cell.turn)][static_cast<int>(old_cell.type)]
                                        [old_is_promoted][coord.col][coord.row];
        remove_piece_from_bitboard(index, old_cell.turn, old_cell.type, old_cell.is_promoted);
        if (old_cell.type == PieceType::KING) {
            auto &cached_pos = king_pos_[static_cast<int>(old_cell.turn)];
            if (cached_pos && cached_pos->col == coord.col && cached_pos->row == coord.row) {
                cached_pos = std::nullopt;
            }
        }
    }

    int new_is_promoted = is_promoted ? 1 : 0;
    zobrist_hash_ ^=
        g_zobrist_board[static_cast<int>(turn)][static_cast<int>(type)][new_is_promoted][coord.col][coord.row];

    board_[index] = Cell(type, turn, is_promoted);
    add_piece_to_bitboard(index, turn, type, is_promoted);

    if (type == PieceType::KING) {
        king_pos_[static_cast<int>(turn)] = coord;
    }

    if (type == PieceType::PAWN && !is_promoted) {
        pawn_columns_[static_cast<int>(turn)] |= (1 << coord.col);
    }
}

void BoardState::clear_cell(Coord coord) {
    if (!coord.is_valid()) {
        return;
    }

    int index = coord.col * Shogi::BOARD_ROWS + coord.row;
    Cell old_cell = get_cell(coord);
    if (!old_cell.is_empty()) {
        int old_is_promoted = old_cell.is_promoted ? 1 : 0;
        zobrist_hash_ ^= g_zobrist_board[static_cast<int>(old_cell.turn)][static_cast<int>(old_cell.type)]
                                        [old_is_promoted][coord.col][coord.row];
        remove_piece_from_bitboard(index, old_cell.turn, old_cell.type, old_cell.is_promoted);
        if (old_cell.type == PieceType::KING) {
            auto &cached_pos = king_pos_[static_cast<int>(old_cell.turn)];
            if (cached_pos && cached_pos->col == coord.col && cached_pos->row == coord.row) {
                cached_pos = std::nullopt;
            }
        }

        if (old_cell.type == PieceType::PAWN && !old_cell.is_promoted) {
            bool has_other_pawn = false;
            for (int row = 0; row < Shogi::BOARD_ROWS; ++row) {
                if (row == coord.row)
                    continue;
                const Cell &cell = board_[coord.col * Shogi::BOARD_ROWS + row];
                if (cell.type == PieceType::PAWN && !cell.is_promoted && cell.turn == old_cell.turn) {
                    has_other_pawn = true;
                    break;
                }
            }
            if (!has_other_pawn) {
                pawn_columns_[static_cast<int>(old_cell.turn)] &= ~(1 << coord.col);
            }
        }
    }

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

Shogi::UndoInfo BoardState::apply_move(const Move &move) {
    Shogi::UndoInfo undo;
    undo.move = move;
    undo.prev_hash = zobrist_hash_;
    undo.prev_pawn_cols[0] = pawn_columns_[0];
    undo.prev_pawn_cols[1] = pawn_columns_[1];
    undo.prev_score = score_;

    Turn current_side = turn_to_move_;
    Turn opponent_side = (turn_to_move_ == Turn::SENTE) ? Turn::GOTE : Turn::SENTE;
    int sign = (current_side == Turn::SENTE) ? 1 : -1;

    int from_idx = move.from_col * Shogi::BOARD_ROWS + move.from_row;
    int to_idx = move.to_col * Shogi::BOARD_ROWS + move.to_row;

    Cell target = get_cell({move.to_col, move.to_row});
    if (!target.is_empty()) {
        undo.captured_type = static_cast<uint8_t>(target.type);
        undo.captured_promoted = target.is_promoted;
    } else {
        undo.captured_type = static_cast<uint8_t>(PieceType::EMPTY);
        undo.captured_promoted = false;
    }

    if (move.is_drop) {
        PieceType piece_type = move.piece_type;
        int count = hand_[static_cast<int>(current_side)][static_cast<int>(piece_type)];

        int hand_value = Shogi::PIECE_VALUES[static_cast<int>(piece_type)][0];
        int board_score = Shogi::get_piece_score(piece_type, false, current_side, move.to_col, move.to_row);
        board_score = Shogi::apply_board_discount(board_score);
        score_ += sign * (board_score - hand_value);

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
        add_piece_to_bitboard(to_idx, current_side, piece_type, false);

        if (piece_type == PieceType::PAWN) {
            pawn_columns_[static_cast<int>(current_side)] |= (1 << move.to_col);
        }
    } else {
        Cell source = get_cell({move.from_col, move.from_row});

        int from_score =
            Shogi::get_piece_score(source.type, source.is_promoted, current_side, move.from_col, move.from_row);
        from_score = Shogi::apply_board_discount(from_score);
        score_ -= sign * from_score;

        int src_is_promoted = source.is_promoted ? 1 : 0;
        zobrist_hash_ ^= g_zobrist_board[static_cast<int>(current_side)][static_cast<int>(source.type)][src_is_promoted]
                                        [move.from_col][move.from_row];
        remove_piece_from_bitboard(from_idx, current_side, source.type, source.is_promoted);

        if (!target.is_empty()) {
            int captured_score =
                Shogi::get_piece_score(target.type, target.is_promoted, opponent_side, move.to_col, move.to_row);
            captured_score = Shogi::apply_board_discount(captured_score);
            score_ += sign * captured_score;
            int hand_value = Shogi::PIECE_VALUES[static_cast<int>(target.type)][0];
            score_ += sign * hand_value;

            int tgt_is_promoted = target.is_promoted ? 1 : 0;
            zobrist_hash_ ^= g_zobrist_board[static_cast<int>(opponent_side)][static_cast<int>(target.type)]
                                            [tgt_is_promoted][move.to_col][move.to_row];
            remove_piece_from_bitboard(to_idx, opponent_side, target.type, target.is_promoted);

            PieceType captured_type = target.type;
            int count = hand_[static_cast<int>(current_side)][static_cast<int>(captured_type)];
            int idx_old = std::clamp(count, 0, 19);
            int idx_new = std::clamp(count + 1, 0, 19);
            zobrist_hash_ ^= g_zobrist_hand[static_cast<int>(current_side)][static_cast<int>(captured_type)][idx_old];
            zobrist_hash_ ^= g_zobrist_hand[static_cast<int>(current_side)][static_cast<int>(captured_type)][idx_new];
            hand_[static_cast<int>(current_side)][static_cast<int>(captured_type)]++;
        }

        bool is_promoted = move.is_promotion || source.is_promoted;

        int to_score = Shogi::get_piece_score(source.type, is_promoted, current_side, move.to_col, move.to_row);
        to_score = Shogi::apply_board_discount(to_score);
        score_ += sign * to_score;

        int new_is_promoted = is_promoted ? 1 : 0;
        zobrist_hash_ ^= g_zobrist_board[static_cast<int>(current_side)][static_cast<int>(source.type)][new_is_promoted]
                                        [move.to_col][move.to_row];

        board_[to_idx] = Cell(source.type, current_side, is_promoted);
        board_[from_idx] = Cell();
        add_piece_to_bitboard(to_idx, current_side, source.type, is_promoted);

        if (source.type == PieceType::KING) {
            king_pos_[static_cast<int>(current_side)] = Coord{move.to_col, move.to_row};
        }

        if (source.type == PieceType::PAWN && !source.is_promoted) {
            pawn_columns_[static_cast<int>(current_side)] &= ~(1 << move.from_col);
            if (!is_promoted) {
                pawn_columns_[static_cast<int>(current_side)] |= (1 << move.to_col);
            }
        }
        if (!target.is_empty() && target.type == PieceType::PAWN && !target.is_promoted) {
            pawn_columns_[static_cast<int>(opponent_side)] &= ~(1 << move.to_col);
        }
    }

    zobrist_hash_ ^= g_zobrist_turn_enemy;
    turn_to_move_ = opponent_side;

    return undo;
}

void BoardState::undo_move(const Shogi::UndoInfo &undo) {
    const Move &move = undo.move;

    Turn original_side = (turn_to_move_ == Turn::SENTE) ? Turn::GOTE : Turn::SENTE;
    Turn opponent_side = turn_to_move_;

    int from_idx = move.from_col * Shogi::BOARD_ROWS + move.from_row;
    int to_idx = move.to_col * Shogi::BOARD_ROWS + move.to_row;

    if (move.is_drop) {
        remove_piece_from_bitboard(to_idx, original_side, move.piece_type, false);
        board_[to_idx] = Cell();
        hand_[static_cast<int>(original_side)][static_cast<int>(move.piece_type)]++;
    } else {
        Cell moved_piece = get_cell({move.to_col, move.to_row});
        remove_piece_from_bitboard(to_idx, original_side, moved_piece.type, moved_piece.is_promoted);

        bool was_promoted_before = moved_piece.is_promoted && !move.is_promotion;
        if (move.is_promotion) {
            was_promoted_before = false;
        } else {
            was_promoted_before = moved_piece.is_promoted;
        }

        board_[from_idx] = Cell(moved_piece.type, original_side, was_promoted_before);
        add_piece_to_bitboard(from_idx, original_side, moved_piece.type, was_promoted_before);

        PieceType captured_type = static_cast<PieceType>(undo.captured_type);
        if (captured_type != PieceType::EMPTY) {
            board_[to_idx] = Cell(captured_type, opponent_side, undo.captured_promoted);
            add_piece_to_bitboard(to_idx, opponent_side, captured_type, undo.captured_promoted);
            if (hand_[static_cast<int>(original_side)][static_cast<int>(captured_type)] > 0) {
                hand_[static_cast<int>(original_side)][static_cast<int>(captured_type)]--;
            }
        } else {
            board_[to_idx] = Cell();
        }

        if (moved_piece.type == PieceType::KING) {
            king_pos_[static_cast<int>(original_side)] = Coord{move.from_col, move.from_row};
        }
    }

    pawn_columns_[0] = undo.prev_pawn_cols[0];
    pawn_columns_[1] = undo.prev_pawn_cols[1];

    zobrist_hash_ = undo.prev_hash;
    score_ = undo.prev_score;

    turn_to_move_ = original_side;
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
