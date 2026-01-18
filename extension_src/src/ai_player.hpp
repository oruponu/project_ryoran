#ifndef AI_PLAYER_HPP
#define AI_PLAYER_HPP

#include "board_state.hpp"
#include "shogi_engine.hpp"
#include <godot_cpp/classes/node2d.hpp>
#include <godot_cpp/classes/ref_counted.hpp>

namespace godot {

class AIPlayer {

  private:
    const uint64_t TIME_LIMIT_USEC = 1000000; // 1秒

    std::vector<Shogi::Move> get_legal_moves(const BoardState &board, Shogi::Side side, bool only_captures = false);
    int evaluate(const BoardState &board);
    int alpha_beta(BoardState board, int depth, int alpha, int beta, Shogi::Side side, uint64_t end_time,
                   bool &timeout);
    double calculate_win_probability(int score);

  public:
    AIPlayer() {}
    ~AIPlayer() {}

    Dictionary search_best_move(BoardState board);
};

} // namespace godot

#endif
