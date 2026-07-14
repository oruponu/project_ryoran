#pragma once

#include "board_state.hpp"
#include <godot_cpp/variant/array.hpp>
#include <godot_cpp/variant/dictionary.hpp>
#include <unordered_map>
#include <unordered_set>
#include <vector>

[[nodiscard]] godot::Dictionary make_move_dictionary(const Shogi::Move &move, int score, double win_rate);

enum class TTFlag : uint8_t {
	EXACT,
	LOWER_BOUND,
	UPPER_BOUND
};

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
	static constexpr size_t TT_SIZE = 1 << 20; // 約100万エントリ
	static constexpr size_t DFPN_TT_SIZE = 1 << 20;
	static constexpr uint32_t INFINITY_PN = 10000000;
	static constexpr int MAX_PLY = 128;
	static constexpr int HISTORY_CAP = 700000; // Killer2（800000）を超えさせない上限

	std::unordered_map<uint64_t, TTEntry> transposition_table_;
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
	int history_len_ = 0;

	[[nodiscard]] int get_move_ordering_score(const BoardState &board, const Shogi::Move &move, int ply);
	[[nodiscard]] std::optional<int> detect_path_repetition(int ply, uint64_t hash, Shogi::Turn stm);
	[[nodiscard]] uint64_t hash_at(int v) const {
		return v >= history_len_ ? path_hashes_[v - history_len_] : game_history_hashes_[v];
	}
	[[nodiscard]] bool in_check_at(int v) const {
		return v >= history_len_ ? path_in_check_[v - history_len_] : game_history_in_check_[v];
	}
	void update_killer(int ply, const Shogi::Move &move);
	void update_history(Shogi::Turn turn, const Shogi::Move &move, int depth);
	[[nodiscard]] int alpha_beta(BoardState &board, int depth, int ply, int alpha, int beta, Shogi::Turn turn,
			uint64_t end_time, bool &timeout, uint64_t &node_count, bool can_null = true);
	std::optional<Shogi::Move> find_mate(BoardState &board, int max_depth, uint64_t max_nodes = 100000);
	void dfpn_search(BoardState &board, Shogi::Turn turn, int threshold_pn, int threshold_dn, int &pn, int &dn,
			int depth, uint64_t &node_count, const uint64_t max_nodes);
	void generate_check_moves(BoardState &board, Shogi::MoveList &move_list);
	[[nodiscard]] int quiescence_search(BoardState &board, int alpha, int beta, Shogi::Turn turn, int ply,
			uint64_t &node_count, int qs_ply = 0);
	[[nodiscard]] double calculate_win_probability(int score);

	[[nodiscard]] TTEntry *probe_tt(uint64_t hash);
	void store_tt(uint64_t hash, int score, int depth, TTFlag flag, const Shogi::Move &best_move);
	void clear_tt();

public:
	AIPlayer() {}
	~AIPlayer() {}

	[[nodiscard]] godot::Array search_top_moves(BoardState board, int count);
	void set_game_history(const std::vector<uint64_t> &hashes, const std::vector<bool> &in_checks);
};
