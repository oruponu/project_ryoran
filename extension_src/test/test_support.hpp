#pragma once

#include "ai_player.hpp"
#include "board_state.hpp"
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

using Shogi::Move;
using Shogi::PieceType;
using Shogi::Turn;

constexpr PieceType P = PieceType::PAWN;
constexpr PieceType L = PieceType::LANCE;
constexpr PieceType N = PieceType::KNIGHT;
constexpr PieceType S = PieceType::SILVER;
constexpr PieceType G = PieceType::GOLD;
constexpr PieceType B = PieceType::BISHOP;
constexpr PieceType R = PieceType::ROOK;
constexpr PieceType K = PieceType::KING;

struct KMove {
	int from_file, from_rank, to_file, to_rank;
	PieceType piece_type;
	bool is_promotion;
};

extern const KMove KIFU2[];

void check(bool condition, const char *name);
int failure_count();

std::vector<uint8_t> read_file(const char *path);
void setup_initial_position(BoardState &board);
bool replay(BoardState &board, int move_count, const KMove *kifu);

struct AIPlayerTestAccess {
	AIPlayer &ai;

	static constexpr int QS_PLY_LIMIT = AIPlayer::QS_PLY_LIMIT;
	static constexpr uint32_t INFINITY_PN = AIPlayer::INFINITY_PN;
	static constexpr uint64_t NO_TIME_LIMIT = AIPlayer::NO_TIME_LIMIT;
	static constexpr int MAX_PLY = AIPlayer::MAX_PLY;
	static constexpr int RFP_MAX_DEPTH = AIPlayer::RFP_MAX_DEPTH;
	static constexpr int RFP_MARGIN = AIPlayer::RFP_MARGIN;
	static bool can_prune_node(bool in_check, int alpha, int beta) { return AIPlayer::can_prune_node(in_check, alpha, beta); }
	static bool can_use_partial_top(int partial_size, int count) { return AIPlayer::can_use_partial_top(partial_size, count); }
	static bool reverse_futility_cuts(int depth, int static_eval, int beta) {
		return AIPlayer::reverse_futility_cuts(depth, static_eval, beta);
	}
	static constexpr int FUTILITY_MAX_DEPTH = AIPlayer::FUTILITY_MAX_DEPTH;
	static constexpr int FUTILITY_MARGIN_1 = AIPlayer::FUTILITY_MARGINS[1];
	static constexpr int FUTILITY_MARGIN_2 = AIPlayer::FUTILITY_MARGINS[2];
	static bool is_prunable_quiet_move(const Move &move, bool gives_check, bool is_killer, int prior_move_count) {
		return AIPlayer::is_prunable_quiet_move(move, gives_check, is_killer, prior_move_count);
	}
	static bool futility_prunes(int depth, int static_eval, int alpha) { return AIPlayer::futility_prunes(depth, static_eval, alpha); }
	static constexpr int LMP_MAX_DEPTH = AIPlayer::LMP_MAX_DEPTH;
	static bool late_move_prunes(int depth, int prior_move_count) { return AIPlayer::late_move_prunes(depth, prior_move_count); }
	static constexpr int IIR_MIN_DEPTH = AIPlayer::IIR_MIN_DEPTH;
	static bool iir_reduces(int depth, bool has_tt_move, bool in_check) { return AIPlayer::iir_reduces(depth, has_tt_move, in_check); }
	static int lmr_reduction(int depth, int move_number) { return AIPlayer::lmr_reduction(depth, move_number); }
	static int null_move_reduction(int depth) { return AIPlayer::null_move_reduction(depth); }

	int get_move_ordering_score(const BoardState &board, const Move &move, int ply) {
		return ai.get_move_ordering_score(board, move, ply);
	}
	void sort_moves(const BoardState &board, Move *first, Move *last, int ply) { ai.sort_moves(board, first, last, ply); }
	void update_killer(int ply, const Move &move) { ai.update_killer(ply, move); }
	void update_history(Turn turn, const Move &move, int depth) { ai.update_history(turn, move, depth); }
	int quiescence_search(BoardState &board, int alpha, int beta, int ply, uint64_t &node_count, int qs_ply = 0) {
		ai.tt_.allocate(ai.tt_bytes_);
		bool timeout = false;
		return ai.quiescence_search(board, alpha, beta, ply, AIPlayer::NO_TIME_LIMIT, timeout, node_count, qs_ply);
	}
	int quiescence_search(BoardState &board, int alpha, int beta, int ply, uint64_t end_time, bool &timeout,
			uint64_t &node_count) {
		ai.tt_.allocate(ai.tt_bytes_);
		return ai.quiescence_search(board, alpha, beta, ply, end_time, timeout, node_count);
	}
	int alpha_beta(BoardState &board, int depth, int ply, int alpha, int beta, uint64_t end_time, bool &timeout,
			uint64_t &node_count) {
		ai.tt_.allocate(ai.tt_bytes_);
		return ai.alpha_beta(board, depth, ply, alpha, beta, end_time, timeout, node_count);
	}
	std::optional<int> detect_path_repetition(int ply, uint64_t hash) { return ai.detect_path_repetition(ply, hash); }
	std::optional<Move> find_mate(BoardState &board, int max_depth) { return ai.find_mate(board, max_depth); }
	std::optional<Move> find_mate(BoardState &board, int max_depth, uint64_t end_time) {
		return ai.find_mate(board, max_depth, 100000, end_time);
	}
	void dfpn_search(BoardState &board, Turn turn, int threshold_pn, int threshold_dn, int &pn, int &dn, int depth,
			uint64_t &node_count, uint64_t max_nodes) {
		ai.dfpn_search(board, turn, threshold_pn, threshold_dn, pn, dn, depth, node_count, max_nodes);
	}
	double calculate_win_probability(int score) { return ai.calculate_win_probability(score); }
	auto &path_hashes() { return ai.path_hashes_; }
	auto &path_in_check() { return ai.path_in_check_; }
	void set_time_check_countdown(int countdown) { ai.time_check_countdown_ = countdown; }
	TranspositionTable &tt() {
		ai.tt_.allocate(ai.tt_bytes_);
		return ai.tt_;
	}
	int max_depth() const { return ai.max_depth_; }
	auto &dfpn_path() { return ai.dfpn_path_; }
	auto &dfpn_table() { return ai.dfpn_table_; }
	static std::string format_percent(double value) { return AIPlayer::format_percent(value); }
};

void run_zobrist_tests(const std::vector<uint8_t> &data);
void run_rules_tests();
void run_search_tests();
void run_evaluator_tests();
void run_transposition_table_tests();
void run_usi_protocol_tests();
