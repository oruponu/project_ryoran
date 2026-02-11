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

uint64_t g_zobrist_board[2][Shogi::PIECE_TYPE_COUNT][2][Shogi::BOARD_COLS][Shogi::BOARD_ROWS];
uint64_t g_zobrist_hand[2][Shogi::PIECE_TYPE_COUNT][20];
uint64_t g_zobrist_turn_enemy;
bool g_zobrist_initialized = false;

} // namespace

BoardState::BoardState(Turn turn_to_move) : turn_to_move_(turn_to_move), score_(0) {
    AttackTable::initialize();
    Evaluator::initialize();

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
    score_ = Evaluator::calculate_score(*this);

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

Bitboard BoardState::get_checkers(Turn turn) const {
    Bitboard checkers;
    auto king_position = get_king_position(turn);
    if (!king_position.has_value()) {
        return checkers;
    }

    int king_square = king_position->col * Shogi::BOARD_ROWS + king_position->row;

    const Turn enemy_turn = (turn == Turn::SENTE) ? Turn::GOTE : Turn::SENTE;
    int enemy_index = static_cast<int>(enemy_turn);

    const Bitboard &enemy_pawns = bitboard_piece_[enemy_index][static_cast<int>(PieceType::PAWN)];
    const Bitboard &enemy_lances = bitboard_piece_[enemy_index][static_cast<int>(PieceType::LANCE)];
    const Bitboard &enemy_knights = bitboard_piece_[enemy_index][static_cast<int>(PieceType::KNIGHT)];
    const Bitboard &enemy_silvers = bitboard_piece_[enemy_index][static_cast<int>(PieceType::SILVER)];
    const Bitboard &enemy_golds = bitboard_piece_[enemy_index][static_cast<int>(PieceType::GOLD)];
    const Bitboard &enemy_bishops = bitboard_piece_[enemy_index][static_cast<int>(PieceType::BISHOP)];
    const Bitboard &enemy_rooks = bitboard_piece_[enemy_index][static_cast<int>(PieceType::ROOK)];
    const Bitboard &enemy_kings = bitboard_piece_[enemy_index][static_cast<int>(PieceType::KING)];
    const Bitboard &enemy_promoted = bitboard_promoted_[enemy_index];
    const Bitboard occupancy = bitboard_all_;

    // 香車の利き
    Bitboard lance_attacks = AttackTable::get_lance_attacks(king_square, turn, occupancy);
    checkers |= (lance_attacks & (enemy_lances & ~enemy_promoted));

    // 角の利き
    Bitboard bishop_attacks = AttackTable::get_bishop_attacks(king_square, occupancy);
    checkers |= (bishop_attacks & enemy_bishops);

    // 飛車の利き
    Bitboard rook_attacks = AttackTable::get_rook_attacks(king_square, occupancy);
    checkers |= (rook_attacks & enemy_rooks);

    // 歩の利き
    checkers |= (AttackTable::get_pawn_attacks(king_square, turn) & (enemy_pawns & ~enemy_promoted));

    // 桂馬の利き
    checkers |= (AttackTable::get_knight_attacks(king_square, turn) & (enemy_knights & ~enemy_promoted));

    // 銀の利き
    checkers |= (AttackTable::get_silver_attacks(king_square, turn) & (enemy_silvers & ~enemy_promoted));

    // 金および金と同じ動きをする成り駒の利きをチェック
    Bitboard gold_likes = enemy_golds | (enemy_promoted & (enemy_pawns | enemy_lances | enemy_knights | enemy_silvers));
    checkers |= (AttackTable::get_gold_attacks(king_square, turn) & gold_likes);

    // 玉および竜（斜め1マス）と馬（縦横1マス）の利きをチェック
    Bitboard king_likes = enemy_kings | (enemy_promoted & (enemy_bishops | enemy_rooks));
    checkers |= (AttackTable::get_king_attacks(king_square) & king_likes);

    return checkers;
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

PinMasks BoardState::calculate_pin_masks(Turn turn) const {
    PinMasks pin_masks;
    pin_masks.pinned = Bitboard();

    auto king_position = get_king_position(turn);
    if (!king_position.has_value()) {
        return pin_masks;
    }

    int king_square = king_position->col * Shogi::BOARD_ROWS + king_position->row;

    Turn enemy_turn = (turn == Turn::SENTE) ? Turn::GOTE : Turn::SENTE;
    int my_index = static_cast<int>(turn);
    int enemy_index = static_cast<int>(enemy_turn);

    const Bitboard &enemy_lances = bitboard_piece_[enemy_index][static_cast<int>(PieceType::LANCE)];
    const Bitboard &enemy_bishops = bitboard_piece_[enemy_index][static_cast<int>(PieceType::BISHOP)];
    const Bitboard &enemy_rooks = bitboard_piece_[enemy_index][static_cast<int>(PieceType::ROOK)];
    const Bitboard &enemy_promoted = bitboard_promoted_[enemy_index];

    Bitboard enemy_line_sliders = enemy_rooks | (enemy_promoted & enemy_bishops);
    Bitboard enemy_diagonal_sliders = enemy_bishops | (enemy_promoted & enemy_rooks);

    for (int dir = 0; dir < 8; ++dir) {
        Bitboard sliders;
        if (dir % 2 != 0) {
            sliders = enemy_diagonal_sliders;
        } else {
            sliders = enemy_line_sliders;
            if (dir == 0 && enemy_turn == Turn::GOTE) {
                sliders |= enemy_lances;
            } else if (dir == 4 && enemy_turn == Turn::SENTE) {
                sliders |= enemy_lances;
            }
        }

        Bitboard ray = AttackTable::get_ray(king_square, dir);
        Bitboard attackers = ray & sliders;
        if (attackers.is_empty()) {
            continue;
        }

        int attacker_square;

        // 最も玉に近い攻撃駒を特定
        if (dir >= 1 && dir <= 4) {
            attacker_square = attackers.lsb();
        } else {
            attacker_square = attackers.msb();
        }

        Bitboard path = ray ^ AttackTable::get_ray(attacker_square, dir);
        path.set(attacker_square);

        Bitboard between = path & bitboard_all_;
        between.clear(attacker_square);

        if (between.count() == 1) {
            int pinned_square = between.lsb();
            Bitboard pinned;
            pinned.set(pinned_square);
            if (!(pinned & bitboard_side_[my_index]).is_empty()) {
                pin_masks.pinned.set(pinned_square);
                pin_masks.valid_ray_masks[pinned_square] = path;
            }
        }
    }

    return pin_masks;
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
    const Cell &piece = get_cell(from);
    if (piece.is_empty()) {
        return false;
    }

    // 盤面の範囲外には移動不可
    if (!to.is_valid()) {
        return false;
    }

    // 現在地と同じ場所には移動不可
    if (from == to) {
        return false;
    }

    // 味方の駒がある場所には移動不可
    const Cell &target = get_cell(to);
    if (!target.is_empty() && target.turn == piece.turn) {
        return false;
    }

    int from_index = from.col * Shogi::BOARD_ROWS + from.row;
    int to_index = to.col * Shogi::BOARD_ROWS + to.row;
    int side = static_cast<int>(piece.turn);

    Bitboard attacks;
    if (piece.is_promoted) {
        switch (piece.type) {
        case PieceType::BISHOP:
            attacks = AttackTable::get_promoted_bishop_attacks(from_index, bitboard_all_);
            break;
        case PieceType::ROOK:
            attacks = AttackTable::get_promoted_rook_attacks(from_index, bitboard_all_);
            break;
        default:
            attacks = AttackTable::get_gold_attacks(from_index, piece.turn);
            break;
        }
    } else {
        switch (piece.type) {
        case PieceType::PAWN:
            attacks = AttackTable::get_pawn_attacks(from_index, piece.turn);
            break;
        case PieceType::LANCE:
            attacks = AttackTable::get_lance_attacks(from_index, piece.turn, bitboard_all_);
            break;
        case PieceType::KNIGHT:
            attacks = AttackTable::get_knight_attacks(from_index, piece.turn);
            break;
        case PieceType::SILVER:
            attacks = AttackTable::get_silver_attacks(from_index, piece.turn);
            break;
        case PieceType::GOLD:
            attacks = AttackTable::get_gold_attacks(from_index, piece.turn);
            break;
        case PieceType::BISHOP:
            attacks = AttackTable::get_bishop_attacks(from_index, bitboard_all_);
            break;
        case PieceType::ROOK:
            attacks = AttackTable::get_rook_attacks(from_index, bitboard_all_);
            break;
        case PieceType::KING:
            attacks = AttackTable::get_king_attacks(from_index);
            break;
        default:
            return false;
        }
    }

    return attacks.is_set(to_index);
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

    remove_piece_from_bitboard(from_idx, from_cell.turn, from_cell.type, from_cell.is_promoted);
    if (!to_cell.is_empty()) {
        remove_piece_from_bitboard(to_idx, to_cell.turn, to_cell.type, to_cell.is_promoted);
    }
    add_piece_to_bitboard(to_idx, from_cell.turn, from_cell.type, from_cell.is_promoted);

    if (from_cell.type == PieceType::KING) {
        king_pos_[static_cast<int>(from_cell.turn)] = to;
    }

    // 王手放置になる手を除外
    const bool in_check = is_king_in_check(from_cell.turn);

    board_[from_idx] = from_cell;
    board_[to_idx] = to_cell;
    king_pos_[static_cast<int>(from_cell.turn)] = old_king_pos;

    remove_piece_from_bitboard(to_idx, from_cell.turn, from_cell.type, from_cell.is_promoted);
    if (!to_cell.is_empty()) {
        add_piece_to_bitboard(to_idx, to_cell.turn, to_cell.type, to_cell.is_promoted);
    }
    add_piece_to_bitboard(from_idx, from_cell.turn, from_cell.type, from_cell.is_promoted);

    return !in_check;
}

bool BoardState::is_legal_drop(PieceType piece_type, bool is_enemy, Coord to) {
    if (!is_valid_drop(piece_type, is_enemy, to)) {
        return false;
    }

    const Turn turn = is_enemy ? Turn::GOTE : Turn::SENTE;
    const int to_idx = to.col * Shogi::BOARD_ROWS + to.row;

    board_[to_idx] = Cell(piece_type, turn, false);

    add_piece_to_bitboard(to_idx, turn, piece_type, false);

    // 王手放置になる手を除外
    const bool in_check = is_king_in_check(turn);

    board_[to_idx] = Cell();

    remove_piece_from_bitboard(to_idx, turn, piece_type, false);

    return !in_check;
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
    auto king_position = get_king_position(turn);
    if (!king_position) {
        return false;
    }

    int king_square = king_position->col * Shogi::BOARD_ROWS + king_position->row;

    const Turn enemy_turn = (turn == Turn::SENTE) ? Turn::GOTE : Turn::SENTE;
    int enemy_index = static_cast<int>(enemy_turn);

    const Bitboard &enemy_pawns = bitboard_piece_[enemy_index][static_cast<int>(PieceType::PAWN)];
    const Bitboard &enemy_lances = bitboard_piece_[enemy_index][static_cast<int>(PieceType::LANCE)];
    const Bitboard &enemy_knights = bitboard_piece_[enemy_index][static_cast<int>(PieceType::KNIGHT)];
    const Bitboard &enemy_silvers = bitboard_piece_[enemy_index][static_cast<int>(PieceType::SILVER)];
    const Bitboard &enemy_golds = bitboard_piece_[enemy_index][static_cast<int>(PieceType::GOLD)];
    const Bitboard &enemy_bishops = bitboard_piece_[enemy_index][static_cast<int>(PieceType::BISHOP)];
    const Bitboard &enemy_rooks = bitboard_piece_[enemy_index][static_cast<int>(PieceType::ROOK)];
    const Bitboard &enemy_kings = bitboard_piece_[enemy_index][static_cast<int>(PieceType::KING)];
    const Bitboard &enemy_promoted = bitboard_promoted_[enemy_index];
    const Bitboard &occupancy = bitboard_all_;

    // 香車の利きをチェック
    Bitboard lance_attacks = AttackTable::get_lance_attacks(king_square, turn, occupancy);
    if (!(lance_attacks & (enemy_lances & ~enemy_promoted)).is_empty()) {
        return true;
    }

    // 角の利きをチェック
    Bitboard bishop_attacks = AttackTable::get_bishop_attacks(king_square, occupancy);
    if (!(bishop_attacks & enemy_bishops).is_empty()) {
        return true;
    }

    // 飛車の利きをチェック
    Bitboard rook_attacks = AttackTable::get_rook_attacks(king_square, occupancy);
    if (!(rook_attacks & enemy_rooks).is_empty()) {
        return true;
    }

    // 歩の利きをチェック
    if (!(AttackTable::get_pawn_attacks(king_square, turn) & (enemy_pawns & ~enemy_promoted)).is_empty()) {
        return true;
    }

    // 桂馬の利きをチェック
    if (!(AttackTable::get_knight_attacks(king_square, turn) & (enemy_knights & ~enemy_promoted)).is_empty()) {
        return true;
    }

    // 銀の利きをチェック
    if (!(AttackTable::get_silver_attacks(king_square, turn) & (enemy_silvers & ~enemy_promoted)).is_empty()) {
        return true;
    }

    // 金および金と同じ動きをする成り駒の利きをチェック
    Bitboard gold_likes = enemy_golds | (enemy_promoted & (enemy_pawns | enemy_lances | enemy_knights | enemy_silvers));
    if (!(AttackTable::get_gold_attacks(king_square, turn) & gold_likes).is_empty()) {
        return true;
    }

    // 玉および竜（斜め1マス）と馬（縦横1マス）の利きをチェック
    Bitboard king_likes = enemy_kings | (enemy_promoted & (enemy_bishops | enemy_rooks));
    if (!(AttackTable::get_king_attacks(king_square) & king_likes).is_empty()) {
        return true;
    }

    return false;
}

void BoardState::get_legal_moves(Shogi::MoveList &move_list, bool only_captures) {
    move_list.clear();

    const Turn current_turn = turn_to_move_;
    const Turn opponent_turn = (current_turn == Turn::SENTE) ? Turn::GOTE : Turn::SENTE;
    const Bitboard my_pieces_bitboard = bitboard_side_[static_cast<int>(current_turn)];
    const Bitboard opponent_pieces_bitboard = bitboard_side_[static_cast<int>(opponent_turn)];
    const Bitboard occupancy = bitboard_all_;
    const int promotion_row_min = (current_turn == Turn::SENTE) ? 0 : 6;
    const int promotion_row_max = (current_turn == Turn::SENTE) ? 2 : 8;
    auto is_promotion_rank = [&](int row) { return (row >= promotion_row_min && row <= promotion_row_max); };
    PinMasks pin_masks = calculate_pin_masks(turn_to_move_);

    Bitboard checkers = get_checkers(current_turn);
    Bitboard evasion_mask;
    if (checkers.is_empty()) {
        // 王手なし
        evasion_mask = ~Bitboard();
    } else if (checkers.count() > 1) {
        // 両王手
        evasion_mask = Bitboard();
    } else {
        // 単独王手
        int checker_index = checkers.lsb();
        evasion_mask.set(checker_index);

        auto king_position = get_king_position(current_turn);
        if (king_position.has_value()) {
            int king_square = king_position->col * Shogi::BOARD_ROWS + king_position->row;
            for (int dir = 0; dir < 8; ++dir) {
                Bitboard ray = AttackTable::get_ray(king_square, dir);
                if (ray.is_set(checker_index)) {
                    Bitboard between = ray ^ AttackTable::get_ray(checker_index, dir);
                    evasion_mask |= between;
                    break;
                }
            }
        }
    }

    // 盤上の駒
    for (int piece_type = 0; piece_type < Shogi::PIECE_TYPE_COUNT; ++piece_type) {
        PieceType type = static_cast<PieceType>(piece_type);
        Bitboard pieces = bitboard_piece_[static_cast<int>(current_turn)][piece_type];
        Bitboard promoted_pieces = bitboard_promoted_[static_cast<int>(current_turn)];

        // 両王手されている場合、玉以外の駒の移動は不可
        if (type != PieceType::KING && checkers.count() > 1) {
            continue;
        }

        while (!pieces.is_empty()) {
            int from_index = pieces.lsb();
            pieces.clear(from_index);

            Bitboard from_bitboard;
            from_bitboard.set(from_index);
            bool is_promoted = !(promoted_pieces & from_bitboard).is_empty();

            Coord from{from_index / Shogi::BOARD_ROWS, from_index % Shogi::BOARD_ROWS};
            Bitboard attacks;

            // 駒の利き
            if (is_promoted) {
                if (type == PieceType::BISHOP) {
                    attacks = AttackTable::get_promoted_bishop_attacks(from_index, occupancy);
                } else if (type == PieceType::ROOK) {
                    attacks = AttackTable::get_promoted_rook_attacks(from_index, occupancy);
                } else {
                    attacks = AttackTable::get_gold_attacks(from_index, current_turn);
                }
            } else {
                switch (type) {
                case PieceType::PAWN:
                    attacks = AttackTable::get_pawn_attacks(from_index, current_turn);
                    break;
                case PieceType::LANCE:
                    attacks = AttackTable::get_lance_attacks(from_index, current_turn, occupancy);
                    break;
                case PieceType::KNIGHT:
                    attacks = AttackTable::get_knight_attacks(from_index, current_turn);
                    break;
                case PieceType::SILVER:
                    attacks = AttackTable::get_silver_attacks(from_index, current_turn);
                    break;
                case PieceType::GOLD:
                    attacks = AttackTable::get_gold_attacks(from_index, current_turn);
                    break;
                case PieceType::BISHOP:
                    attacks = AttackTable::get_bishop_attacks(from_index, occupancy);
                    break;
                case PieceType::ROOK:
                    attacks = AttackTable::get_rook_attacks(from_index, occupancy);
                    break;
                case PieceType::KING:
                    attacks = AttackTable::get_king_attacks(from_index);
                    break;
                default:
                    break;
                }
            }

            attacks = attacks & ~my_pieces_bitboard;
            if (type != PieceType::KING) {
                if (!(pin_masks.pinned & from_bitboard).is_empty()) {
                    attacks &= pin_masks.valid_ray_masks[from_index];
                }

                attacks &= evasion_mask;
            }

            while (!attacks.is_empty()) {
                int to_index = attacks.lsb();
                attacks.clear(to_index);

                Coord to{to_index / Shogi::BOARD_ROWS, to_index % Shogi::BOARD_ROWS};

                Bitboard to_bitboard;
                to_bitboard.set(to_index);
                bool is_capture = !(opponent_pieces_bitboard & to_bitboard).is_empty();

                if (only_captures && !is_capture) {
                    continue;
                }

                if (type == PieceType::KING) {
                    if (!is_legal_move(from, to)) {
                        continue;
                    }
                }

                bool can_promote = false;
                bool must_promote = false;

                // 成り判定
                if (!is_promoted && type != PieceType::KING && type != PieceType::GOLD) {
                    if (is_promotion_rank(from.row) || is_promotion_rank(to.row)) {
                        can_promote = true;
                    }
                }

                if (is_dead_end(type, current_turn == Turn::GOTE, to.row)) {
                    must_promote = true;
                }

                if (!must_promote) {
                    move_list.push(Shogi::Move(from.col, from.row, to.col, to.row, type, false, false, is_capture));
                }

                if (can_promote) {
                    move_list.push(Shogi::Move(from.col, from.row, to.col, to.row, type, true, false, is_capture));
                }
            }
        }
    }

    // 持ち駒
    if (!only_captures && checkers.count() <= 1) {
        Bitboard empty_cells = ~occupancy;
        Bitboard valid_drop_targets = empty_cells & evasion_mask;

        if (!valid_drop_targets.is_empty()) {
            constexpr std::array<PieceType, 7> HAND_TYPES = {PieceType::PAWN,   PieceType::LANCE, PieceType::KNIGHT,
                                                             PieceType::SILVER, PieceType::GOLD,  PieceType::BISHOP,
                                                             PieceType::ROOK};

            for (PieceType type : HAND_TYPES) {
                if (get_hand_count(current_turn, type) == 0) {
                    continue;
                }

                Bitboard target_bitboard = valid_drop_targets;

                while (!target_bitboard.is_empty()) {
                    int to_index = target_bitboard.lsb();
                    target_bitboard.clear(to_index);

                    Coord to{to_index / Shogi::BOARD_ROWS, to_index % Shogi::BOARD_ROWS};

                    if (is_dead_end(type, current_turn == Turn::GOTE, to.row)) {
                        continue;
                    }
                    if (type == PieceType::PAWN && is_nifu(type, current_turn, to.col)) {
                        continue;
                    }

                    move_list.push(Shogi::Move(0, 0, to.col, to.row, type, false, true, false));
                }
            }
        }
    }
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
