#include "move_generator.hpp"
#include "test_support.hpp"
#include <algorithm>
#include <span>
#include <vector>

namespace {

const char *START_SFEN_B = "lnsgkgsnl/1r5b1/ppppppppp/9/9/9/PPPPPPPPP/1B5R1/LNSGKGSNL b - 1";
const char *START_SFEN_W = "lnsgkgsnl/1r5b1/ppppppppp/9/9/9/PPPPPPPPP/1B5R1/LNSGKGSNL w - 1";

void run_from_sfen_tests() {
	check(!BoardState::from_sfen("xyz b - 1").has_value(), "from_sfen: invalid piece char is rejected");
	check(!BoardState::from_sfen("4k4/9/9/9/9/9/9/9/4K4 b -").has_value(), "from_sfen: missing move number is rejected");

	std::optional<BoardState> sente = BoardState::from_sfen(START_SFEN_B);
	std::optional<BoardState> gote = BoardState::from_sfen(START_SFEN_W);
	check(sente.has_value() && sente->get_turn_to_move() == Turn::SENTE, "from_sfen: 'b' sets SENTE to move");
	check(gote.has_value() && gote->get_turn_to_move() == Turn::GOTE, "from_sfen: 'w' sets GOTE to move");
	check(sente.has_value() && gote.has_value() && sente->get_zobrist_hash() != gote->get_zobrist_hash(),
			"from_sfen: side to move changes hash");

	std::optional<BoardState> hand = BoardState::from_sfen("4k4/9/9/9/9/9/9/9/4K4 b 18P 1");
	check(hand.has_value() && hand->get_hand_count(Turn::SENTE, P) == 18, "from_sfen: multi-digit hand count");
}

void run_set_hand_count_tests() {
	{
		BoardState board(Turn::SENTE);
		board.set_cell({ 4, 8 }, K, Turn::SENTE, false);
		board.set_cell({ 4, 0 }, K, Turn::GOTE, false);
		check(board.get_hand_count(Turn::SENTE, P) == 0, "set_hand_count: initial hand is empty");

		uint64_t hash0 = board.get_zobrist_hash();
		board.set_hand_count(Turn::SENTE, P, 1);
		check(board.get_hand_count(Turn::SENTE, P) == 1, "set_hand_count: count is set");
		check(board.get_zobrist_hash() != hash0, "set_hand_count: zobrist hash changes");

		uint64_t hash1 = board.get_zobrist_hash();
		Move drop(0, 0, 4, 4, P, false, true, false);
		Shogi::UndoInfo undo = board.apply_move(drop);
		check(board.get_hand_count(Turn::SENTE, P) == 0, "set_hand_count: drop consumes hand piece");
		board.undo_move(undo);
		check(board.get_zobrist_hash() == hash1 && board.get_hand_count(Turn::SENTE, P) == 1,
				"set_hand_count: drop round-trips with apply_move/undo_move");

		board.set_hand_count(Turn::SENTE, P, 0);
		check(board.get_hand_count(Turn::SENTE, P) == 0 && board.get_zobrist_hash() == hash0,
				"set_hand_count: resetting to 0 restores hash");
	}
}

void run_incremental_hash_tests() {
	{
		std::optional<BoardState> board = BoardState::from_sfen("4k4/9/9/9/9/9/9/9/4K4 b P 1");
		std::optional<BoardState> expected = BoardState::from_sfen("4k4/9/9/9/4P4/9/9/9/4K4 w - 2");
		check(board.has_value() && expected.has_value(), "incremental hash: drop positions parse");
		if (board.has_value() && expected.has_value()) {
			(void)board->apply_move(Move(0, 0, 4, 4, P, false, true, false));
			check(board->get_zobrist_hash() == expected->get_zobrist_hash(),
					"incremental hash: drop emptying the hand matches from_sfen");
		}
	}
	{
		std::optional<BoardState> board = BoardState::from_sfen("4k4/9/9/9/4p4/4P4/9/9/4K4 b - 1");
		std::optional<BoardState> expected = BoardState::from_sfen("4k4/9/9/9/4P4/9/9/9/4K4 w P 2");
		check(board.has_value() && expected.has_value(), "incremental hash: capture positions parse");
		if (board.has_value() && expected.has_value()) {
			(void)board->apply_move(Move(4, 5, 4, 4, P, false, false, true));
			check(board->get_zobrist_hash() == expected->get_zobrist_hash(),
					"incremental hash: capture into an empty hand matches from_sfen");
		}
	}
}

void run_uchifuzume_tests() {
	{
		// 共通形: 1二への歩打ちが打ち歩詰めになる局面（玉は2三金の紐で取れず、2一香/2二桂が逃げ場を塞ぐ）
		auto make_base = [] {
			BoardState board(Turn::SENTE);
			board.set_cell({ 0, 0 }, K, Turn::GOTE, false); // 1一玉
			board.set_cell({ 1, 0 }, L, Turn::GOTE, false); // 2一香
			board.set_cell({ 1, 1 }, N, Turn::GOTE, false); // 2二桂
			board.set_cell({ 1, 2 }, G, Turn::SENTE, false); // 2三金（1二の歩に紐を付ける）
			board.set_cell({ 4, 8 }, K, Turn::SENTE, false); // 5九玉
			return board;
		};
		auto contains_pawn_drop_12 = [](BoardState &board) {
			Shogi::MoveList move_list;
			MoveGenerator::get_legal_moves(board, move_list);
			for (const Move &move : move_list) {
				if (move.is_drop && move.piece_type == P && move.to_col == 0 && move.to_row == 1) {
					return true;
				}
			}
			return false;
		};

		// 1) 単純な打ち歩詰め → 手生成から除外される
		{
			BoardState board = make_base();
			board.set_hand_count(Turn::SENTE, P, 1);
			check(!contains_pawn_drop_12(board), "uchifuzume: mating pawn drop not generated");
		}

		// 2) 2一香を除くと玉が2一へ逃げられる → 王手だが詰みではないので合法
		{
			BoardState board = make_base();
			board.clear_cell({ 1, 0 });
			board.set_hand_count(Turn::SENTE, P, 1);
			check(contains_pawn_drop_12(board), "uchifuzume: checking drop with escape square stays legal");
		}

		// 3) 1六飛が1二に利いていて歩を取れる → 合法
		{
			BoardState board = make_base();
			board.set_cell({ 0, 5 }, R, Turn::GOTE, false); // 1六飛
			board.set_hand_count(Turn::SENTE, P, 1);
			check(contains_pawn_drop_12(board), "uchifuzume: capturable pawn drop stays legal");
		}

		// 4) 歩を取れる唯一の駒(2二金)が5五角にピンされている → 取れないので打ち歩詰め
		{
			BoardState board(Turn::SENTE);
			board.set_cell({ 0, 0 }, K, Turn::GOTE, false); // 1一玉
			board.set_cell({ 1, 0 }, L, Turn::GOTE, false); // 2一香（逃げ場を塞ぐ）
			board.set_cell({ 1, 1 }, G, Turn::GOTE, false); // 2二金（1二に利く）
			board.set_cell({ 4, 4 }, B, Turn::SENTE, false); // 5五角が2二金をピン
			board.set_cell({ 1, 2 }, G, Turn::SENTE, false); // 2三金（1二の歩に紐）
			board.set_cell({ 4, 8 }, K, Turn::SENTE, false); // 5九玉
			board.set_hand_count(Turn::SENTE, P, 1);
			check(!contains_pawn_drop_12(board), "uchifuzume: pinned defender cannot capture -> drop is illegal");

			// 対照: 角を除くとピンが外れて2二金が歩を取れる → 合法
			board.clear_cell({ 4, 4 });
			check(contains_pawn_drop_12(board), "uchifuzume: unpinned defender can capture -> drop is legal");
		}

		// 5) 突き歩詰め（盤上の1三歩を1二へ進める詰み）は合法のまま
		{
			BoardState board = make_base();
			board.set_cell({ 0, 2 }, P, Turn::SENTE, false); // 1三歩（持ち歩なし）
			Shogi::MoveList move_list;
			MoveGenerator::get_legal_moves(board, move_list);
			bool found_push = false;
			for (const Move &move : move_list) {
				if (!move.is_drop && move.piece_type == P && move.from_col == 0 && move.from_row == 2 &&
						move.to_col == 0 && move.to_row == 1 && !move.is_promotion) {
					found_push = true;
				}
			}
			check(found_push, "uchifuzume: pawn PUSH mate (tsukifu-zume) stays legal");

			AIPlayer ai;
			AIPlayerTestAccess ai_t{ ai };
			check(ai_t.find_mate(board, 3).has_value(), "uchifuzume: find_mate finds pawn-push mate");
		}

		// 6) 打ち歩以外に詰みがない局面 → find_mate は詰みを証明しない
		{
			BoardState board = make_base();
			board.set_hand_count(Turn::SENTE, P, 1);
			AIPlayer ai;
			AIPlayerTestAccess ai_t{ ai };
			check(!ai_t.find_mate(board, 5).has_value(),
					"uchifuzume: find_mate does not prove mate via illegal pawn drop");
		}

		// 7) 後手番の打ち歩詰め（対称性の確認）: 先手 9九玉・8九香・8八桂 / 後手 8七金、後手持ち歩1
		{
			BoardState board(Turn::GOTE);
			board.set_cell({ 8, 8 }, K, Turn::SENTE, false); // 9九玉
			board.set_cell({ 7, 8 }, L, Turn::SENTE, false); // 8九香
			board.set_cell({ 7, 7 }, N, Turn::SENTE, false); // 8八桂
			board.set_cell({ 7, 6 }, G, Turn::GOTE, false); // 8七金（9八の歩に紐を付ける）
			board.set_cell({ 4, 0 }, K, Turn::GOTE, false); // 5一玉
			board.set_hand_count(Turn::GOTE, P, 1);
			Shogi::MoveList move_list;
			MoveGenerator::get_legal_moves(board, move_list);
			bool found = false;
			for (const Move &move : move_list) {
				if (move.is_drop && move.piece_type == P && move.to_col == 8 && move.to_row == 7) {
					found = true;
				}
			}
			check(!found, "uchifuzume: GOTE mating pawn drop not generated (mirror)");
		}

		// 8) 紐のない歩は玉で取れる → 王手でも合法（打ち歩詰めにならない、よくある形）
		{
			BoardState board = make_base();
			board.clear_cell({ 1, 2 }); // 2三金を除去 → 1二の歩に紐がなくなる
			board.set_hand_count(Turn::SENTE, P, 1);
			check(contains_pawn_drop_12(board), "uchifuzume: undefended pawn capturable by king stays legal");
		}
	}
}

void run_promotion_rule_tests() {
	check(MoveGenerator::is_dead_end(P, false, 0), "dead_end: SENTE pawn on rank 1");
	check(!MoveGenerator::is_dead_end(P, false, 1), "dead_end: SENTE pawn on rank 2 is allowed");
	check(MoveGenerator::is_dead_end(N, false, 1), "dead_end: SENTE knight on rank 2");
	check(MoveGenerator::is_dead_end(P, true, 8), "dead_end: GOTE pawn on rank 9");

	check(MoveGenerator::can_promote(P, false, false, 3, 2), "can_promote: SENTE to-square in zone");
	check(MoveGenerator::can_promote(P, false, false, 2, 3), "can_promote: SENTE from-square in zone");
	check(!MoveGenerator::can_promote(P, false, false, 4, 3), "can_promote: SENTE both squares outside zone");
	check(MoveGenerator::can_promote(P, false, true, 5, 6), "can_promote: GOTE to-square in zone");
	check(MoveGenerator::can_promote(P, false, true, 6, 5), "can_promote: GOTE from-square in zone");
	check(!MoveGenerator::can_promote(P, false, true, 4, 5), "can_promote: GOTE both squares outside zone");
	check(!MoveGenerator::can_promote(P, false, true, 3, 2), "can_promote: SENTE zone does not apply to GOTE");
	check(MoveGenerator::can_promote(S, false, false, 3, 2), "can_promote: silver can promote in zone");
	check(!MoveGenerator::can_promote(P, true, false, 3, 2), "can_promote: promoted piece cannot promote");
	check(!MoveGenerator::can_promote(K, false, false, 3, 2), "can_promote: king cannot promote");
	check(!MoveGenerator::can_promote(G, false, false, 3, 2), "can_promote: gold cannot promote");
	check(!MoveGenerator::can_promote(P, false, false, -1, 9), "can_promote: both rows out of range");
	check(MoveGenerator::can_promote(P, false, false, -1, 2), "can_promote: one row out of range, other in zone");
}

void run_king_move_tests() {
	auto king_targets = [](BoardState &board, bool only_captures = false) {
		Shogi::MoveList move_list;
		MoveGenerator::get_legal_moves(board, move_list, only_captures);
		std::vector<Shogi::Coord> targets;
		for (const Move &move : move_list) {
			if (!move.is_drop && move.piece_type == K) {
				targets.push_back({ move.to_col, move.to_row });
			}
		}
		return targets;
	};
	auto has_target = [](const std::vector<Shogi::Coord> &targets, int col, int row) {
		return std::ranges::find(targets, Shogi::Coord{ col, row }) != targets.end();
	};

	// 相手の駒が利いているマスには動けない
	{
		BoardState board(Turn::SENTE);
		board.set_cell({ 4, 4 }, K, Turn::SENTE, false); // 5五玉
		board.set_cell({ 4, 2 }, G, Turn::GOTE, false); // 5三金
		board.set_cell({ 8, 0 }, K, Turn::GOTE, false); // 9一玉
		auto targets = king_targets(board);
		check(targets.size() == 5, "king move: squares attacked by gold are excluded");
		check(!has_target(targets, 3, 3) && !has_target(targets, 4, 3) && !has_target(targets, 5, 3),
				"king move: cannot step into gold's attacks");
	}

	// 相手の玉の隣には動けない
	{
		BoardState board(Turn::SENTE);
		board.set_cell({ 4, 4 }, K, Turn::SENTE, false); // 5五玉
		board.set_cell({ 4, 2 }, K, Turn::GOTE, false); // 5三玉
		auto targets = king_targets(board);
		check(targets.size() == 5, "king move: squares next to opponent king are excluded");
		check(!has_target(targets, 3, 3) && !has_target(targets, 4, 3) && !has_target(targets, 5, 3),
				"king move: cannot step next to opponent king");
	}

	// 飛び駒の王手の筋に沿って下がれない
	{
		BoardState board(Turn::SENTE);
		board.set_cell({ 4, 4 }, K, Turn::SENTE, false); // 5五玉
		board.set_cell({ 4, 0 }, R, Turn::GOTE, false); // 5一飛
		board.set_cell({ 8, 0 }, K, Turn::GOTE, false); // 9一玉
		auto targets = king_targets(board);
		check(!has_target(targets, 4, 5), "king move: cannot retreat along rook check");
		check(targets.size() == 6, "king move: rook check leaves six escapes");
	}
	{
		BoardState board(Turn::SENTE);
		board.set_cell({ 4, 4 }, K, Turn::SENTE, false); // 5五玉
		board.set_cell({ 0, 0 }, B, Turn::GOTE, false); // 1一角
		board.set_cell({ 8, 0 }, K, Turn::GOTE, false); // 9一玉
		auto targets = king_targets(board);
		check(!has_target(targets, 5, 5), "king move: cannot retreat along bishop check");
		check(targets.size() == 6, "king move: bishop check leaves six escapes");
	}
	{
		BoardState board(Turn::SENTE);
		board.set_cell({ 4, 4 }, K, Turn::SENTE, false); // 5五玉
		board.set_cell({ 4, 0 }, L, Turn::GOTE, false); // 5一香
		board.set_cell({ 8, 0 }, K, Turn::GOTE, false); // 9一玉
		auto targets = king_targets(board);
		check(!has_target(targets, 4, 5), "king move: cannot retreat along lance check");
		check(targets.size() == 6, "king move: lance check leaves six escapes");
	}

	// 両王手では玉しか動けず、両方の筋を避ける
	{
		BoardState board(Turn::SENTE);
		board.set_cell({ 4, 4 }, K, Turn::SENTE, false); // 5五玉
		board.set_cell({ 4, 0 }, R, Turn::GOTE, false); // 5一飛
		board.set_cell({ 0, 0 }, B, Turn::GOTE, false); // 1一角
		board.set_cell({ 8, 0 }, K, Turn::GOTE, false); // 9一玉
		board.set_cell({ 6, 6 }, G, Turn::SENTE, false); // 7七金
		board.set_hand_count(Turn::SENTE, P, 1);
		Shogi::MoveList move_list;
		MoveGenerator::get_legal_moves(board, move_list);
		auto targets = king_targets(board);
		check(targets.size() == 4 && move_list.size() == 4, "king move: double check allows only four king moves");
		check(!has_target(targets, 4, 5) && !has_target(targets, 5, 5), "king move: double check avoids both lines");
	}

	// 紐のない王手駒は取れ、紐の付いた王手駒は取れない
	{
		auto make_board = [] {
			BoardState board(Turn::SENTE);
			board.set_cell({ 4, 4 }, K, Turn::SENTE, false); // 5五玉
			board.set_cell({ 4, 3 }, G, Turn::GOTE, false); // 5四金
			board.set_cell({ 8, 0 }, K, Turn::GOTE, false); // 9一玉
			return board;
		};
		{
			BoardState board = make_board();
			check(has_target(king_targets(board), 4, 3), "king move: can capture undefended checker");
			check(has_target(king_targets(board, true), 4, 3), "king move: captures-only includes undefended checker");
		}
		{
			BoardState board = make_board();
			board.set_cell({ 4, 2 }, S, Turn::GOTE, false); // 5三銀
			check(!has_target(king_targets(board), 4, 3), "king move: cannot capture checker defended by silver");
			check(!has_target(king_targets(board, true), 4, 3), "king move: captures-only excludes defended checker");
		}
		{
			BoardState board = make_board();
			board.set_cell({ 4, 0 }, R, Turn::GOTE, false); // 5一飛
			check(!has_target(king_targets(board), 4, 3), "king move: cannot capture checker defended by rook behind it");
		}
		{
			BoardState board = make_board();
			board.set_cell({ 4, 2 }, P, Turn::GOTE, true); // 5三と
			check(!has_target(king_targets(board), 4, 3), "king move: cannot capture checker defended by tokin");
		}
	}

	// 後手の玉でも同じ
	{
		BoardState board(Turn::GOTE);
		board.set_cell({ 4, 4 }, K, Turn::GOTE, false); // 5五玉
		board.set_cell({ 4, 8 }, R, Turn::SENTE, false); // 5九飛
		board.set_cell({ 0, 8 }, K, Turn::SENTE, false); // 1九玉
		auto targets = king_targets(board);
		check(!has_target(targets, 4, 3), "king move: GOTE cannot retreat along rook check");
		check(targets.size() == 6, "king move: GOTE rook check leaves six escapes");
	}
	{
		BoardState board(Turn::GOTE);
		board.set_cell({ 4, 4 }, K, Turn::GOTE, false); // 5五玉
		board.set_cell({ 4, 5 }, G, Turn::SENTE, false); // 5六金
		board.set_cell({ 4, 6 }, S, Turn::SENTE, false); // 5七銀
		board.set_cell({ 0, 8 }, K, Turn::SENTE, false); // 1九玉
		check(!has_target(king_targets(board), 4, 5), "king move: GOTE cannot capture defended checker");
	}
}

void run_nifu_tests() {
	auto has_pawn_drop_on_col = [](BoardState &board, int col) {
		Shogi::MoveList move_list;
		MoveGenerator::get_legal_moves(board, move_list);
		for (const Move &move : move_list) {
			if (move.is_drop && move.piece_type == P && move.to_col == col) {
				return true;
			}
		}
		return false;
	};

	BoardState board(Turn::SENTE);
	board.set_cell({ 4, 8 }, K, Turn::SENTE, false); // 5九玉
	board.set_cell({ 4, 0 }, K, Turn::GOTE, false); // 5一玉
	board.set_cell({ 2, 6 }, P, Turn::SENTE, false); // 3七歩
	board.set_cell({ 6, 6 }, P, Turn::SENTE, true); // 7七と
	board.set_cell({ 7, 2 }, P, Turn::GOTE, false); // 8三歩（相手の歩）
	board.set_hand_count(Turn::SENTE, P, 1);

	check(!has_pawn_drop_on_col(board, 2), "nifu: no pawn drop on a file with own pawn");
	check(has_pawn_drop_on_col(board, 3), "nifu: pawn drop allowed on an empty file");
	check(has_pawn_drop_on_col(board, 6), "nifu: promoted pawn does not count");
	check(has_pawn_drop_on_col(board, 7), "nifu: opponent pawn does not count");
}

} // namespace

void run_zobrist_tests(const std::vector<uint8_t> &data) {
	check(!BoardState::zobrist_initialized(), "zobrist: not initialized before loading");

	std::vector<uint8_t> bad_magic = data;
	if (!bad_magic.empty()) {
		bad_magic[0] ^= 0xFF;
	}
	check(!BoardState::load_zobrist_params(bad_magic), "zobrist: bad magic is rejected");
	check(data.size() > 8 && !BoardState::load_zobrist_params(std::span(data).first(data.size() - 8)),
			"zobrist: truncated data is rejected");
	check(!BoardState::zobrist_initialized(), "zobrist: rejected data leaves params uninitialized");

	check(BoardState::load_zobrist_params(data), "zobrist: production file is accepted");
	check(BoardState::zobrist_initialized(), "zobrist: initialized after loading");
	check(BoardState::load_zobrist_params({}), "zobrist: loading again is a no-op success");
}

void run_rules_tests() {
	run_from_sfen_tests();
	run_set_hand_count_tests();
	run_incremental_hash_tests();
	run_uchifuzume_tests();
	run_promotion_rule_tests();
	run_king_move_tests();
	run_nifu_tests();
}
