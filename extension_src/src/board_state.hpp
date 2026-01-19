#ifndef BOARD_STATE_HPP
#define BOARD_STATE_HPP

#include <godot_cpp/classes/node2d.hpp>
#include <vector>

#include "shogi_utils.hpp"

using namespace godot;

struct Cell {
    int type;
    Shogi::Side side;
    bool is_promoted;

    [[nodiscard]] bool is_empty() const { return type == Shogi::EMPTY; }

    Cell() : type(Shogi::EMPTY), side(Shogi::PLAYER), is_promoted(false) {}
    Cell(int t, Shogi::Side s, bool p) : type(t), side(s), is_promoted(p) {}

    [[nodiscard]] bool operator==(const Cell &other) const {
        if (type == Shogi::EMPTY && other.type == Shogi::EMPTY) {
            return true;
        }

        return type == other.type && side == other.side && is_promoted == other.is_promoted;
    }

    [[nodiscard]] bool operator!=(const Cell &other) const { return !(*this == other); }
};

class BoardState {
  private:
    Cell board[Shogi::BOARD_SIZE];
    int hand[2][Shogi::PIECE_TYPE_COUNT];

    Shogi::Side side_to_move;
    uint64_t zobrist_hash;

    [[nodiscard]] uint64_t calculate_zobrist_hash() const;

    [[nodiscard]] bool is_valid_move(Shogi::Coord from, Shogi::Coord to) const;
    [[nodiscard]] bool is_valid_drop(int piece_type, bool is_enemy, Shogi::Coord to) const;
    [[nodiscard]] bool is_path_blocked(Shogi::Coord from, Shogi::Coord to) const;
    [[nodiscard]] bool is_nifu(int piece_type, Shogi::Side side, int col) const;
    [[nodiscard]] Shogi::Coord find_king_position(Shogi::Side side) const;

  public:
    BoardState(Shogi::Side side_to_move = Shogi::PLAYER);
    explicit BoardState(Node *main_node, Shogi::Side side_to_move);

    [[nodiscard]] Shogi::Side get_side_to_move() const { return side_to_move; }

    [[nodiscard]] bool operator==(const BoardState &other) const {
        if (side_to_move != other.side_to_move) {
            return false;
        }

        for (int i = 0; i < Shogi::BOARD_SIZE; ++i) {
            if (board[i] != other.board[i]) {
                return false;
            }
        }

        for (Shogi::Side side : {Shogi::PLAYER, Shogi::ENEMY}) {
            for (int piece_type = 0; piece_type < Shogi::PIECE_TYPE_COUNT; ++piece_type) {
                if (hand[side][piece_type] != other.hand[side][piece_type]) {
                    return false;
                }
            }
        }

        return true;
    }

    static void load_zobrist_params(const String &path);

    [[nodiscard]] uint64_t get_zobrist_hash() const;

    [[nodiscard]] bool is_legal_move(Shogi::Coord from, Shogi::Coord to) const;
    [[nodiscard]] bool is_legal_drop(int piece_type, bool is_enemy, Shogi::Coord to) const;
    [[nodiscard]] bool can_move_geometry(int piece_type, bool is_enemy, bool is_promoted, Shogi::Coord from,
                                         Shogi::Coord to) const;
    [[nodiscard]] bool is_dead_end(int piece_type, bool is_enemy, int to_row) const;
    [[nodiscard]] bool is_king_in_check(Shogi::Side side) const;

    // 盤面の操作
    [[nodiscard]] const Cell &get_cell(Shogi::Coord coord) const;
    void set_cell(Shogi::Coord coord, int type, Shogi::Side side, bool is_promoted);
    void clear_cell(Shogi::Coord coord);
    [[nodiscard]] int get_hand_count(Shogi::Side side, int piece_type) const;
    void apply_move(const Shogi::Move &move);

    // 盤面の出力（デバッグ用）
    void print_board() const;
};

#endif
