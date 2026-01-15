#ifndef BOARD_STATE_HPP
#define BOARD_STATE_HPP

#include <godot_cpp/classes/node2d.hpp>
#include <vector>

#include "shogi_utils.hpp"

using namespace godot;

struct Cell {
    int type;
    int8_t side;
    bool is_promoted;

    bool is_empty() const { return type == Shogi::EMPTY; }

    Cell() : type(Shogi::EMPTY), side(Shogi::PLAYER), is_promoted(false) {}
    Cell(int t, int s, bool p) : type(t), side(s), is_promoted(p) {}
};

class BoardState {
  private:
    Cell board[Shogi::BOARD_SIZE];
    int hand[2][Shogi::PIECE_TYPE_COUNT];

    // 座標が盤面内か
    static bool is_valid_coord(int col, int row) {
        return col >= 0 && col < Shogi::BOARD_COLS && row >= 0 && row < Shogi::BOARD_ROWS;
    }

    bool is_valid_move(int from_col, int from_row, int to_col, int to_row) const;
    bool is_valid_drop(int piece_type, bool is_enemy, int to_col, int to_row) const;
    bool is_path_blocked(int from_col, int from_row, int to_col, int to_row) const;
    bool is_nifu(int piece_type, int side, int col) const;
    std::pair<int, int> find_king_position(int side) const;

  public:
    BoardState();

    void init_from_main(Node *main_node);
    bool is_legal_move(int from_col, int from_row, int to_col, int to_row) const;
    bool is_legal_drop(int piece_type, bool is_enemy, int to_col, int to_row) const;
    bool can_move_geometry(int piece_type, bool is_enemy, bool is_promoted, int from_col, int from_row, int to_col,
                           int to_row) const;
    bool is_dead_end(int piece_type, bool is_enemy, int to_row) const;
    bool is_king_in_check(int side) const;

    // 盤面の操作
    const Cell &get_cell(int col, int row) const;
    void set_cell(int col, int row, int type, int side, bool is_promoted);
    void clear_cell(int col, int row);
    int get_hand_count(int side, int piece_type) const;
    void apply_move(const Shogi::Move &move, int side);

    // 盤面の出力（デバッグ用）
    void print_board() const;
};

#endif
