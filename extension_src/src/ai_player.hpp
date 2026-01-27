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

struct DfpnEntry {
    uint64_t hash;
    uint32_t pn;
    uint32_t dn;
};

struct ChildNode {
    Shogi::Move move;
    uint64_t hash;
    int pn;
    int dn;
};

class AIPlayer {

  private:
    static constexpr uint64_t TIME_LIMIT_USEC = 1000000; // 1秒
    static constexpr size_t TT_SIZE = 1 << 20;           // 約100万エントリ
    static constexpr size_t DFPN_TT_SIZE = 1 << 20;
    static constexpr uint32_t INFINITY_PN = 10000000;

    std::unordered_map<uint64_t, TTEntry> transposition_table_;
    std::unordered_map<uint64_t, DfpnEntry> dfpn_table_;

    [[nodiscard]] int get_move_ordering_score(const BoardState &board, const Shogi::Move &move);
    [[nodiscard]] int alpha_beta(BoardState &board, int depth, int alpha, int beta, Shogi::Turn turn, uint64_t end_time,
                                 bool &timeout, uint64_t &node_count);
    std::optional<Shogi::Move> find_mate(BoardState &board, int max_depth);
    void dfpn_search(BoardState &board, Shogi::Turn turn, int threshold_pn, int threshold_dn, int &pn, int &dn,
                     int depth, uint64_t &node_count, const uint64_t max_nodes);
    void generate_check_moves(BoardState &board, Shogi::MoveList &move_list);
    [[nodiscard]] int quiescence_search(BoardState &board, int alpha, int beta, Shogi::Turn turn, uint64_t &node_count);
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
