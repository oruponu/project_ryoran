#pragma once

#include "ai_player.hpp"
#include "board_state.hpp"
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

using Shogi::Coord;
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
	int ff, fr, tf, tr;
	PieceType pt;
	bool promo;
};

extern const KMove KIFU2[];

void check(bool condition, const char *name);
int failure_count();

std::vector<uint8_t> read_file(const char *path);
void setup_initial_position(BoardState &board);
bool replay(BoardState &board, int num_moves, const KMove *kifu);

struct AIPlayerTestAccess {
	AIPlayer &ai;

	static constexpr int QS_PLY_LIMIT = AIPlayer::QS_PLY_LIMIT;
	static constexpr uint32_t INFINITY_PN = AIPlayer::INFINITY_PN;

	int get_move_ordering_score(const BoardState &board, const Move &move, int ply) {
		return ai.get_move_ordering_score(board, move, ply);
	}
	void update_killer(int ply, const Move &move) { ai.update_killer(ply, move); }
	void update_history(Turn turn, const Move &move, int depth) { ai.update_history(turn, move, depth); }
	int quiescence_search(BoardState &board, int alpha, int beta, Turn turn, int ply, uint64_t &node_count,
			int qs_ply = 0) {
		return ai.quiescence_search(board, alpha, beta, turn, ply, node_count, qs_ply);
	}
	int alpha_beta(BoardState &board, int depth, int ply, int alpha, int beta, Turn turn, uint64_t end_time,
			bool &timeout, uint64_t &node_count) {
		return ai.alpha_beta(board, depth, ply, alpha, beta, turn, end_time, timeout, node_count);
	}
	std::optional<int> detect_path_repetition(int ply, uint64_t hash, Turn stm) {
		return ai.detect_path_repetition(ply, hash, stm);
	}
	std::optional<Move> find_mate(BoardState &board, int max_depth) { return ai.find_mate(board, max_depth); }
	void dfpn_search(BoardState &board, Turn turn, int threshold_pn, int threshold_dn, int &pn, int &dn, int depth,
			uint64_t &node_count, uint64_t max_nodes) {
		ai.dfpn_search(board, turn, threshold_pn, threshold_dn, pn, dn, depth, node_count, max_nodes);
	}
	auto &path_hashes() { return ai.path_hashes_; }
	auto &path_in_check() { return ai.path_in_check_; }
	auto &transposition_table() { return ai.transposition_table_; }
	auto &dfpn_path() { return ai.dfpn_path_; }
	auto &dfpn_table() { return ai.dfpn_table_; }
	static std::string format_percent(double value) { return AIPlayer::format_percent(value); }
};

void run_zobrist_tests(const std::vector<uint8_t> &data);
void run_rules_tests();
void run_search_tests();
