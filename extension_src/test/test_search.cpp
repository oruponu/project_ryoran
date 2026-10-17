#include "move_generator.hpp"
#include "test_support.hpp"
#include "usi_protocol.hpp"
#include <algorithm>
#include <cstdio>
#include <regex>
#include <string>
#include <vector>

namespace {

const char *NON_BOOK_SENTE_SFEN = "lnsgkgsnl/1r5b1/ppppppppp/9/9/9/PPPPPPPPP/1B5R1/LNSGKGSNL b P 1";

void run_logger_tests() {
	std::optional<BoardState> board = BoardState::from_sfen(NON_BOOK_SENTE_SFEN);
	check(board.has_value(), "logger: test position parses");
	if (!board.has_value()) {
		return;
	}

	AIPlayer ai;
	ai.set_time_limit_usec(300000);
	std::vector<std::string> lines;
	ai.set_logger([&lines](const std::string &line) { lines.push_back(line); });
	std::vector<ScoredMove> moves = ai.search_top_moves(*board, 3);

	check(!moves.empty() && moves.size() <= 3, "search_top_moves: returns 1-3 moves");
	bool ordered = true;
	bool rates_in_range = true;
	for (size_t i = 0; i < moves.size(); ++i) {
		rates_in_range = rates_in_range && moves[i].win_rate >= 0.0 && moves[i].win_rate <= 1.0;
		if (i > 0 && moves[i - 1].score < moves[i].score) {
			ordered = false;
		}
	}
	check(ordered, "search_top_moves: best move first");
	check(rates_in_range, "search_top_moves: win_rate within [0, 1]");

	const std::regex depth_line(R"(Depth \d+ completed\. BestScore: -?\d+, WinRate: \d+(\.[1-9])?%)");
	const std::regex total_line(R"(Total nodes searched: \d+, TT hashfull: \d+)");
	bool has_depth = false;
	bool depth_format = true;
	bool has_total = false;
	for (const std::string &line : lines) {
		if (line.rfind("Depth ", 0) == 0) {
			has_depth = true;
			depth_format = depth_format && std::regex_match(line, depth_line);
		}
		if (std::regex_match(line, total_line)) {
			has_total = true;
		}
	}
	check(has_depth, "logger: receives depth lines");
	check(depth_format, "logger: depth line format matches (WinRate drops trailing .0)");
	check(has_total, "logger: receives total nodes line");

	AIPlayer silent;
	silent.set_time_limit_usec(100000);
	check(!silent.search_top_moves(*board, 1).empty(), "search_top_moves: works without logger");
}

void run_max_depth_tests() {
	std::optional<BoardState> board = BoardState::from_sfen(NON_BOOK_SENTE_SFEN);
	check(board.has_value(), "max_depth: test position parses");
	if (!board.has_value()) {
		return;
	}

	AIPlayer ai;
	ai.set_max_depth(3);
	ai.set_time_limit_usec(60'000'000);
	std::vector<std::string> lines;
	ai.set_logger([&lines](const std::string &line) { lines.push_back(line); });
	static_cast<void>(ai.search_top_moves(*board, 1));

	std::string last_depth_line;
	for (const std::string &line : lines) {
		if (line.starts_with("Depth ")) {
			last_depth_line = line;
		}
	}
	check(last_depth_line.starts_with("Depth 3 completed."), "max_depth: search stops after depth 3");

	AIPlayer clamped;
	AIPlayerTestAccess clamped_t{ clamped };
	clamped.set_max_depth(1000);
	check(clamped_t.max_depth() == AIPlayerTestAccess::MAX_PLY - 1, "max_depth: clamped to fit the transposition table depth");
}

std::vector<int> completed_depths(const std::vector<std::string> &lines) {
	std::vector<int> depths;
	for (const std::string &line : lines) {
		int depth = 0;
		if (std::sscanf(line.c_str(), "Depth %d completed.", &depth) == 1) {
			depths.push_back(depth);
		}
	}
	return depths;
}

void run_stop_tests() {
	std::optional<BoardState> board = BoardState::from_sfen(NON_BOOK_SENTE_SFEN);
	check(board.has_value(), "stop: test position parses");
	if (!board.has_value()) {
		return;
	}

	AIPlayer ai;
	ai.set_max_depth(3);
	ai.set_time_limit_usec(60'000'000);
	std::vector<std::string> lines;
	ai.set_logger([&lines](const std::string &line) { lines.push_back(line); });

	ai.request_stop();
	std::vector<ScoredMove> moves = ai.search_top_moves(*board, 3);
	check(completed_depths(lines) == std::vector<int>{ 1 }, "stop: requested stop still completes depth 1 only");
	check(!moves.empty(), "stop: returns depth 1 result");

	lines.clear();
	static_cast<void>(ai.search_top_moves(*board, 3));
	check(completed_depths(lines) == std::vector<int>{ 1 }, "stop: search does not clear the stop flag");

	lines.clear();
	ai.clear_stop();
	static_cast<void>(ai.search_top_moves(*board, 3));
	check(completed_depths(lines) == std::vector<int>{ 1, 2, 3 }, "clear_stop: search reaches max depth again");
}

bool same_moves(const std::vector<ScoredMove> &a, const std::vector<ScoredMove> &b) {
	return std::ranges::equal(a, b, [](const ScoredMove &x, const ScoredMove &y) {
		return x.move == y.move && x.score == y.score && x.win_rate == y.win_rate;
	});
}

void run_depth_callback_tests() {
	std::optional<BoardState> board = BoardState::from_sfen(NON_BOOK_SENTE_SFEN);
	check(board.has_value(), "depth_callback: test position parses");
	if (!board.has_value()) {
		return;
	}

	{
		AIPlayer ai;
		ai.set_max_depth(4);
		ai.set_time_limit_usec(60'000'000);
		std::vector<int> depths;
		std::vector<ScoredMove> last;
		ai.set_depth_callback([&](const std::vector<ScoredMove> &moves, int depth) {
			depths.push_back(depth);
			last = moves;
		});
		std::vector<ScoredMove> result = ai.search_top_moves(*board, 3);
		check(depths == std::vector<int>{ 1, 2, 3, 4 }, "depth_callback: called once per completed depth in order");
		check(same_moves(last, result), "depth_callback: last call matches the returned moves");
	}

	{
		AIPlayer ai;
		ai.set_max_depth(6);
		ai.set_time_limit_usec(60'000'000);
		std::vector<int> depths;
		std::vector<ScoredMove> at_depth2;
		ai.set_depth_callback([&](const std::vector<ScoredMove> &moves, int depth) {
			depths.push_back(depth);
			if (depth == 2) {
				at_depth2 = moves;
				ai.request_stop();
			}
		});
		std::vector<ScoredMove> result = ai.search_top_moves(*board, 3);
		check(depths == std::vector<int>{ 1, 2 }, "depth_callback: stop requested at depth 2 ends the search");
		check(same_moves(at_depth2, result), "depth_callback: stopped search returns the depth 2 moves");
	}

	{
		std::optional<BoardState> mate = BoardState::from_sfen("4k4/9/4G4/9/9/9/9/9/4K4 b G 1");
		check(mate.has_value(), "depth_callback: mate position parses");
		if (!mate.has_value()) {
			return;
		}
		AIPlayer ai;
		int calls = 0;
		ai.set_depth_callback([&](const std::vector<ScoredMove> &, int) { ++calls; });
		std::vector<ScoredMove> result = ai.search_top_moves(*mate, 3);
		check(result.size() == 1 && calls == 0, "depth_callback: root mate returns without depth calls");
	}
}

void run_tt_persistence_tests() {
	std::optional<BoardState> first = BoardState::from_sfen(NON_BOOK_SENTE_SFEN);
	std::optional<BoardState> second =
			BoardState::from_sfen("lnsg1g2l/1rk3s2/p1pppp1pp/6p2/2N6/1SP2PPP1/PPNPR3P/3g2S2/+p4K1NL b 2BLPg 41");
	check(first.has_value() && second.has_value(), "tt persistence: test positions parse");
	if (!first.has_value() || !second.has_value()) {
		return;
	}

	AIPlayer ai;
	AIPlayerTestAccess ai_t{ ai };
	ai.set_max_depth(3);
	ai.set_time_limit_usec(60'000'000);
	std::vector<ScoredMove> moves = ai.search_top_moves(*first, 1);
	check(!moves.empty(), "tt persistence: first search returns a move");
	if (moves.empty()) {
		return;
	}

	BoardState child = *first;
	static_cast<void>(child.apply_move(moves[0].move));
	uint64_t child_hash = child.get_zobrist_hash();
	check(ai_t.tt().probe(child_hash).has_value(), "tt persistence: the best move's child is stored");

	static_cast<void>(ai.search_top_moves(*second, 1));
	check(ai_t.tt().probe(child_hash).has_value(), "tt persistence: entries survive the next search of another position");
}

void run_format_percent_tests() {
	check(AIPlayerTestAccess::format_percent(50.0) == "50", "format_percent: 50.0 drops trailing .0");
	check(AIPlayerTestAccess::format_percent(49.96) == "50", "format_percent: rounds up to integer without .0");
	check(AIPlayerTestAccess::format_percent(53.24) == "53.2", "format_percent: keeps one decimal");
	check(AIPlayerTestAccess::format_percent(100.0) == "100", "format_percent: 100.0 keeps integer zeros");
	check(AIPlayerTestAccess::format_percent(0.0) == "0", "format_percent: 0.0 becomes 0");
}

void run_win_probability_tests() {
	AIPlayer ai;
	AIPlayerTestAccess ai_t{ ai };
	check(ai_t.calculate_win_probability(-AIPlayer::MATE_BOUND) == 0.0, "calculate_win_probability: being mated is exactly 0");
	check(ai_t.calculate_win_probability(-(AIPlayer::MATE_SCORE - 1)) == 0.0,
			"calculate_win_probability: mated in 1 is exactly 0");
	check(ai_t.calculate_win_probability(AIPlayer::MATE_BOUND) == 1.0, "calculate_win_probability: mating is exactly 1");
	check(ai_t.calculate_win_probability(AIPlayer::MATE_SCORE - 1) == 1.0,
			"calculate_win_probability: mate in 1 is exactly 1");
	check(ai_t.calculate_win_probability(-(AIPlayer::MATE_BOUND - 1)) > 0.0,
			"calculate_win_probability: losing without mate stays above 0");
	check(ai_t.calculate_win_probability(0) == 0.5, "calculate_win_probability: even score is 0.5");
}

void run_move_ordering_tests() {
	BoardState board(Turn::SENTE);
	AIPlayer ai;
	AIPlayerTestAccess ai_t{ ai };

	// quiet手は初期状態でスコア0（history未学習・killerなし）
	Move quiet(1, 6, 1, 5, P, false, false, false);
	check(ai_t.get_move_ordering_score(board, quiet, 3) == 0, "quiet move scores 0 initially");

	Move k1(6, 6, 6, 5, P, false, false, false);
	ai_t.update_killer(3, k1);
	check(ai_t.get_move_ordering_score(board, k1, 3) == 900000, "killer1 scores 900000");

	// 新しいkillerが入るとスロットがシフトし、旧killer1はkiller2(800000)になる
	Move k2(0, 8, 1, 8, L, false, false, false);
	ai_t.update_killer(3, k2);
	check(ai_t.get_move_ordering_score(board, k2, 3) == 900000, "new killer1 scores 900000");
	check(ai_t.get_move_ordering_score(board, k1, 3) == 800000, "old killer shifts to killer2");

	ai_t.update_killer(3, k2);
	check(ai_t.get_move_ordering_score(board, k1, 3) == 800000, "re-registering killer1 does not shift");

	check(ai_t.get_move_ordering_score(board, k1, 4) == 0, "killer is per-ply");

	Move h1(2, 6, 2, 5, P, false, false, false);
	ai_t.update_history(Turn::SENTE, h1, 4);
	check(ai_t.get_move_ordering_score(board, h1, 3) == 16, "history adds depth*depth");

	for (int i = 0; i < 100000; ++i) {
		ai_t.update_history(Turn::SENTE, h1, 10);
	}
	check(ai_t.get_move_ordering_score(board, h1, 3) == 700000, "history clamped at HISTORY_CAP");

	BoardState gote_board(Turn::GOTE);
	check(ai_t.get_move_ordering_score(gote_board, h1, 3) == 0, "history is per-side");

	// 駒取り手はkiller/historyの影響を受けずMVV-LVA帯（歩を歩で取る = 1000000）
	board.set_cell({ 2, 5 }, P, Turn::GOTE, false);
	Move cap(2, 6, 2, 5, P, false, false, true);
	ai_t.update_killer(3, cap);
	check(ai_t.get_move_ordering_score(board, cap, 3) == 1000000, "capture stays in MVV-LVA band");

	// 成りの加点をkillerの点に足しても駒取り帯を超えない
	Move kp(6, 2, 6, 1, S, true, false, false);
	ai_t.update_killer(5, kp);
	check(ai_t.get_move_ordering_score(board, kp, 5) == 920000, "killer + promotion = 920000");
}

void run_sort_moves_tests() {
	std::optional<BoardState> board = BoardState::from_sfen(NON_BOOK_SENTE_SFEN);
	check(board.has_value(), "sort_moves: test position parses");
	if (!board.has_value()) {
		return;
	}

	Shogi::MoveList generated;
	MoveGenerator::get_legal_moves(*board, generated);
	AIPlayer ai;
	AIPlayerTestAccess ai_t{ ai };

	Shogi::MoveList sorted = generated;
	ai_t.sort_moves(*board, sorted.begin(), sorted.end(), 0);
	check(std::ranges::equal(sorted, generated), "sort_moves: equal scores keep generation order");

	ai_t.update_history(Turn::SENTE, generated[generated.size() / 2], 4);
	ai_t.update_killer(0, generated[generated.size() - 1]);
	Shogi::MoveList expected = generated;
	std::ranges::stable_sort(expected, std::ranges::greater{},
			[&](const Move &move) { return ai_t.get_move_ordering_score(*board, move, 0); });
	sorted = generated;
	ai_t.sort_moves(*board, sorted.begin(), sorted.end(), 0);
	check(std::ranges::equal(sorted, expected), "sort_moves: higher scores first, ties in generation order");
}

void run_see_tests() {
	{
		// A: タダ取り（守りのない後手歩を先手飛で取る）
		BoardState board(Turn::SENTE);
		board.set_cell({ 4, 4 }, P, Turn::GOTE, false);
		board.set_cell({ 4, 8 }, R, Turn::SENTE, false);
		Move move(4, 8, 4, 4, R, false, false, true);
		check(MoveGenerator::see(board, move) == 90, "SEE: free pawn capture = +90");
	}
	{
		// B: 歩で守られた歩を飛車で取る → 取り返されて損
		BoardState board(Turn::SENTE);
		board.set_cell({ 4, 4 }, P, Turn::GOTE, false);
		board.set_cell({ 4, 3 }, P, Turn::GOTE, false); // 守りの歩
		board.set_cell({ 4, 8 }, R, Turn::SENTE, false);
		Move move(4, 8, 4, 4, R, false, false, true);
		check(MoveGenerator::see(board, move) == -900, "SEE: defended pawn capture by rook = -900");
	}
	{
		// C: 取り合い連鎖 — 金が取り返すと飛に取られるため後手は取り返さない
		//（negamax 畳み込みの検証: 後手が取り返しを放棄して +495 で確定）
		BoardState board(Turn::SENTE);
		board.set_cell({ 4, 4 }, S, Turn::GOTE, false);
		board.set_cell({ 4, 3 }, G, Turn::GOTE, false); // 守りの金
		board.set_cell({ 5, 5 }, S, Turn::SENTE, false); // 取る銀
		board.set_cell({ 4, 8 }, R, Turn::SENTE, false); // 控えの飛
		Move move(5, 5, 4, 4, S, false, false, true);
		check(MoveGenerator::see(board, move) == 495, "SEE: gold declines recapture = +495");
	}
	{
		// D: 香車の x-ray — 取る歩の後ろの香が取り合いに参加する
		BoardState board(Turn::SENTE);
		board.set_cell({ 4, 4 }, S, Turn::GOTE, false);
		board.set_cell({ 4, 3 }, P, Turn::GOTE, false); // 守りの歩
		board.set_cell({ 4, 5 }, P, Turn::SENTE, false); // 取る歩
		board.set_cell({ 4, 7 }, L, Turn::SENTE, false); // 歩の後ろの香
		Move move(4, 5, 4, 4, P, false, false, true);
		check(MoveGenerator::see(board, move) == 495, "SEE: x-ray lance joins = +495");

		// 香がいなければ歩を取り返されて 495-90=405
		BoardState board2(Turn::SENTE);
		board2.set_cell({ 4, 4 }, S, Turn::GOTE, false);
		board2.set_cell({ 4, 3 }, P, Turn::GOTE, false);
		board2.set_cell({ 4, 5 }, P, Turn::SENTE, false);
		check(MoveGenerator::see(board2, move) == 405, "SEE: without lance = +405");
	}
	{
		// E: 玉は相手の利きが残るマスへ取り返しに出られない
		BoardState board(Turn::SENTE);
		board.set_cell({ 4, 4 }, P, Turn::GOTE, false);
		board.set_cell({ 4, 3 }, K, Turn::GOTE, false); // 守りの玉
		board.set_cell({ 4, 8 }, R, Turn::SENTE, false);
		board.set_cell({ 3, 5 }, G, Turn::SENTE, false); // 55地点に利きを足す金（玉の取り返しを禁止）
		Move move(4, 8, 4, 4, R, false, false, true);
		check(MoveGenerator::see(board, move) == 90, "SEE: king cannot recapture into attack = +90");

		// 金がいなければ玉が取り返せて -900
		BoardState board2(Turn::SENTE);
		board2.set_cell({ 4, 4 }, P, Turn::GOTE, false);
		board2.set_cell({ 4, 3 }, K, Turn::GOTE, false);
		board2.set_cell({ 4, 8 }, R, Turn::SENTE, false);
		check(MoveGenerator::see(board2, move) == -900, "SEE: king recaptures freely = -900");
	}
	{
		// F: 成り駒の価値 — と金(540)を取る / と金で取る
		BoardState board(Turn::SENTE);
		board.set_cell({ 4, 4 }, P, Turn::GOTE, true); // と金
		board.set_cell({ 4, 8 }, R, Turn::SENTE, false);
		Move move(4, 8, 4, 4, R, false, false, true);
		check(MoveGenerator::see(board, move) == 540, "SEE: promoted pawn victim = +540");

		// 守られた歩(90)をと金(540)で取ると 90-540=-450
		BoardState board2(Turn::SENTE);
		board2.set_cell({ 4, 4 }, P, Turn::GOTE, false);
		board2.set_cell({ 4, 3 }, P, Turn::GOTE, false); // 守りの歩
		board2.set_cell({ 4, 5 }, P, Turn::SENTE, true); // と金で取る（金の利きで5五に届く）
		Move move2(4, 5, 4, 4, P, false, false, true);
		check(MoveGenerator::see(board2, move2) == -450, "SEE: tokin attacker loses 540 = -450");
	}
}

void run_quiescence_tests() {
	{
		// 対照: 窓が広ければタダ取りは探索される（枝刈りが過剰でないこと）
		BoardState board(Turn::SENTE);
		board.set_cell({ 4, 4 }, P, Turn::GOTE, false);
		board.set_cell({ 4, 8 }, R, Turn::SENTE, false);
		AIPlayer ai;
		AIPlayerTestAccess ai_t{ ai };
		uint64_t nodes = 0;
		int score = ai_t.quiescence_search(board, -AIPlayer::INFINITE_SCORE, AIPlayer::INFINITE_SCORE, 0, nodes);
		check(nodes > 1, "qsearch explores free capture (control)");

		// Delta pruning: 取る駒+マージンを足しても alpha に届かなければ読まない
		int stand_pat = board.get_score();
		nodes = 0;
		int high_alpha = stand_pat + 90 + 540 + 1; // victim(90) + DELTA_MARGIN(540) でも届かない
		score = ai_t.quiescence_search(board, high_alpha, high_alpha + 1, 0, nodes);
		check(nodes == 1, "delta pruning skips hopeless capture");
		check(score == stand_pat + 90 + 540, "delta pruning returns the optimistic value of the skipped capture");
	}
	{
		// SEE pruning: 負け取りしかない局面では手を読まず stand_pat を返す
		BoardState board(Turn::SENTE);
		board.set_cell({ 4, 4 }, P, Turn::GOTE, false);
		board.set_cell({ 4, 3 }, P, Turn::GOTE, false); // 守りの歩
		board.set_cell({ 4, 8 }, R, Turn::SENTE, false);
		AIPlayer ai;
		AIPlayerTestAccess ai_t{ ai };
		uint64_t nodes = 0;
		int score = ai_t.quiescence_search(board, -AIPlayer::INFINITE_SCORE, AIPlayer::INFINITE_SCORE, 0, nodes);
		check(nodes == 1, "SEE pruning skips losing capture");
		check(score == board.get_score(), "SEE pruning returns stand_pat");
	}
	{
		// qs ply 上限: 上限到達ノードはタダ取りがあっても stand_pat を返す
		BoardState board(Turn::SENTE);
		board.set_cell({ 4, 4 }, P, Turn::GOTE, false);
		board.set_cell({ 4, 8 }, R, Turn::SENTE, false);
		AIPlayer ai;
		AIPlayerTestAccess ai_t{ ai };
		uint64_t nodes = 0;
		int score = ai_t.quiescence_search(board, -AIPlayer::INFINITE_SCORE, AIPlayer::INFINITE_SCORE, 0, nodes, AIPlayerTestAccess::QS_PLY_LIMIT);
		check(nodes == 1, "QS ply limit stops search");
		check(score == board.get_score(), "QS ply limit returns stand_pat");
	}
	{
		// 後手番: タダ取りを読むと手番側から見た評価値が stand_pat を上回る
		BoardState board(Turn::GOTE);
		board.set_cell({ 4, 4 }, P, Turn::SENTE, false);
		board.set_cell({ 4, 0 }, R, Turn::GOTE, false);
		AIPlayer ai;
		AIPlayerTestAccess ai_t{ ai };
		uint64_t nodes = 0;
		int score = ai_t.quiescence_search(board, -AIPlayer::INFINITE_SCORE, AIPlayer::INFINITE_SCORE, 0, nodes);
		check(nodes > 1, "qsearch explores free capture for GOTE");
		check(score > -board.get_score(), "qsearch returns side-to-move score for GOTE");
	}
}

void setup_capture_choice_position(BoardState &board) {
	board.set_cell({ 0, 0 }, K, Turn::GOTE, false);
	board.set_cell({ 8, 8 }, K, Turn::SENTE, false);
	board.set_cell({ 6, 4 }, G, Turn::GOTE, false);
	board.set_cell({ 6, 7 }, R, Turn::SENTE, false);
	board.set_cell({ 2, 4 }, B, Turn::GOTE, false);
	board.set_cell({ 2, 3 }, P, Turn::GOTE, false);
	board.set_cell({ 2, 7 }, R, Turn::SENTE, false);
	board.set_cell({ 4, 2 }, S, Turn::GOTE, false);
	board.set_cell({ 4, 1 }, P, Turn::GOTE, false);
	board.set_cell({ 4, 7 }, L, Turn::SENTE, false);
	board.set_cell({ 0, 4 }, N, Turn::GOTE, false);
	board.set_cell({ 0, 5 }, S, Turn::SENTE, false);
}

void run_pvs_tests() {
	// 期待値はPVS導入前の実装で求めた値
	BoardState board(Turn::SENTE);
	setup_capture_choice_position(board);
	AIPlayer ai;
	AIPlayerTestAccess ai_t{ ai };
	bool timeout = false;
	uint64_t nodes = 0;
	int score = ai_t.alpha_beta(board, 2, 0, -AIPlayer::INFINITE_SCORE, AIPlayer::INFINITE_SCORE, UINT64_MAX, timeout, nodes);
	check(score == 1411, "pvs: depth-2 score matches full-window search (1411)");
	std::optional<TTData> entry = ai_t.tt().probe(board.get_zobrist_hash());
	check(entry.has_value() && entry->move == pack_move(Move(6, 7, 6, 4, R, false, false, true)),
			"pvs: depth-2 best move is rook takes gold");
}

void run_fail_low_tt_move_tests() {
	// どの手もαを超えないノードでは置換表の最善手を残す
	BoardState board(Turn::SENTE);
	setup_capture_choice_position(board);
	AIPlayer ai;
	AIPlayerTestAccess ai_t{ ai };
	const Move king_move(8, 8, 7, 7, K, false, false, false);
	uint64_t hash = board.get_zobrist_hash();
	ai_t.tt().store(hash, 0, 1, TTFlag::EXACT, pack_move(king_move));

	bool timeout = false;
	uint64_t nodes = 0;
	constexpr int HIGH_ALPHA = 100000;
	static_cast<void>(ai_t.alpha_beta(board, 2, 0, HIGH_ALPHA, HIGH_ALPHA + 1, UINT64_MAX, timeout, nodes));
	std::optional<TTData> entry = ai_t.tt().probe(hash);
	check(entry.has_value() && entry->flag == TTFlag::UPPER_BOUND, "tt move: fail-low node is stored as upper bound");
	check(entry.has_value() && entry->move == pack_move(king_move), "tt move: fail-low node keeps the previous best move");

	// A move not in the move list is not kept
	AIPlayer other;
	AIPlayerTestAccess other_t{ other };
	constexpr uint16_t NOT_A_LEGAL_MOVE = 0x7fff;
	other_t.tt().store(hash, 0, 1, TTFlag::EXACT, NOT_A_LEGAL_MOVE);
	timeout = false;
	nodes = 0;
	static_cast<void>(other_t.alpha_beta(board, 2, 0, HIGH_ALPHA, HIGH_ALPHA + 1, UINT64_MAX, timeout, nodes));
	Shogi::MoveList move_list;
	MoveGenerator::get_legal_moves(board, move_list);
	std::optional<TTData> other_entry = other_t.tt().probe(hash);
	check(other_entry.has_value() &&
					std::ranges::any_of(move_list, [&other_entry](const Move &move) { return pack_move(move) == other_entry->move; }),
			"tt move: a move not in the move list is not kept on fail-low");
}

bool is_board_move(const ScoredMove &scored, int from_col, int from_row, int to_col, int to_row) {
	const Move &move = scored.move;
	return !move.is_drop && move.from_col == from_col && move.from_row == from_row && move.to_col == to_col && move.to_row == to_row;
}

void run_hint_score_tests() {
	// Expected values come from the implementation after reverse futility pruning
	BoardState board(Turn::SENTE);
	setup_capture_choice_position(board);
	AIPlayer ai;
	ai.set_max_depth(3);
	ai.set_time_limit_usec(60'000'000);
	std::vector<ScoredMove> moves = ai.search_top_moves(board, 3);
	check(moves.size() == 3, "hint: returns 3 moves");
	if (moves.size() != 3) {
		return;
	}
	check(is_board_move(moves[0], 6, 7, 6, 4) && moves[0].score == 1871, "hint: 1st is rook takes gold (1871)");
	check(is_board_move(moves[1], 0, 5, 0, 4) && moves[1].score == 1320, "hint: 2nd is silver takes knight (1320)");
	check(is_board_move(moves[2], 2, 7, 2, 4) && moves[2].score == 1306, "hint: 3rd is rook takes bishop (1306)");
}

const char *SENTE_AHEAD_SFEN = "4k4/9/9/9/9/9/9/9/4K4 b RBGSNLP 1";
const char *SENTE_AHEAD_IN_CHECK_SFEN = "4k4/9/9/9/9/9/9/4r4/4K4 b RBGSNLP 1";
const char *GOTE_AHEAD_SFEN = "4k4/9/9/9/9/9/9/9/4K4 w rbgsnlp 1";

struct AlphaBetaResult {
	int score = 0;
	uint64_t nodes = 0;
	int static_eval = 0;
};

AlphaBetaResult run_alpha_beta(const char *sfen, int depth, int alpha, int beta) {
	std::optional<BoardState> board = BoardState::from_sfen(sfen);
	check(board.has_value(), "pruning: test position parses");
	if (!board.has_value()) {
		return {};
	}
	AIPlayer ai;
	AIPlayerTestAccess ai_t{ ai };
	AlphaBetaResult result;
	int score = board->get_score();
	result.static_eval = board->get_turn_to_move() == Turn::SENTE ? score : -score;
	bool timeout = false;
	result.score = ai_t.alpha_beta(*board, depth, 0, alpha, beta, UINT64_MAX, timeout, result.nodes);
	return result;
}

void run_reverse_futility_tests() {
	using T = AIPlayerTestAccess;
	check(T::can_prune_node(false, 99, 100), "pruning: zero window node can be pruned");
	check(!T::can_prune_node(true, 99, 100), "pruning: not in check");
	check(!T::can_prune_node(false, 99, 101), "pruning: not in PV nodes");
	check(!T::can_prune_node(false, AIPlayer::MATE_BOUND - 1, AIPlayer::MATE_BOUND), "pruning: not near a winning mate score");
	check(!T::can_prune_node(false, -AIPlayer::MATE_BOUND - 1, -AIPlayer::MATE_BOUND), "pruning: not near a losing mate score");

	check(T::reverse_futility_cuts(3, 100 + 3 * T::RFP_MARGIN, 100), "rfp: cuts at the margin boundary");
	check(!T::reverse_futility_cuts(3, 99 + 3 * T::RFP_MARGIN, 100), "rfp: does not cut just below the margin");
	check(!T::reverse_futility_cuts(T::RFP_MAX_DEPTH + 1, 100000, 0), "rfp: not above the max depth");

	AlphaBetaResult cut = run_alpha_beta(SENTE_AHEAD_SFEN, 2, 99, 100);
	check(cut.score == 100 && cut.nodes == 1, "rfp: returns beta without searching children");

	AlphaBetaResult gote = run_alpha_beta(GOTE_AHEAD_SFEN, 2, 99, 100);
	check(gote.score == 100 && gote.nodes == 1, "rfp: works for gote to move");

	AlphaBetaResult pv = run_alpha_beta(SENTE_AHEAD_SFEN, 2, 99, 101);
	check(pv.nodes > 1, "rfp: does not cut PV nodes");

	AlphaBetaResult in_check = run_alpha_beta(SENTE_AHEAD_IN_CHECK_SFEN, 2, 99, 100);
	check(in_check.nodes > 1, "rfp: does not cut when in check");

	AlphaBetaResult deep = run_alpha_beta(SENTE_AHEAD_SFEN, T::RFP_MAX_DEPTH + 1, 99, 100);
	check(deep.nodes > 1, "rfp: does not cut above the max depth");

	int s = cut.static_eval;
	AlphaBetaResult boundary = run_alpha_beta(SENTE_AHEAD_SFEN, 2, s - 2 * T::RFP_MARGIN - 1, s - 2 * T::RFP_MARGIN);
	check(boundary.nodes == 1, "rfp: cuts at the margin boundary in search");
	AlphaBetaResult below = run_alpha_beta(SENTE_AHEAD_SFEN, 2, s - 2 * T::RFP_MARGIN, s - 2 * T::RFP_MARGIN + 1);
	check(below.nodes > 1, "rfp: does not cut just below the margin in search");

	AlphaBetaResult mate_window = run_alpha_beta(SENTE_AHEAD_SFEN, 2, -AIPlayer::MATE_BOUND - 1, -AIPlayer::MATE_BOUND);
	check(mate_window.nodes > 1, "rfp: does not cut when the window is a mate score");
}

const char *SENTE_BEHIND_SFEN = "4k4/9/9/9/9/9/PPPPPPPPP/9/4K4 b 2r2b4g4s 1";

void run_quiet_move_prune_tests() {
	using T = AIPlayerTestAccess;
	const Move quiet(6, 6, 6, 5, P, false, false, false);
	const Move capture(6, 6, 6, 5, P, false, false, true);
	const Move promotion(6, 3, 6, 2, P, true, false, false);
	check(T::is_prunable_quiet_move(quiet, false, false, 1), "quiet prune: quiet move after the first");
	check(!T::is_prunable_quiet_move(quiet, false, false, 0), "quiet prune: never the first move");
	check(!T::is_prunable_quiet_move(capture, false, false, 1), "quiet prune: not captures");
	check(!T::is_prunable_quiet_move(promotion, false, false, 1), "quiet prune: not promotions");
	check(!T::is_prunable_quiet_move(quiet, true, false, 1), "quiet prune: not checks");
	check(!T::is_prunable_quiet_move(quiet, false, true, 1), "quiet prune: not killers");
}

void run_futility_tests() {
	using T = AIPlayerTestAccess;
	check(T::futility_prunes(1, 0, T::FUTILITY_MARGIN_1), "futility: prunes at the margin boundary");
	check(!T::futility_prunes(1, 1, T::FUTILITY_MARGIN_1), "futility: does not prune just below the margin");
	check(!T::futility_prunes(T::FUTILITY_MAX_DEPTH + 1, -100000, 0), "futility: not above the max depth");

	int s = run_alpha_beta(SENTE_BEHIND_SFEN, 1, 0, 1).static_eval;
	int alpha = s + T::FUTILITY_MARGIN_1;
	AlphaBetaResult pruned = run_alpha_beta(SENTE_BEHIND_SFEN, 1, alpha, alpha + 1);
	AlphaBetaResult pv = run_alpha_beta(SENTE_BEHIND_SFEN, 1, alpha, alpha + 2);
	AlphaBetaResult below = run_alpha_beta(SENTE_BEHIND_SFEN, 1, alpha - 1, alpha);
	check(pruned.nodes < pv.nodes, "futility: zero window search skips quiet moves");
	check(pruned.nodes < below.nodes, "futility: no pruning just below the margin");
}

void run_late_move_pruning_tests() {
	using T = AIPlayerTestAccess;
	check(!T::late_move_prunes(1, 5) && T::late_move_prunes(1, 6), "lmp: depth 1 prunes from the 7th move");
	check(!T::late_move_prunes(2, 11) && T::late_move_prunes(2, 12), "lmp: depth 2 prunes from the 13th move");
	check(!T::late_move_prunes(3, 21) && T::late_move_prunes(3, 22), "lmp: depth 3 prunes from the 23rd move");
	check(!T::late_move_prunes(T::LMP_MAX_DEPTH + 1, 1000), "lmp: not above the max depth");

	// Futility does not prune here because the static eval is within its margin of alpha
	int s = run_alpha_beta(SENTE_BEHIND_SFEN, 2, 0, 1).static_eval;
	int alpha = s + 300;
	AlphaBetaResult pruned = run_alpha_beta(SENTE_BEHIND_SFEN, 2, alpha, alpha + 1);
	AlphaBetaResult pv = run_alpha_beta(SENTE_BEHIND_SFEN, 2, alpha, alpha + 2);
	check(pruned.nodes < pv.nodes, "lmp: zero window search skips late quiet moves");
}

const char *QUIET_DEFENSE_SFEN = "4k4/9/4p4/g8/R8/9/7s1/9/8K b g 1";

void run_pruning_false_mate_tests() {
	int s = run_alpha_beta(QUIET_DEFENSE_SFEN, 2, 0, 1).static_eval;
	int alpha = s + AIPlayerTestAccess::FUTILITY_MARGIN_2;
	AlphaBetaResult pruned = run_alpha_beta(QUIET_DEFENSE_SFEN, 2, alpha, alpha + 1);
	check(pruned.score > -AIPlayer::MATE_BOUND, "pruning: no false mate when only quiet moves stop the mate");
}

void run_pruning_mate_regression_tests() {
	BoardState board(Turn::SENTE);
	setup_initial_position(board);
	if (!replay(board, 59, KIFU2)) {
		check(false, "pruning: replay kif2-59");
		return;
	}
	AIPlayer ai;
	ai.set_max_depth(6);
	ai.set_time_limit_usec(60'000'000);
	std::vector<ScoredMove> moves = ai.search_top_moves(board, 1);
	check(!moves.empty() && moves[0].score <= -AIPlayer::MATE_BOUND, "pruning: still sees being mated at kif2-59 depth 6");
}

void run_lmr_and_extension_tests() {
	// LMR回帰: 深さ5の通常探索が kif2-60 の詰みを見逃さないこと
	{
		BoardState mate_board(Turn::SENTE);
		setup_initial_position(mate_board);
		if (replay(mate_board, 60, KIFU2)) {
			AIPlayer mate_ai;
			AIPlayerTestAccess mate_ai_t{ mate_ai };
			bool timeout = false;
			uint64_t nodes = 0;
			int score = mate_ai_t.alpha_beta(mate_board, 5, 0, -AIPlayer::INFINITE_SCORE, AIPlayer::INFINITE_SCORE, UINT64_MAX,
					timeout, nodes);
			check(score >= AIPlayer::MATE_BOUND, "depth-5 search finds mate at kif2-60");
		} else {
			check(false, "replay kif2-60 for LMR regression");
		}
	}

	// 王手延長: 延長なしでは見逃す kif2-60 の詰みを深さ4で発見できること
	// （手順が9手あるため深さ3では延長を足しても届かず、深さ4にした）
	{
		BoardState ext_board(Turn::SENTE);
		setup_initial_position(ext_board);
		if (replay(ext_board, 60, KIFU2)) {
			AIPlayer ext_ai;
			AIPlayerTestAccess ext_ai_t{ ext_ai };
			bool timeout = false;
			uint64_t nodes = 0;
			int score = ext_ai_t.alpha_beta(ext_board, 4, 0, -AIPlayer::INFINITE_SCORE, AIPlayer::INFINITE_SCORE, UINT64_MAX,
					timeout, nodes);
			check(score >= AIPlayer::MATE_BOUND, "check extension finds mate at kif2-60 at depth 4");
		} else {
			check(false, "replay kif2-60 for check extension");
		}
	}
}

void run_path_repetition_tests() {
	{
		AIPlayer ai;
		AIPlayerTestAccess ai_t{ ai };
		std::ranges::fill(ai_t.path_hashes(), 0);
		std::ranges::fill(ai_t.path_in_check(), false);

		const uint64_t H = 0xABCDEF12345678ULL;

		// 反復なし: 一致する祖先がない
		ai_t.path_hashes()[0] = 0x1111ULL;
		check(!ai_t.detect_path_repetition(2, H).has_value(),
				"rep: no repetition when no ancestor matches");

		// 単純反復（王手なし）→ 引き分け0。経路 ply0(=H) → ply1 → ply2(=H,現在)
		ai_t.path_hashes()[0] = H;
		ai_t.path_in_check()[0] = false;
		ai_t.path_in_check()[1] = false;
		ai_t.path_in_check()[2] = false; // 呼び出し側が現在の ply の王手状態を書き込む想定
		auto r1 = ai_t.detect_path_repetition(2, H);
		check(r1.has_value() && r1.value() == 0, "rep: simple repetition scores draw 0");

		// 手番側(S=SENTE)の連続王手 → S の負け。相手(O)の手番の ply1 が王手を受けている
		ai_t.path_in_check()[1] = true;
		ai_t.path_in_check()[2] = false;
		auto r2 = ai_t.detect_path_repetition(2, H);
		check(r2.has_value() && r2.value() == -(AIPlayer::MATE_SCORE - 2),
				"rep: STM perpetual check makes SENTE lose");

		// 相手(O)の連続王手 → S 勝ち。S の手番 ply2(現在) が王手を受けている
		ai_t.path_in_check()[1] = false;
		ai_t.path_in_check()[2] = true;
		auto r3 = ai_t.detect_path_repetition(2, H);
		check(r3.has_value() && r3.value() == (AIPlayer::MATE_SCORE - 2),
				"rep: opponent perpetual check makes SENTE win");

		// 王手混在（連続でない）→ 引き分け0。4-ply サイクル [0,4]
		std::ranges::fill(ai_t.path_hashes(), 0);
		std::ranges::fill(ai_t.path_in_check(), false);
		ai_t.path_hashes()[0] = H;
		ai_t.path_in_check()[1] = true; // O 手番で王手
		ai_t.path_in_check()[3] = false; // 次の O 手番では王手なし → S連続王手不成立
		ai_t.path_in_check()[2] = false;
		ai_t.path_in_check()[4] = false; // S 手番は王手なし → O連続王手も不成立
		auto r4 = ai_t.detect_path_repetition(4, H);
		check(r4.has_value() && r4.value() == 0, "rep: non-consecutive checks score draw 0");

		// ply 0 / 1 は手番が同じ祖先がないため必ず nullopt
		check(!ai_t.detect_path_repetition(0, H).has_value(),
				"rep: ply 0 has no ancestor (nullopt)");
		check(!ai_t.detect_path_repetition(1, H).has_value(),
				"rep: ply 1 has no ancestor (nullopt)");

		// 相互連続王手（両者とも王手）→ turn_perpetual 優先で手番側 S が負け
		std::ranges::fill(ai_t.path_hashes(), 0);
		std::ranges::fill(ai_t.path_in_check(), false);
		ai_t.path_hashes()[0] = H;
		ai_t.path_in_check()[1] = true; // O の手番で王手
		ai_t.path_in_check()[2] = true; // S(現在) も王手されている
		auto rm = ai_t.detect_path_repetition(2, H);
		check(rm.has_value() && rm.value() == -(AIPlayer::MATE_SCORE - 2),
				"rep: mutual perpetual check, STM (SENTE) loses by precedence");
	}
}

void run_history_repetition_tests() {
	{
		AIPlayer ai;
		AIPlayerTestAccess ai_t{ ai };
		std::ranges::fill(ai_t.path_hashes(), 0);
		std::ranges::fill(ai_t.path_in_check(), false);

		// (1) 履歴の局面にルートが戻る → 引き分け0
		const uint64_t X = 0xAAAA1111ULL, Y = 0xBBBB2222ULL;
		ai.set_game_history({ X, Y }, { false, false });
		ai_t.path_hashes()[0] = X; // ルート(現局面, 仮想idx2, S手番)が履歴idx0(X)と一致
		ai_t.path_in_check()[0] = false;
		auto rh1 = ai_t.detect_path_repetition(0, X);
		check(rh1.has_value() && rh1.value() == 0, "rep: history-boundary repetition scores draw 0");

		// (2) 連続王手のサイクルが履歴境界をまたぐ（王手フラグが履歴側）→ 手番側S負け
		const uint64_t H = 0xCCCC3333ULL;
		ai.set_game_history({ H, 0xDDDDULL }, { false, true }); // 履歴idx1(O手番)が王手
		ai_t.path_hashes()[0] = H;
		ai_t.path_in_check()[0] = false; // 現在(S)は王手を受けていない
		auto rh2 = ai_t.detect_path_repetition(0, H);
		check(rh2.has_value() && rh2.value() == -(AIPlayer::MATE_SCORE - 0),
				"rep: perpetual across history boundary makes SENTE lose");

		// (3) サイクルが履歴(一致)＋経路(王手フラグ)をまたぐ → 手番側S負け
		const uint64_t Z = 0xEEEE4444ULL;
		ai.set_game_history({ Z }, { false }); // 履歴idx0=Z(S手番)
		std::ranges::fill(ai_t.path_hashes(), 0);
		std::ranges::fill(ai_t.path_in_check(), false);
		ai_t.path_hashes()[0] = 0x9999ULL; // ルート(仮想idx1, O手番)
		ai_t.path_in_check()[0] = true; // idx1(O手番)が王手を受けている = S が王手をかけている
		ai_t.path_hashes()[1] = Z; // ply1(仮想idx2, S手番)が履歴Zと一致
		ai_t.path_in_check()[1] = false; // 現在(S)は王手なし
		auto rh3 = ai_t.detect_path_repetition(1, Z);
		check(rh3.has_value() && rh3.value() == -(AIPlayer::MATE_SCORE - 1),
				"rep: perpetual with cycle crossing boundary (path+history)");
	}
}

void run_alpha_beta_repetition_tests() {
	{
		BoardState board(Turn::SENTE);
		board.set_cell({ 4, 4 }, K, Turn::GOTE, false);
		board.set_cell({ 4, 0 }, K, Turn::SENTE, false);
		AIPlayer ai;
		AIPlayerTestAccess ai_t{ ai };
		ai_t.path_hashes()[0] = board.get_zobrist_hash(); // 現局面を ply0 の祖先として登録
		ai_t.path_in_check()[0] = false;
		bool timeout = false;
		uint64_t nodes = 0;
		// ply=2 で現局面に入る = ply0 と同一局面 → 反復を検出してすぐ 0 を返す（評価関数を呼ばない）
		int score = ai_t.alpha_beta(board, 3, 2, -AIPlayer::INFINITE_SCORE, AIPlayer::INFINITE_SCORE, UINT64_MAX, timeout, nodes);
		check(score == 0, "rep: alpha_beta returns draw 0 on path repetition");
		check(nodes == 1, "rep: alpha_beta stops immediately on repetition");
	}

	// TT に非引き分けエントリがあっても反復検出が優先される
	{
		BoardState board(Turn::SENTE);
		board.set_cell({ 4, 4 }, K, Turn::GOTE, false);
		board.set_cell({ 4, 0 }, K, Turn::SENTE, false);
		AIPlayer ai;
		AIPlayerTestAccess ai_t{ ai };
		uint64_t hash = board.get_zobrist_hash();
		// この局面に対する非引き分けの TT エントリを仕込む（別の経路で保存されたエントリを模したもの）
		ai_t.tt().store(hash, 5000, 10, TTFlag::EXACT, 0);
		// 現局面を経路の起点として登録
		ai_t.path_hashes()[0] = hash;
		ai_t.path_in_check()[0] = false;
		bool timeout = false;
		uint64_t nodes = 0;
		// ply=2 で現局面に戻る = 反復。検出は TT probe より前なので 5000 ではなく 0 を返す
		int score = ai_t.alpha_beta(board, 1, 2, -AIPlayer::INFINITE_SCORE, AIPlayer::INFINITE_SCORE, UINT64_MAX, timeout, nodes);
		check(score == 0, "rep: detection precedes TT probe (draw not TT score)");
		check(nodes == 1, "rep: detection-before-TT stops immediately");
	}
}

void run_gote_search_tests() {
	// 頭金で詰んでいる後手番: 手番側の負け
	{
		BoardState board(Turn::GOTE);
		board.set_cell({ 0, 0 }, K, Turn::GOTE, false);
		board.set_cell({ 0, 1 }, G, Turn::SENTE, false);
		board.set_cell({ 0, 2 }, P, Turn::SENTE, false);
		AIPlayer ai;
		AIPlayerTestAccess ai_t{ ai };
		bool timeout = false;
		uint64_t nodes = 0;
		int score = ai_t.alpha_beta(board, 1, 0, -AIPlayer::INFINITE_SCORE, AIPlayer::INFINITE_SCORE, UINT64_MAX, timeout, nodes);
		check(score == -AIPlayer::MATE_SCORE, "GOTE mated: alpha_beta returns loss for side to move");
	}

	// 後手番の千日手（後手が連続王手をかけている）: 手番側の負け
	{
		BoardState board(Turn::GOTE);
		board.set_cell({ 4, 4 }, K, Turn::GOTE, false);
		board.set_cell({ 4, 0 }, K, Turn::SENTE, false);
		AIPlayer ai;
		AIPlayerTestAccess ai_t{ ai };
		std::ranges::fill(ai_t.path_hashes(), 0);
		std::ranges::fill(ai_t.path_in_check(), false);
		ai_t.path_hashes()[0] = board.get_zobrist_hash();
		ai_t.path_in_check()[1] = true; // ply1（先手の手番）で王手を受けている = 後手が王手をかけ続けている
		bool timeout = false;
		uint64_t nodes = 0;
		int score = ai_t.alpha_beta(board, 3, 2, -AIPlayer::INFINITE_SCORE, AIPlayer::INFINITE_SCORE, UINT64_MAX, timeout, nodes);
		check(score == -(AIPlayer::MATE_SCORE - 2), "GOTE perpetual check: alpha_beta returns loss for side to move");
	}

	// 後手番のルート探索: 飛車得の後手から見た評価値と勝率で候補手を良い順に返す
	{
		std::optional<BoardState> board = BoardState::from_sfen("lnsgkgsnl/1r5b1/ppppppppp/9/9/9/PPPPPPPPP/1B7/LNSGKGSNL w - 1");
		check(board.has_value(), "GOTE root: test position parses");
		if (board.has_value()) {
			AIPlayer ai;
			ai.set_time_limit_usec(300000);
			std::vector<ScoredMove> moves = ai.search_top_moves(*board, 3);
			bool ordered = true;
			for (size_t i = 1; i < moves.size(); ++i) {
				ordered = ordered && moves[i - 1].score >= moves[i].score;
			}
			check(!moves.empty() && moves[0].score > 0 && moves[0].win_rate > 0.5,
					"GOTE root: score and win rate are from side to move");
			check(ordered, "GOTE root: best move first");
		}
	}
}

void run_dfpn_repetition_tests() {
	{
		BoardState board(Turn::SENTE);
		board.set_cell({ 4, 4 }, K, Turn::GOTE, false);
		board.set_cell({ 4, 0 }, K, Turn::SENTE, false);
		uint64_t hash = board.get_zobrist_hash();

		// 反復: 現局面が経路上の祖先にすでに現れている → 不詰み(pn=∞,dn=0)を返し、表に保存しない
		AIPlayer ai;
		AIPlayerTestAccess ai_t{ ai };
		ai_t.dfpn_path().push_back(hash);
		int pn = 1, dn = 1;
		uint64_t nodes = 0;
		ai_t.dfpn_search(board, board.get_turn_to_move(), AIPlayerTestAccess::INFINITY_PN, AIPlayerTestAccess::INFINITY_PN, pn, dn, 5, nodes,
				100000);
		check(pn == static_cast<int>(AIPlayerTestAccess::INFINITY_PN) && dn == 0, "dfpn: path repetition returns unmate (pn=inf, dn=0)");
		check(!ai_t.dfpn_table().contains(hash), "dfpn: repetition node not cached (GHI guard)");

		// 対照: 経路に祖先が無ければ反復扱いしない（KvK は王手になる手がなく不詰み、かつ表に保存される）
		AIPlayer ai2;
		AIPlayerTestAccess ai2_t{ ai2 };
		int pn2 = 1, dn2 = 1;
		uint64_t nodes2 = 0;
		ai2_t.dfpn_search(board, board.get_turn_to_move(), AIPlayerTestAccess::INFINITY_PN, AIPlayerTestAccess::INFINITY_PN, pn2, dn2, 5, nodes2,
				100000);
		check(pn2 == static_cast<int>(AIPlayerTestAccess::INFINITY_PN) && dn2 == 0, "dfpn: KvK leaf is unmate");
		check(ai2_t.dfpn_table().contains(hash), "dfpn: non-repetition leaf IS cached");
	}

	// Self-play game lost by perpetual check: GOTE kept checking with 3f1d and 1d3f
	{
		std::optional<Usi::Position> position = Usi::parse_position(
				"sfen l2g3+N1/5g2+B/p1nkpp2p/1p3sP2/6l1K/2pn2+b1S/PPNlPP2P/1S7/L1G2G3 w 2R2Ps4p 108 moves 3f1d 1e2f 1d3f 2f1e");
		check(position.has_value(), "dfpn history: test position parses");
		if (position.has_value()) {
			AIPlayer ai;
			AIPlayerTestAccess ai_t{ ai };
			ai.set_game_history(position->history_hashes, position->history_in_checks);
			std::optional<Move> mate = ai_t.find_mate(position->board, 21);
			check(mate.has_value(), "dfpn history: mate is found when the root is in game history");
			bool repeats = false;
			if (mate.has_value()) {
				Shogi::UndoInfo undo = position->board.apply_move(*mate);
				repeats = std::ranges::contains(position->history_hashes, position->board.get_zobrist_hash());
				position->board.undo_move(undo);
			}
			check(!repeats, "dfpn history: mate move does not return to a position in game history");
		}
	}
}

} // namespace

void run_search_tests() {
	run_format_percent_tests();
	run_win_probability_tests();
	run_logger_tests();
	run_max_depth_tests();
	run_stop_tests();
	run_depth_callback_tests();
	run_tt_persistence_tests();
	run_move_ordering_tests();
	run_sort_moves_tests();
	run_see_tests();
	run_quiescence_tests();
	run_pvs_tests();
	run_fail_low_tt_move_tests();
	run_hint_score_tests();
	run_reverse_futility_tests();
	run_quiet_move_prune_tests();
	run_futility_tests();
	run_late_move_pruning_tests();
	run_pruning_false_mate_tests();
	run_pruning_mate_regression_tests();
	run_lmr_and_extension_tests();
	run_path_repetition_tests();
	run_history_repetition_tests();
	run_alpha_beta_repetition_tests();
	run_gote_search_tests();
	run_dfpn_repetition_tests();
}
