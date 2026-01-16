#ifndef AI_PLAYER_HPP
#define AI_PLAYER_HPP

#include "board_state.hpp"
#include "shogi_engine.hpp"
#include <godot_cpp/classes/node2d.hpp>
#include <godot_cpp/classes/ref_counted.hpp>

namespace godot {

struct SearchTimeoutException {};

class AIPlayer {

  private:
    const uint64_t TIME_LIMIT_USEC = 1000000; // 1秒

    const int VAL_PAWN = 90;
    const int VAL_LANCE = 230;
    const int VAL_KNIGHT = 260;
    const int VAL_SILVER = 370;
    const int VAL_GOLD = 440;
    const int VAL_BISHOP = 570;
    const int VAL_ROOK = 640;
    const int VAL_KING = 99999;
    const int VAL_PRO_PAWN = 530;
    const int VAL_PRO_LANCE = 490;
    const int VAL_PRO_KNIGHT = 510;
    const int VAL_PRO_SILVER = 500;
    const int VAL_PRO_BISHOP = 830;
    const int VAL_PRO_ROOK = 950;

    bool is_enemy_side;

    std::vector<Shogi::Move> get_legal_moves(const BoardState &board, int side);
    int evaluate(const BoardState &board);
    int alpha_beta(BoardState board, int depth, int alpha, int beta, int side, uint64_t end_time);

  public:
    explicit AIPlayer(bool p_is_enemy_side) : is_enemy_side(p_is_enemy_side) {}
    ~AIPlayer() {}

    Dictionary get_next_move(Node2D *main_node);
};

} // namespace godot

#endif
