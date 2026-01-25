#ifndef BOARD_STATE_HPP
#define BOARD_STATE_HPP

#include "bitboard.hpp"
#include "shogi_utils.hpp"
#include <godot_cpp/classes/node2d.hpp>
#include <optional>
#include <vector>

using namespace godot;

struct Cell {
    Shogi::PieceType type;
    Shogi::Turn turn;
    bool is_promoted;

    [[nodiscard]] bool is_empty() const { return type == Shogi::PieceType::EMPTY; }

    Cell() : type(Shogi::PieceType::EMPTY), turn(Shogi::Turn::SENTE), is_promoted(false) {}
    Cell(Shogi::PieceType t, Shogi::Turn s, bool p) : type(t), turn(s), is_promoted(p) {}

    [[nodiscard]] bool operator==(const Cell &other) const {
        if (type == Shogi::PieceType::EMPTY && other.type == Shogi::PieceType::EMPTY) {
            return true;
        }

        return type == other.type && turn == other.turn && is_promoted == other.is_promoted;
    }

    [[nodiscard]] bool operator!=(const Cell &other) const { return !(*this == other); }
};

struct PinMasks {
    // ピンされている味方駒
    Bitboard pinned;

    // ピンされた駒の移動可能範囲
    Bitboard valid_ray_masks[Shogi::BOARD_SIZE];

    PinMasks() { pinned = Bitboard(); }
};

class BoardState {
  private:
    static constexpr int KING_DEFENSE_WEIGHTS[9] = {84992, 42496, 28330, 21248, 16998, 14165, 12141, 10624, 9443};
    static constexpr int KING_THREAT_WEIGHTS[9] = {94208, 47104, 31402, 23552, 18841, 15701, 13458, 11776, 10467};

    inline static bool attack_tables_initialized_;
    inline static bool eval_tables_initialized_;

    // 近接駒の利き
    inline static Bitboard attacks_pawn_[2][Shogi::BOARD_SIZE];
    inline static Bitboard attacks_knight_[2][Shogi::BOARD_SIZE];
    inline static Bitboard attacks_silver_[2][Shogi::BOARD_SIZE];
    inline static Bitboard attacks_gold_[2][Shogi::BOARD_SIZE];
    inline static Bitboard attacks_king_[Shogi::BOARD_SIZE];

    // 走り駒の利き
    inline static Bitboard sttacks_lance_[2][Shogi::BOARD_SIZE];
    inline static Bitboard rays_[8][Shogi::BOARD_SIZE];

    inline static int defense_weight_table_[Shogi::BOARD_SIZE][Shogi::BOARD_SIZE];
    inline static int threat_weight_table_[Shogi::BOARD_SIZE][Shogi::BOARD_SIZE];
    inline static int multi_effect_weight_table_[11];

    // すべての駒のBitboard
    Bitboard bitboard_all_;

    // 手番別のBitboard
    Bitboard bitboard_side_[2];

    // 手番別・駒種別のBitboard
    Bitboard bitboard_piece_[2][Shogi::PIECE_TYPE_COUNT];

    // 手番別の成り駒のBitboard
    Bitboard bitboard_promoted_[2];

    Cell board_[Shogi::BOARD_SIZE];
    int hand_[2][Shogi::PIECE_TYPE_COUNT];

    Shogi::Turn turn_to_move_;
    uint64_t zobrist_hash_;
    int score_;
    std::optional<Shogi::Coord> king_pos_[2];
    uint16_t pawn_columns_[2];

    static void initialize_attack_tables();
    static void initialize_eval_tables();

    static Bitboard get_ray(int square, int direction) { return rays_[direction][square]; }

    static const Bitboard &get_pawn_attacks(int square, Shogi::Turn turn) {
        return attacks_pawn_[static_cast<int>(turn)][square];
    }

    static const Bitboard &get_knight_attacks(int square, Shogi::Turn turn) {
        return attacks_knight_[static_cast<int>(turn)][square];
    }

    static const Bitboard &get_silver_attacks(int square, Shogi::Turn turn) {
        return attacks_silver_[static_cast<int>(turn)][square];
    }

    static const Bitboard &get_gold_attacks(int square, Shogi::Turn turn) {
        return attacks_gold_[static_cast<int>(turn)][square];
    }

    static const Bitboard &get_king_attacks(int square) { return attacks_king_[square]; }

    static int distance(int lhs, int rhs) {
        int col1 = lhs / Shogi::BOARD_ROWS;
        int row1 = lhs % Shogi::BOARD_ROWS;
        int col2 = rhs / Shogi::BOARD_ROWS;
        int row2 = rhs % Shogi::BOARD_ROWS;
        return std::max(std::abs(col1 - col2), std::abs(row1 - row2));
    }

    Bitboard get_lance_attacks(int square, Shogi::Turn turn, const Bitboard &occupancy) const;

    Bitboard get_bishop_attacks(int square, const Bitboard &occupancy) const;

    Bitboard get_rook_attacks(int square, const Bitboard &occupancy) const;

    Bitboard get_promoted_bishop_attacks(int square, const Bitboard &occupancy) const {
        return get_bishop_attacks(square, occupancy) | get_king_attacks(square);
    }

    Bitboard get_promoted_rook_attacks(int square, const Bitboard &occupancy) const {
        return get_rook_attacks(square, occupancy) | get_king_attacks(square);
    }

    Bitboard get_checkers(Shogi::Turn turn) const;

    void build_bitboard();
    void add_piece_to_bitboard(int index, Shogi::Turn turn, Shogi::PieceType type, bool is_promoted);
    void remove_piece_from_bitboard(int index, Shogi::Turn turn, Shogi::PieceType type, bool is_promoted);

    PinMasks calculate_pin_masks(Shogi::Turn turn) const;

    [[nodiscard]] uint64_t calculate_zobrist_hash() const;
    [[nodiscard]] int calculate_score() const;
    [[nodiscard]] int calculate_spatial_score() const;

    [[nodiscard]] bool is_valid_move(Shogi::Coord from, Shogi::Coord to) const;
    [[nodiscard]] bool is_valid_drop(Shogi::PieceType piece_type, bool is_enemy, Shogi::Coord to) const;
    [[nodiscard]] bool is_nifu(Shogi::PieceType piece_type, Shogi::Turn turn, int col) const;
    void update_king_position_cache();
    void update_pawn_columns_cache();

  public:
    BoardState(Shogi::Turn turn_to_move = Shogi::Turn::SENTE);
    explicit BoardState(Node *main_node, Shogi::Turn turn_to_move);

    [[nodiscard]] Shogi::Turn get_turn_to_move() const { return turn_to_move_; }
    [[nodiscard]] int get_score() const { return score_ + calculate_spatial_score(); }

    [[nodiscard]] bool operator==(const BoardState &other) const {
        if (turn_to_move_ != other.turn_to_move_) {
            return false;
        }

        for (int i = 0; i < Shogi::BOARD_SIZE; ++i) {
            if (board_[i] != other.board_[i]) {
                return false;
            }
        }

        for (Shogi::Turn turn : {Shogi::Turn::SENTE, Shogi::Turn::GOTE}) {
            for (int piece_type = 0; piece_type < Shogi::PIECE_TYPE_COUNT; ++piece_type) {
                if (hand_[static_cast<int>(turn)][piece_type] != other.hand_[static_cast<int>(turn)][piece_type]) {
                    return false;
                }
            }
        }

        return true;
    }

    static void load_zobrist_params(const String &path);

    [[nodiscard]] uint64_t get_zobrist_hash() const;

    [[nodiscard]] bool is_legal_move(Shogi::Coord from, Shogi::Coord to);
    [[nodiscard]] bool is_legal_drop(Shogi::PieceType piece_type, bool is_enemy, Shogi::Coord to);
    [[nodiscard]] bool is_dead_end(Shogi::PieceType piece_type, bool is_enemy, int to_row) const;
    [[nodiscard]] bool is_king_in_check(Shogi::Turn turn) const;
    [[nodiscard]] std::optional<Shogi::Coord> get_king_position(Shogi::Turn turn) const;

    [[nodiscard]] std::vector<Shogi::Move> get_legal_moves(bool only_captures = false);

    // 盤面の操作
    [[nodiscard]] const Cell &get_cell(Shogi::Coord coord) const;
    void set_cell(Shogi::Coord coord, Shogi::PieceType type, Shogi::Turn turn, bool is_promoted);
    void clear_cell(Shogi::Coord coord);
    [[nodiscard]] int get_hand_count(Shogi::Turn turn, Shogi::PieceType piece_type) const;
    [[nodiscard]] Shogi::UndoInfo apply_move(const Shogi::Move &move);
    void undo_move(const Shogi::UndoInfo &undo_info);

    // 盤面の出力（デバッグ用）
    void print_board() const;
};

#endif
