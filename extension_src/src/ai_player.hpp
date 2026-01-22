#ifndef AI_PLAYER_HPP
#define AI_PLAYER_HPP

#include "board_state.hpp"
#include "shogi_engine.hpp"
#include <godot_cpp/classes/node2d.hpp>
#include <godot_cpp/classes/ref_counted.hpp>
#include <unordered_map>

namespace godot {

enum class TTFlag : uint8_t { EXACT, LOWER_BOUND, UPPER_BOUND };

struct TTEntry {
    uint64_t hash;
    int score;
    int depth;
    TTFlag flag;
    Shogi::Move best_move;
};

class AIPlayer {

  private:
    static constexpr uint64_t TIME_LIMIT_USEC = 1000000; // 1秒
    static constexpr size_t TT_SIZE = 1 << 20;           // 約100万エントリ

    std::unordered_map<uint64_t, TTEntry> transposition_table_;

    [[nodiscard]] int evaluate(const BoardState &board);
    [[nodiscard]] int get_pst_value(Shogi::PieceType piece_type, Shogi::Turn turn, Shogi::Coord coord);
    [[nodiscard]] int alpha_beta(BoardState board, int depth, int alpha, int beta, Shogi::Turn turn, uint64_t end_time,
                                 bool &timeout, uint64_t &node_count);
    [[nodiscard]] int quiescence_search(BoardState board, int alpha, int beta, Shogi::Turn turn, uint64_t &node_count);
    [[nodiscard]] double calculate_win_probability(int score);

    [[nodiscard]] TTEntry *probe_tt(uint64_t hash);
    void store_tt(uint64_t hash, int score, int depth, TTFlag flag, const Shogi::Move &best_move);
    void clear_tt();

  public:
    AIPlayer() { transposition_table_.reserve(TT_SIZE); }
    ~AIPlayer() {}

    [[nodiscard]] Dictionary search_best_move(BoardState board);
};

} // namespace godot

#endif
