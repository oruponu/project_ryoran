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
    int get_pst_value(int piece_type, Shogi::Side side, Shogi::Coord coord);
    int alpha_beta(BoardState board, int depth, int alpha, int beta, Shogi::Side side, uint64_t end_time, bool &timeout,
                   uint64_t &node_count);
    int quiescence_search(BoardState board, int alpha, int beta, Shogi::Side side, uint64_t &node_count);
    double calculate_win_probability(int score);

  public:
    AIPlayer() {}
    ~AIPlayer() {}

    Dictionary search_best_move(BoardState board);
};

} // namespace godot

#endif
