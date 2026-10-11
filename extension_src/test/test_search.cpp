#include "move_generator.hpp"
#include "test_support.hpp"
#include <algorithm>
#include <regex>
#include <string>

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
	const std::regex total_line(R"(Total nodes searched: \d+, TT size: \d+)");
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

void run_format_percent_tests() {
	check(AIPlayerTestAccess::format_percent(50.0) == "50", "format_percent: 50.0 drops trailing .0");
	check(AIPlayerTestAccess::format_percent(49.96) == "50", "format_percent: rounds up to integer without .0");
	check(AIPlayerTestAccess::format_percent(53.24) == "53.2", "format_percent: keeps one decimal");
	check(AIPlayerTestAccess::format_percent(100.0) == "100", "format_percent: 100.0 keeps integer zeros");
	check(AIPlayerTestAccess::format_percent(0.0) == "0", "format_percent: 0.0 becomes 0");
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
		check(score == stand_pat, "delta pruning returns stand_pat");
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
		ai_t.transposition_table()[hash] = TTEntry{ 5000, 10, TTFlag::EXACT, Move{} };
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
}

} // namespace

void run_search_tests() {
	run_format_percent_tests();
	run_logger_tests();
	run_move_ordering_tests();
	run_see_tests();
	run_quiescence_tests();
	run_lmr_and_extension_tests();
	run_path_repetition_tests();
	run_history_repetition_tests();
	run_alpha_beta_repetition_tests();
	run_gote_search_tests();
	run_dfpn_repetition_tests();
}
