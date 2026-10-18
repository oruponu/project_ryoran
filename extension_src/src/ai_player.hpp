#pragma once

#include "board_state.hpp"
#include "transposition_table.hpp"
#include <atomic>
#include <functional>
#include <optional>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

struct DfpnEntry {
	uint32_t pn;
	uint32_t dn;
};

struct ChildNode {
	Shogi::Move move;
	int pn;
	int dn;
};

struct ScoredMove {
	Shogi::Move move;
	int score;
	double win_rate;
};

class AIPlayer {
	friend struct AIPlayerTestAccess;

public:
	static constexpr int MATE_SCORE = 999999;
	static constexpr int MATE_BOUND = MATE_SCORE - 10000;
	static constexpr int INFINITE_SCORE = 99999999;

private:
	static constexpr uint64_t TIME_LIMIT_USEC = 1000000; // 1秒
	static constexpr size_t TT_BYTES = size_t{ 32 } << 20;
	uint64_t time_limit_usec_ = TIME_LIMIT_USEC;
	int max_depth_ = DEFAULT_MAX_DEPTH;
	int time_check_countdown_ = 0;
	static constexpr uint32_t INFINITY_PN = 10000000;
	static constexpr int MAX_PLY = 128;
	static constexpr int DEFAULT_MAX_DEPTH = 12;
	static constexpr int CAPTURE_ORDERING_BASE = 1000000;
	static constexpr int KILLER1_ORDERING_SCORE = 900000;
	static constexpr int KILLER2_ORDERING_SCORE = 800000;
	static constexpr int PROMOTION_ORDERING_BONUS = 20000;
	static constexpr int HISTORY_CAP = 700000; // Killer2を超えさせない上限
	static constexpr int RFP_MAX_DEPTH = 3;
	static constexpr int RFP_MARGIN = 200;
	static constexpr int FUTILITY_MAX_DEPTH = 2;
	static constexpr int FUTILITY_MARGINS[FUTILITY_MAX_DEPTH + 1] = { 0, 300, 550 };
	static constexpr int LMP_MAX_DEPTH = 3;
	// 静止探索を打ち切る深さ
	static constexpr int QS_PLY_LIMIT = 16;

	std::function<void(const std::string &)> logger_;
	std::function<void(const std::vector<ScoredMove> &, int)> depth_callback_;
	std::atomic<bool> stop_requested_ = false;

	TranspositionTable tt_;
	size_t tt_bytes_ = TT_BYTES;
	std::unordered_map<uint64_t, DfpnEntry> dfpn_table_;
	std::vector<uint64_t> dfpn_path_;
	Shogi::Move killer_moves_[MAX_PLY][2]; // 探索の深さごとに2スロット
	bool killer_valid_[MAX_PLY][2] = {};
	int history_[2][Shogi::PIECE_TYPE_COUNT][Shogi::BOARD_SIZE] = {};
	// 千日手（経路反復）：経路上の局面ハッシュと王手状態
	uint64_t path_hashes_[MAX_PLY + 1] = {};
	bool path_in_check_[MAX_PLY + 1] = {};
	std::vector<uint64_t> game_history_hashes_;
	std::vector<bool> game_history_in_check_;
	std::unordered_set<uint64_t> game_history_hash_set_;
	int game_history_length_ = 0;

	[[nodiscard]] int get_move_ordering_score(const BoardState &board, const Shogi::Move &move, int ply);
	void sort_moves(const BoardState &board, Shogi::Move *first, Shogi::Move *last, int ply);
	[[nodiscard]] std::optional<int> detect_path_repetition(int ply, uint64_t hash);
	[[nodiscard]] uint64_t hash_at(int v) const {
		return v >= game_history_length_ ? path_hashes_[v - game_history_length_] : game_history_hashes_[v];
	}
	[[nodiscard]] bool in_check_at(int v) const {
		return v >= game_history_length_ ? path_in_check_[v - game_history_length_] : game_history_in_check_[v];
	}
	void update_killer(int ply, const Shogi::Move &move);
	void update_history(Shogi::Turn turn, const Shogi::Move &move, int depth);
	[[nodiscard]] static bool can_prune_node(bool in_check, int alpha, int beta);
	[[nodiscard]] static bool reverse_futility_cuts(int depth, int static_eval, int beta);
	[[nodiscard]] static bool is_prunable_quiet_move(const Shogi::Move &move, bool gives_check, bool is_killer, int prior_move_count);
	[[nodiscard]] static bool futility_prunes(int depth, int static_eval, int alpha);
	[[nodiscard]] static bool late_move_prunes(int depth, int prior_move_count);
	[[nodiscard]] static int lmr_reduction(int depth, int move_number);
	[[nodiscard]] static int null_move_reduction(int depth);
	[[nodiscard]] int alpha_beta(BoardState &board, int depth, int ply, int alpha, int beta, uint64_t end_time,
			bool &timeout, uint64_t &node_count, bool can_null = true);
	std::optional<Shogi::Move> find_mate(BoardState &board, int max_depth, uint64_t max_nodes = 100000);
	void dfpn_search(BoardState &board, Shogi::Turn turn, int threshold_pn, int threshold_dn, int &pn, int &dn,
			int depth, uint64_t &node_count, const uint64_t max_nodes);
	void generate_check_moves(BoardState &board, Shogi::MoveList &move_list);
	[[nodiscard]] int quiescence_search(BoardState &board, int alpha, int beta, int ply, uint64_t &node_count,
			int qs_ply = 0);
	[[nodiscard]] double calculate_win_probability(int score);

	[[nodiscard]] bool should_stop(uint64_t end_time) const;
	void log(const std::string &message) const;
	[[nodiscard]] static std::string format_percent(double value);

public:
	[[nodiscard]] std::vector<ScoredMove> search_top_moves(BoardState board, int count);
	void set_game_history(const std::vector<uint64_t> &hashes, const std::vector<bool> &in_checks);
	void set_time_limit_usec(uint64_t usec);
	void set_max_depth(int depth);
	void set_tt_bytes(size_t bytes);
	void set_logger(std::function<void(const std::string &)> logger);
	void request_stop();
	void clear_stop();
	void set_depth_callback(std::function<void(const std::vector<ScoredMove> &, int)> callback);
};
