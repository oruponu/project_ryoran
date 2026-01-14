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

  public:
    BoardState();

    void init_from_main(Node *main_node);
    std::vector<Shogi::Move> get_legal_moves(int side) const;

    // 盤面情報の取得
    const Cell &get_cell(int col, int row) const;
    int get_hand_count(int side, int piece_type) const;

    // 座標が盤面内か
    static bool is_valid_coord(int col, int row) {
        return col >= 0 && col < Shogi::BOARD_COLS && row >= 0 && row < Shogi::BOARD_ROWS;
    }

    // 指定した列に歩が存在するか
    bool has_pawn_on_column(int side, int col) const;

    // 盤面を出力（デバッグ用）
    void print_board() const;
};

#endif
