#include "ai_player.hpp"
#include "move_generator.hpp"
#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstring>
#include <format>
#include <functional>
#include <iterator>
#include <ranges>
#include <string>
#include <utility>
#include <vector>

using Shogi::Move;
using Shogi::Turn;

namespace {

// 不詰みの証明はノード数上限まで探索しがちなため、思考時間に収まるよう上限を用途別に分ける
constexpr int OWN_MATE_DEPTH = 21;
constexpr uint64_t OWN_MATE_NODES = 30000;
constexpr int MATE_CHECK_DEPTH = 9;
constexpr uint64_t MATE_CHECK_NODES = 5000;

// 後方の駒を取らない手は最善である可能性が低いため、先頭の手を除いて探索深さを削減する
constexpr int LMR_MOVE_THRESHOLD = 4;
constexpr int LMR_DEEP_MOVE_THRESHOLD = 12;

// Delta Pruningのマージン
constexpr int DELTA_MARGIN = 540;

// 詰みの評価値はルートからの手数を含むため、置換表にはその局面からの手数に変換して格納して取得時に戻す
int score_to_tt(int score, int ply) {
	if (score > AIPlayer::MATE_BOUND) {
		return score + ply;
	}
	if (score < -AIPlayer::MATE_BOUND) {
		return score - ply;
	}
	return score;
}

int score_from_tt(int score, int ply) {
	if (score > AIPlayer::MATE_BOUND) {
		return score - ply;
	}
	if (score < -AIPlayer::MATE_BOUND) {
		return score + ply;
	}
	return score;
}

int side_to_move_score(const BoardState &board) {
	int score = board.get_score();
	return (board.get_turn_to_move() == Turn::SENTE) ? score : -score;
}

struct RootMove {
	Shogi::Move move;
	int score;
};

uint64_t now_usec() {
	using namespace std::chrono;
	return static_cast<uint64_t>(duration_cast<microseconds>(steady_clock::now().time_since_epoch()).count());
}

} //namespace

void AIPlayer::set_time_limit_usec(uint64_t usec) {
	if (usec == 0) {
		return;
	}
	time_limit_usec_ = usec;
}

void AIPlayer::set_logger(std::function<void(const std::string &)> logger) {
	logger_ = std::move(logger);
}

void AIPlayer::log(const std::string &message) const {
	if (logger_) {
		logger_(message);
	}
}

std::string AIPlayer::format_percent(double value) {
	std::string text = std::format("{:.1f}", value);
	if (text.contains('.')) {
		while (text.back() == '0') {
			text.pop_back();
		}
		if (text.back() == '.') {
			text.pop_back();
		}
	}
	return text;
}

void AIPlayer::set_game_history(const std::vector<uint64_t> &hashes, const std::vector<bool> &in_checks) {
	// 境界外参照防止のため短い方に合わせる
	size_t n = std::min(hashes.size(), in_checks.size());
	game_history_hashes_.assign(hashes.begin(), hashes.begin() + n);
	game_history_in_check_.assign(in_checks.begin(), in_checks.begin() + n);
	game_history_length_ = static_cast<int>(n);
	game_history_hash_set_.clear();
	game_history_hash_set_.insert(game_history_hashes_.begin(), game_history_hashes_.end());
}

std::optional<int> AIPlayer::detect_path_repetition(int ply, uint64_t hash) {
	int v = game_history_length_ + ply;
	for (int p = v - 2; p >= 0; p -= 2) {
		if (p < game_history_length_ && !game_history_hash_set_.contains(hash)) {
			break; // 現局面が履歴に無ければ無駄な走査を打ち切る
		}
		if (hash_at(p) != hash) {
			continue;
		}

		bool turn_perpetual = true; // 手番側が連続王手
		for (int q = p + 1; q <= v - 1; q += 2) {
			if (!in_check_at(q)) {
				turn_perpetual = false;
				break;
			}
		}
		bool opponent_perpetual = true; // 相手が連続王手
		for (int q = p + 2; q <= v; q += 2) {
			if (!in_check_at(q)) {
				opponent_perpetual = false;
				break;
			}
		}

		int mate = MATE_SCORE - ply;
		if (turn_perpetual) {
			return -mate; // 連続王手の千日手は王手側の負け
		}
		if (opponent_perpetual) {
			return mate;
		}
		return 0;
	}
	return std::nullopt;
}

int AIPlayer::get_move_ordering_score(const BoardState &board, const Shogi::Move &move, int ply) {
	int score = 0;

	if (move.is_capture) {
		// 駒を取る手：MVV-LVA
		const Cell &target_cell = board.get_cell({ move.to_col, move.to_row });
		if (!target_cell.is_empty()) {
			int victim_value = Shogi::PIECE_VALUES[std::to_underlying(target_cell.piece_type)][target_cell.is_promoted ? 1 : 0];
			int aggressor_value = Shogi::PIECE_VALUES[std::to_underlying(move.piece_type)][0];
			// 高い駒を安い駒で取るほど高得点
			score = CAPTURE_ORDERING_BASE + victim_value - aggressor_value;
		}
	} else {
		// 駒を取らない手：Killer / History
		bool is_killer = false;
		if (ply < MAX_PLY) {
			if (killer_valid_[ply][0] && move == killer_moves_[ply][0]) {
				score = KILLER1_ORDERING_SCORE;
				is_killer = true;
			} else if (killer_valid_[ply][1] && move == killer_moves_[ply][1]) {
				score = KILLER2_ORDERING_SCORE;
				is_killer = true;
			}
		}
		if (!is_killer) {
			int turn_index = std::to_underlying(board.get_turn_to_move());
			int to_index = Shogi::to_index(move.to_col, move.to_row);
			score = history_[turn_index][std::to_underlying(move.piece_type)][to_index];
		}
	}

	// 成る手
	if (move.is_promotion) {
		score += PROMOTION_ORDERING_BONUS;
	}

	return score;
}

void AIPlayer::update_killer(int ply, const Shogi::Move &move) {
	if (ply >= MAX_PLY) {
		return;
	}
	if (killer_valid_[ply][0] && killer_moves_[ply][0] == move) {
		return;
	}
	killer_moves_[ply][1] = killer_moves_[ply][0];
	killer_valid_[ply][1] = killer_valid_[ply][0];
	killer_moves_[ply][0] = move;
	killer_valid_[ply][0] = true;
}

void AIPlayer::update_history(Shogi::Turn turn, const Shogi::Move &move, int depth) {
	int turn_index = std::to_underlying(turn);
	int to_index = Shogi::to_index(move.to_col, move.to_row);
	int &value = history_[turn_index][std::to_underlying(move.piece_type)][to_index];
	value = std::min(value + depth * depth, HISTORY_CAP);
}

int AIPlayer::alpha_beta(BoardState &board, int depth, int ply, int alpha, int beta, uint64_t end_time, bool &timeout,
		uint64_t &node_count, bool can_null) {
	++node_count;

	if (now_usec() > end_time) {
		timeout = true;
		return 0;
	}

	uint64_t hash = board.get_zobrist_hash();
	Turn turn = board.get_turn_to_move();
	bool in_check = MoveGenerator::is_king_in_check(board, turn);

	// 千日手（経路反復）：TTより前に判定
	if (ply < MAX_PLY) {
		path_in_check_[ply] = in_check;
		if (auto rep = detect_path_repetition(ply, hash); rep.has_value()) {
			return rep.value();
		}
		path_hashes_[ply] = hash;
	}

	int original_alpha = alpha;
	int original_beta = beta;
	Move tt_best_move{};
	bool has_tt_move = false;

	// 置換表の上限と下限で探索窓を狭めると保存時のフラグ分類が不正確になるため、カットのみに使う
	TTEntry *tt_entry = probe_tt(hash);
	if (tt_entry != nullptr && tt_entry->depth >= depth) {
		int tt_score = score_from_tt(tt_entry->score, ply);
		if (tt_entry->flag == TTFlag::EXACT) {
			return tt_score;
		} else if (tt_entry->flag == TTFlag::LOWER_BOUND && tt_score >= beta) {
			return tt_score;
		} else if (tt_entry->flag == TTFlag::UPPER_BOUND && tt_score <= alpha) {
			return tt_score;
		}
	}

	// TTから最善手を取得
	if (tt_entry != nullptr) {
		tt_best_move = tt_entry->best_move;
		has_tt_move = true;
	}

	if (depth <= 0) {
		// 静止探索を実行
		return quiescence_search(board, alpha, beta, ply, node_count);
	}

	// 王手延長：王手を受けた側は深さを減らさず読む
	const int extension = (in_check && ply < MAX_PLY) ? 1 : 0;

	Shogi::MoveList move_list;
	MoveGenerator::get_legal_moves(board, move_list);
	if (move_list.is_empty()) {
		// 投了（手番側の負け）
		return -(MATE_SCORE - ply);
	}

	// Null Move Pruning
	constexpr int NULL_MOVE_R = 2;
	if (can_null && depth >= 3 && !in_check && side_to_move_score(board) >= beta) {
		uint64_t null_hash = board.make_null_move();
		int null_score = -alpha_beta(board, depth - 1 - NULL_MOVE_R, ply + 1, -beta, -beta + 1, end_time, timeout,
				node_count, false);
		board.undo_null_move(null_hash);

		if (timeout) {
			return 0;
		}
		if (null_score >= beta) {
			return beta;
		}
	}

	if (has_tt_move) {
		auto it = std::ranges::find(move_list, tt_best_move);
		if (it != move_list.end()) {
			std::ranges::rotate(move_list.begin(), it, it + 1);
		}
	}

	auto sort_start = has_tt_move ? move_list.begin() + 1 : move_list.begin();
	std::ranges::sort(sort_start, move_list.end(), std::ranges::greater{},
			[&](const Move &move) { return get_move_ordering_score(board, move, ply); });

	auto is_killer_move = [&](const Move &move) {
		return ply < MAX_PLY && ((killer_valid_[ply][0] && killer_moves_[ply][0] == move) || (killer_valid_[ply][1] && killer_moves_[ply][1] == move));
	};

	Turn opponent_turn = Shogi::opponent(turn);
	Move best_move = move_list[0];
	int max_eval = -INFINITE_SCORE;
	int move_index = 0;
	for (const Move &move : move_list) {
		// Late Move Reductions
		int reduction = 0;
		if (depth >= 3 && move_index >= LMR_MOVE_THRESHOLD && !in_check && !move.is_capture && !move.is_promotion &&
				!is_killer_move(move)) {
			reduction = (depth >= 6 && move_index >= LMR_DEEP_MOVE_THRESHOLD) ? 2 : 1;
		}
		++move_index;

		Shogi::UndoInfo undo = board.apply_move(move);

		// 王手をかける手は詰み筋の見逃しを防ぐため削減しない
		if (reduction > 0 && MoveGenerator::is_king_in_check(board, opponent_turn)) {
			reduction = 0;
		}

		int eval = -alpha_beta(board, depth - 1 + extension - reduction, ply + 1, -beta, -alpha, end_time, timeout,
				node_count);

		if (!timeout && reduction > 0 && eval > alpha) {
			eval = -alpha_beta(board, depth - 1 + extension, ply + 1, -beta, -alpha, end_time, timeout, node_count);
		}

		board.undo_move(undo);

		if (timeout) {
			return 0;
		}

		if (eval > max_eval) {
			max_eval = eval;
			best_move = move;
		}
		alpha = std::max(alpha, eval);
		if (beta <= alpha) {
			if (!move.is_capture) {
				update_killer(ply, move);
				update_history(turn, move, depth);
			}
			break; // βカット
		}
	}

	TTFlag flag;
	if (max_eval <= original_alpha) {
		flag = TTFlag::UPPER_BOUND;
	} else if (max_eval >= original_beta) {
		flag = TTFlag::LOWER_BOUND;
	} else {
		flag = TTFlag::EXACT;
	}
	store_tt(hash, score_to_tt(max_eval, ply), depth, flag, best_move);

	return max_eval;
}

std::optional<Shogi::Move> AIPlayer::find_mate(BoardState &board, int max_depth, uint64_t max_nodes) {
	dfpn_table_.clear();
	dfpn_path_.clear();
	uint64_t node_count = 0;

	int pn = 1;
	int dn = 1;

	dfpn_search(board, board.get_turn_to_move(), INFINITY_PN, INFINITY_PN, pn, dn, max_depth, node_count, max_nodes);

	if (pn == 0) {
		Shogi::MoveList move_list;
		generate_check_moves(board, move_list);
		for (const auto &move : move_list) {
			Shogi::UndoInfo undo = board.apply_move(move);
			uint64_t hash = board.get_zobrist_hash();

			int child_pn = 1;
			if (auto it = dfpn_table_.find(hash); it != dfpn_table_.end()) {
				child_pn = it->second.pn;
			}

			board.undo_move(undo);

			if (child_pn == 0) {
				return move;
			}
		}
	}

	return std::nullopt;
}

void AIPlayer::generate_check_moves(BoardState &board, Shogi::MoveList &move_list) {
	move_list.clear();

	Shogi::MoveList candidates;
	MoveGenerator::get_legal_moves(board, candidates);
	Turn opponent_turn = Shogi::opponent(board.get_turn_to_move());
	for (const auto &move : candidates) {
		Shogi::UndoInfo undo = board.apply_move(move);
		if (MoveGenerator::is_king_in_check(board, opponent_turn)) {
			move_list.push(move);
		}

		board.undo_move(undo);
	}
}

void AIPlayer::dfpn_search(BoardState &board, Turn turn, int threshold_pn, int threshold_dn, int &pn, int &dn,
		int depth, uint64_t &node_count, const uint64_t max_nodes) {
	++node_count;
	uint64_t hash = board.get_zobrist_hash();

	// 経路上に再出現した局面（連続王手の千日手など）は強制詰みではないため不詰み扱いで返す
	for (uint64_t ancestor : dfpn_path_) {
		if (ancestor == hash) {
			pn = INFINITY_PN;
			dn = 0;
			// GHIを避けるため置換表には保存しない
			return;
		}
	}

	if (auto it = dfpn_table_.find(hash); it != dfpn_table_.end()) {
		const auto &entry = it->second;
		if (entry.pn == 0 || entry.dn == 0 || entry.pn >= INFINITY_PN || entry.dn >= INFINITY_PN) {
			pn = entry.pn;
			dn = entry.dn;
			return;
		}

		if (entry.pn >= threshold_pn || entry.dn >= threshold_dn) {
			pn = entry.pn;
			dn = entry.dn;
			return;
		}

		pn = entry.pn;
		dn = entry.dn;
	} else {
		dn = 1;
		pn = 1;
	}

	if (node_count > max_nodes) {
		return;
	}

	// pn/dnが進まないと親が同じ子を選び続けるため、深さ制限に達したノードは不詰み扱いで返す
	if (depth <= 0) {
		pn = INFINITY_PN;
		dn = 0;
		return;
	}

	bool is_attacker = (board.get_turn_to_move() == turn);
	Shogi::MoveList move_list;
	if (is_attacker) {
		generate_check_moves(board, move_list);
	} else {
		MoveGenerator::get_legal_moves(board, move_list);
	}

	if (move_list.is_empty()) {
		pn = is_attacker ? INFINITY_PN : 0;
		dn = is_attacker ? 0 : INFINITY_PN;
		dfpn_table_[hash] = { .pn = static_cast<uint32_t>(pn), .dn = static_cast<uint32_t>(dn) };
		return;
	}

	std::vector<ChildNode> children;
	children.reserve(move_list.size());

	for (const auto &move : move_list) {
		Shogi::UndoInfo undo = board.apply_move(move);
		uint64_t child_hash = board.get_zobrist_hash();
		int child_pn = 1;
		int child_dn = 1;

		if (auto it = dfpn_table_.find(child_hash); it != dfpn_table_.end()) {
			child_pn = it->second.pn;
			child_dn = it->second.dn;
		}

		children.push_back({ .move = move, .pn = child_pn, .dn = child_dn });
		board.undo_move(undo);

		if (is_attacker) {
			if (child_pn == 0) {
				pn = 0;
				dn = INFINITY_PN;
				dfpn_table_[hash] = { .pn = static_cast<uint32_t>(pn), .dn = static_cast<uint32_t>(dn) };
				return;
			}
		} else {
			if (child_pn >= INFINITY_PN) {
				pn = INFINITY_PN;
				dn = 0;
				dfpn_table_[hash] = { .pn = static_cast<uint32_t>(pn), .dn = static_cast<uint32_t>(dn) };
				return;
			}
		}
	}

	dfpn_path_.push_back(hash);
	while (true) {
		int min_pn = INFINITY_PN;
		int min_dn = INFINITY_PN;
		long long sum_pn = 0;
		long long sum_dn = 0;

		int best_index = -1;
		int second_best_pn = INFINITY_PN;
		int second_best_dn = INFINITY_PN;

		for (int i = 0; i < std::ssize(children); ++i) {
			const auto &child = children[i];
			if (is_attacker) {
				sum_dn += child.dn;
				if (child.pn < min_pn) {
					second_best_pn = min_pn;
					min_pn = child.pn;
					best_index = i;
				} else if (child.pn < second_best_pn) {
					second_best_pn = child.pn;
				}
			} else {
				sum_pn += child.pn;
				if (child.dn < min_dn) {
					second_best_dn = min_dn;
					min_dn = child.dn;
					best_index = i;
				} else if (child.dn < second_best_dn) {
					second_best_dn = child.dn;
				}
			}
		}

		if (sum_pn > INFINITY_PN) {
			sum_pn = INFINITY_PN;
		}
		if (sum_dn > INFINITY_PN) {
			sum_dn = INFINITY_PN;
		}

		if (is_attacker) {
			pn = min_pn;
			dn = static_cast<int>(sum_dn);
		} else {
			pn = static_cast<int>(sum_pn);
			dn = min_dn;
		}

		if (pn >= threshold_pn || dn >= threshold_dn || pn == 0 || dn == 0) {
			break;
		}
		if (node_count > max_nodes) {
			break;
		}

		ChildNode &best_child = children[best_index];
		int next_threshold_pn;
		int next_threshold_dn;

		if (is_attacker) {
			next_threshold_pn = std::min(threshold_pn, second_best_pn + 1);
			long long dn_others = sum_dn - best_child.dn;
			if (threshold_dn >= INFINITY_PN) {
				next_threshold_dn = INFINITY_PN;
			} else {
				next_threshold_dn = threshold_dn - static_cast<int>(dn_others);
				if (next_threshold_dn < 0) {
					next_threshold_dn = 0;
				}
			}
		} else {
			long long pn_others = sum_pn - best_child.pn;
			if (threshold_pn >= INFINITY_PN) {
				next_threshold_pn = INFINITY_PN;
			} else {
				next_threshold_pn = threshold_pn - static_cast<int>(pn_others);
				if (next_threshold_pn < 0) {
					next_threshold_pn = 0;
				}
			}

			next_threshold_dn = std::min(threshold_dn, second_best_dn + 1);
		}

		Shogi::UndoInfo undo = board.apply_move(best_child.move);

		dfpn_search(board, turn, next_threshold_pn, next_threshold_dn, best_child.pn, best_child.dn, depth - 1,
				node_count, max_nodes);

		board.undo_move(undo);
	}
	dfpn_path_.pop_back();

	dfpn_table_[hash] = { .pn = static_cast<uint32_t>(pn), .dn = static_cast<uint32_t>(dn) };
}

int AIPlayer::quiescence_search(BoardState &board, int alpha, int beta, int ply, uint64_t &node_count, int qs_ply) {
	++node_count;

	Turn turn = board.get_turn_to_move();
	bool in_check = MoveGenerator::is_king_in_check(board, turn);

	// 千日手（経路反復）：Stand-Patより前に判定
	if (ply < MAX_PLY) {
		uint64_t hash = board.get_zobrist_hash();
		path_in_check_[ply] = in_check;
		if (auto rep = detect_path_repetition(ply, hash); rep.has_value()) {
			return rep.value();
		}
		path_hashes_[ply] = hash;
	}

	int stand_pat = side_to_move_score(board);

	// 取り合い連鎖が長すぎる場合は打ち切り
	if (qs_ply >= QS_PLY_LIMIT) {
		return stand_pat;
	}

	if (!in_check) {
		if (stand_pat >= beta) {
			return beta;
		}

		if (stand_pat > alpha) {
			alpha = stand_pat;
		}
	}

	Shogi::MoveList move_list;
	MoveGenerator::get_legal_moves(board, move_list, !in_check);
	if (in_check && move_list.is_empty()) {
		// 詰み（手番側の負け）
		return -(MATE_SCORE - ply);
	}
	std::ranges::sort(move_list, std::ranges::greater{},
			[&](const Move &move) { return get_move_ordering_score(board, move, ply); });

	int max_eval = in_check ? -INFINITE_SCORE : stand_pat;

	for (const auto &move : move_list) {
		if (!in_check) {
			// Delta Pruning
			const Cell &victim = board.get_cell({ move.to_col, move.to_row });
			int victim_value = Shogi::PIECE_VALUES[std::to_underlying(victim.piece_type)][victim.is_promoted ? 1 : 0];
			int promotion_gain = 0;
			if (move.is_promotion) {
				int piece_type_index = std::to_underlying(move.piece_type);
				promotion_gain = Shogi::PIECE_VALUES[piece_type_index][1] - Shogi::PIECE_VALUES[piece_type_index][0];
			}
			if (stand_pat + victim_value + promotion_gain + DELTA_MARGIN <= alpha) {
				continue;
			}
			// Static Exchange Evaluation
			if (MoveGenerator::see(board, move) < 0) {
				continue;
			}
		}

		Shogi::UndoInfo undo = board.apply_move(move);
		int eval = -quiescence_search(board, -beta, -alpha, ply + 1, node_count, qs_ply + 1);
		board.undo_move(undo);

		max_eval = std::max(max_eval, eval);
		alpha = std::max(alpha, eval);

		if (alpha >= beta) {
			return beta; // βカット
		}
	}

	return max_eval;
}

double AIPlayer::calculate_win_probability(int score) {
	const double SCALING_FACTOR = 3333.0;
	return 1.0 / (1.0 + std::pow(10.0, -static_cast<double>(score) / SCALING_FACTOR));
}

TTEntry *AIPlayer::probe_tt(uint64_t hash) {
	auto it = transposition_table_.find(hash);
	if (it != transposition_table_.end()) {
		return &it->second;
	}
	return nullptr;
}

void AIPlayer::store_tt(uint64_t hash, int score, int depth, TTFlag flag, const Move &best_move) {
	auto it = transposition_table_.find(hash);
	if (it == transposition_table_.end() || it->second.depth <= depth) {
		transposition_table_[hash] = TTEntry{ .score = score, .depth = depth, .flag = flag, .best_move = best_move };
	}
}

void AIPlayer::clear_tt() {
	transposition_table_.clear();
}

std::vector<ScoredMove> AIPlayer::search_top_moves(BoardState board, int count) {
	if (count < 1) {
		return {};
	}

	Turn root_turn = board.get_turn_to_move();

	auto mate_move = find_mate(board, OWN_MATE_DEPTH, OWN_MATE_NODES);
	if (mate_move.has_value()) {
		log("Checkmate proven.");

		return { ScoredMove{ .move = mate_move.value(), .score = MATE_SCORE - 1, .win_rate = 1.0 } };
	}

	Shogi::MoveList move_list;
	MoveGenerator::get_legal_moves(board, move_list);

	if (move_list.is_empty()) {
		// 投了
		return {};
	}

	transposition_table_.reserve(TT_SIZE);

	if (transposition_table_.size() > TT_SIZE) {
		log(std::format("TT size exceeded limit, clearing. Size was: {}", transposition_table_.size()));
		clear_tt();
	}

	// Killerは探索ごとにクリア
	// Historyはエージングして対局内の学習を引き継ぐ
	std::memset(killer_valid_, 0, sizeof(killer_valid_));
	for (auto &turn_history : history_) {
		for (auto &piece_type_history : turn_history) {
			for (int &value : piece_type_history) {
				value >>= 1;
			}
		}
	}

	uint64_t start_time = now_usec();
	uint64_t strict_limit_time = start_time + time_limit_usec_;

	int max_depth_limit = 12;

	std::vector<RootMove> global_top;
	for (const Move &move : move_list | std::views::take(count)) {
		global_top.push_back({ .move = move, .score = -INFINITE_SCORE });
	}

	// TTから最善手を取得
	uint64_t root_hash = board.get_zobrist_hash();
	TTEntry *root_tt = probe_tt(root_hash);
	Move best_move_prev_iter = (root_tt != nullptr) ? root_tt->best_move : move_list[0];
	bool has_prev_best = (root_tt != nullptr);

	uint64_t total_node_count = 0;

	// ルート局面は反復深化を通じて不変であり、候補手ごとの詰み判定結果は再利用できる
	std::unordered_map<uint64_t, bool> root_mate_cache;

	// 千日手（経路反復）：ルート局面を経路の起点に登録
	path_hashes_[0] = board.get_zobrist_hash();
	path_in_check_[0] = MoveGenerator::is_king_in_check(board, root_turn);

	for (int depth = 1; depth <= max_depth_limit; ++depth) {
		if (depth > 1 && now_usec() > strict_limit_time) {
			log(std::format("Time limit reached before depth {}", depth));
			break;
		}

		std::ranges::sort(move_list, std::ranges::greater{},
				[&](const Move &move) { return get_move_ordering_score(board, move, 0); });

		if (has_prev_best) {
			auto it = std::ranges::find(move_list, best_move_prev_iter);
			if (it != move_list.end()) {
				std::ranges::rotate(move_list.begin(), it, it + 1);
			}
		}

		std::vector<RootMove> depth_top;

		uint64_t search_cutoff_time = (depth == 1) ? UINT64_MAX : strict_limit_time;
		bool timeout = false;

		for (const auto &move : move_list) {
			if (depth > 1 && now_usec() > strict_limit_time) {
				timeout = true;
				break;
			}

			// MultiPV: 暫定リストの最下位より悪い手は候補に残らない
			int alpha = -INFINITE_SCORE;
			int beta = INFINITE_SCORE;
			if (std::ssize(depth_top) >= count) {
				alpha = depth_top.back().score;
			}

			Shogi::UndoInfo undo = board.apply_move(move);

			int score = -alpha_beta(board, depth - 1, 1, -beta, -alpha, search_cutoff_time, timeout, total_node_count);

			// 全候補手の詰み検証は合法手の多い終盤で時間がかかりすぎるため、暫定上位に入る手のみ検証する
			bool candidate = std::ssize(depth_top) < count || score > depth_top.back().score;
			if (!timeout && candidate) {
				uint64_t child_hash = board.get_zobrist_hash();
				auto mate_it = root_mate_cache.find(child_hash);
				bool is_mated;
				if (mate_it != root_mate_cache.end()) {
					is_mated = mate_it->second;
				} else {
					is_mated = find_mate(board, MATE_CHECK_DEPTH, MATE_CHECK_NODES).has_value();
					root_mate_cache.emplace(child_hash, is_mated);
				}

				if (is_mated) {
					// 詰みまでの手数が不明なため、どの詰みよりも速い扱いにする
					score = -(MATE_SCORE - 1);
				}
			}

			board.undo_move(undo);

			if (timeout) {
				break;
			}

			if (std::ssize(depth_top) < count || score > depth_top.back().score) {
				auto it = std::ranges::find_if(depth_top, [&](const RootMove &entry) { return score > entry.score; });
				depth_top.insert(it, { .move = move, .score = score });
				if (std::ssize(depth_top) > count) {
					depth_top.pop_back();
				}
			}
		}

		if (timeout) {
			log(std::format("Time limit reached before depth {}", depth));
			break;
		}

		global_top = depth_top;
		best_move_prev_iter = global_top[0].move;
		has_prev_best = true;

		double win_prob = calculate_win_probability(global_top[0].score);
		log(std::format("Depth {} completed. BestScore: {}, WinRate: {}%", depth, global_top[0].score,
				format_percent(win_prob * 100.0)));

		// 詰み筋を見つけたら打ち切り
		if (global_top[0].score >= MATE_BOUND || global_top[0].score <= -MATE_BOUND) {
			log(std::format("Checkmate found at depth {}", depth));
			break;
		}
	}

	log(std::format("Total nodes searched: {}, TT size: {}", total_node_count, transposition_table_.size()));

	// 手番側が勝ちを読み切った局面では1手だけを返す
	if (global_top[0].score >= MATE_BOUND) {
		global_top.resize(1);
	}

	std::vector<ScoredMove> result;
	result.reserve(global_top.size());
	for (const RootMove &entry : global_top) {
		result.push_back({ .move = entry.move, .score = entry.score, .win_rate = calculate_win_probability(entry.score) });
	}

	return result;
}
