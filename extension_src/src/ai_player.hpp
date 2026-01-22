#ifndef AI_PLAYER_HPP
#define AI_PLAYER_HPP

#include "board_state.hpp"
#include "shogi_engine.hpp"
#include <godot_cpp/classes/node2d.hpp>
#include <godot_cpp/classes/ref_counted.hpp>

namespace godot {

class AIPlayer {

  private:
    static constexpr uint64_t TIME_LIMIT_USEC = 1000000; // 1秒

    [[nodiscard]] int evaluate(const BoardState &board);
    [[nodiscard]] int get_pst_value(Shogi::PieceType piece_type, Shogi::Turn turn, Shogi::Coord coord);
    [[nodiscard]] int alpha_beta(BoardState board, int depth, int alpha, int beta, Shogi::Turn turn, uint64_t end_time,
                                 bool &timeout, uint64_t &node_count);
    [[nodiscard]] int quiescence_search(BoardState board, int alpha, int beta, Shogi::Turn turn, uint64_t &node_count);
    [[nodiscard]] double calculate_win_probability(int score);

  public:
    AIPlayer() {}
    ~AIPlayer() {}

    [[nodiscard]] Dictionary search_best_move(BoardState board);
};

} // namespace godot

#endif
