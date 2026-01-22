#ifndef BOARD_STATE_HPP
#define BOARD_STATE_HPP

#include <godot_cpp/classes/node2d.hpp>
#include <optional>
#include <vector>

#include "shogi_utils.hpp"

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

class BoardState {
  private:
    Cell board_[Shogi::BOARD_SIZE];
    int hand_[2][Shogi::PIECE_TYPE_COUNT];

    Shogi::Turn turn_to_move_;
    uint64_t zobrist_hash_;
    std::optional<Shogi::Coord> king_pos_[2];
    uint16_t pawn_columns_[2];

    [[nodiscard]] uint64_t calculate_zobrist_hash() const;

    [[nodiscard]] bool is_valid_move(Shogi::Coord from, Shogi::Coord to) const;
    [[nodiscard]] bool is_valid_drop(Shogi::PieceType piece_type, bool is_enemy, Shogi::Coord to) const;
    [[nodiscard]] bool is_path_blocked(Shogi::Coord from, Shogi::Coord to) const;
    [[nodiscard]] bool is_nifu(Shogi::PieceType piece_type, Shogi::Turn turn, int col) const;
    void update_king_position_cache();
    void update_pawn_columns_cache();

  public:
    BoardState(Shogi::Turn turn_to_move = Shogi::Turn::SENTE);
    explicit BoardState(Node *main_node, Shogi::Turn turn_to_move);

    [[nodiscard]] Shogi::Turn get_turn_to_move() const { return turn_to_move_; }

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
    [[nodiscard]] bool can_move_geometry(Shogi::PieceType piece_type, bool is_enemy, bool is_promoted,
                                         Shogi::Coord from, Shogi::Coord to) const;
    [[nodiscard]] bool is_dead_end(Shogi::PieceType piece_type, bool is_enemy, int to_row) const;
    [[nodiscard]] bool is_king_in_check(Shogi::Turn turn) const;
    [[nodiscard]] std::optional<Shogi::Coord> get_king_position(Shogi::Turn turn) const;

    [[nodiscard]] std::vector<Shogi::Move> get_legal_moves(bool only_captures = false);

    // 盤面の操作
    [[nodiscard]] const Cell &get_cell(Shogi::Coord coord) const;
    void set_cell(Shogi::Coord coord, Shogi::PieceType type, Shogi::Turn turn, bool is_promoted);
    void clear_cell(Shogi::Coord coord);
    [[nodiscard]] int get_hand_count(Shogi::Turn turn, Shogi::PieceType piece_type) const;
    void apply_move(const Shogi::Move &move);

    // 盤面の出力（デバッグ用）
    void print_board() const;
};

#endif
