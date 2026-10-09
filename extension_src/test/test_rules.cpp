#include "move_generator.hpp"
#include "test_support.hpp"

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
		BoardState b(Turn::SENTE);
		b.set_cell({ 4, 8 }, K, Turn::SENTE, false);
		b.set_cell({ 4, 0 }, K, Turn::GOTE, false);
		check(b.get_hand_count(Turn::SENTE, P) == 0, "set_hand_count: initial hand is empty");

		uint64_t h0 = b.get_zobrist_hash();
		b.set_hand_count(Turn::SENTE, P, 1);
		check(b.get_hand_count(Turn::SENTE, P) == 1, "set_hand_count: count is set");
		check(b.get_zobrist_hash() != h0, "set_hand_count: zobrist hash changes");

		uint64_t h1 = b.get_zobrist_hash();
		Move drop(0, 0, 4, 4, P, false, true, false);
		Shogi::UndoInfo undo = b.apply_move(drop);
		check(b.get_hand_count(Turn::SENTE, P) == 0, "set_hand_count: drop consumes hand piece");
		b.undo_move(undo);
		check(b.get_zobrist_hash() == h1 && b.get_hand_count(Turn::SENTE, P) == 1,
				"set_hand_count: drop round-trips with apply_move/undo_move");

		b.set_hand_count(Turn::SENTE, P, 0);
		check(b.get_hand_count(Turn::SENTE, P) == 0 && b.get_zobrist_hash() == h0,
				"set_hand_count: resetting to 0 restores hash");
	}
}

void run_uchifuzume_tests() {
	{
		// 共通形: 1二への歩打ちが打ち歩詰めになる局面（玉は2三金の紐で取れず、2一香/2二桂が逃げ場を塞ぐ）
		auto make_base = [] {
			BoardState b(Turn::SENTE);
			b.set_cell({ 0, 0 }, K, Turn::GOTE, false); // 1一玉
			b.set_cell({ 1, 0 }, L, Turn::GOTE, false); // 2一香
			b.set_cell({ 1, 1 }, N, Turn::GOTE, false); // 2二桂
			b.set_cell({ 1, 2 }, G, Turn::SENTE, false); // 2三金（1二の歩に紐を付ける）
			b.set_cell({ 4, 8 }, K, Turn::SENTE, false); // 5九玉
			return b;
		};
		auto contains_pawn_drop_12 = [](BoardState &b) {
			Shogi::MoveList list;
			MoveGenerator::get_legal_moves(b, list);
			for (const Move &m : list) {
				if (m.is_drop && m.piece_type == P && m.to_col == 0 && m.to_row == 1) {
					return true;
				}
			}
			return false;
		};

		// 1) 単純な打ち歩詰め → 手生成から除外され、is_legal_drop も false
		{
			BoardState b = make_base();
			b.set_hand_count(Turn::SENTE, P, 1);
			check(!contains_pawn_drop_12(b), "uchifuzume: mating pawn drop not generated");
			check(!MoveGenerator::is_legal_drop(b, P, false, { 0, 1 }),
					"uchifuzume: is_legal_drop rejects mating pawn drop");
		}

		// 2) 2一香を除くと玉が2一へ逃げられる → 王手だが詰みではないので合法
		{
			BoardState b = make_base();
			b.clear_cell({ 1, 0 });
			b.set_hand_count(Turn::SENTE, P, 1);
			check(contains_pawn_drop_12(b), "uchifuzume: checking drop with escape square stays legal");
			check(MoveGenerator::is_legal_drop(b, P, false, { 0, 1 }),
					"uchifuzume: is_legal_drop allows drop with escape");
		}

		// 3) 1六飛が1二に利いていて歩を取れる → 合法
		{
			BoardState b = make_base();
			b.set_cell({ 0, 5 }, R, Turn::GOTE, false); // 1六飛
			b.set_hand_count(Turn::SENTE, P, 1);
			check(contains_pawn_drop_12(b), "uchifuzume: capturable pawn drop stays legal");
		}

		// 4) 歩を取れる唯一の駒(2二金)が5五角にピンされている → 取れないので打ち歩詰め
		{
			BoardState b(Turn::SENTE);
			b.set_cell({ 0, 0 }, K, Turn::GOTE, false); // 1一玉
			b.set_cell({ 1, 0 }, L, Turn::GOTE, false); // 2一香（逃げ場を塞ぐ）
			b.set_cell({ 1, 1 }, G, Turn::GOTE, false); // 2二金（1二に利く）
			b.set_cell({ 4, 4 }, B, Turn::SENTE, false); // 5五角が2二金をピン
			b.set_cell({ 1, 2 }, G, Turn::SENTE, false); // 2三金（1二の歩に紐）
			b.set_cell({ 4, 8 }, K, Turn::SENTE, false); // 5九玉
			b.set_hand_count(Turn::SENTE, P, 1);
			check(!contains_pawn_drop_12(b), "uchifuzume: pinned defender cannot capture -> drop is illegal");

			// 対照: 角を除くとピンが外れて2二金が歩を取れる → 合法
			b.clear_cell({ 4, 4 });
			check(contains_pawn_drop_12(b), "uchifuzume: unpinned defender can capture -> drop is legal");
		}

		// 5) 突き歩詰め（盤上の1三歩を1二へ進める詰み）は合法のまま
		{
			BoardState b = make_base();
			b.set_cell({ 0, 2 }, P, Turn::SENTE, false); // 1三歩（持ち歩なし）
			Shogi::MoveList list;
			MoveGenerator::get_legal_moves(b, list);
			bool found_push = false;
			for (const Move &m : list) {
				if (!m.is_drop && m.piece_type == P && m.from_col == 0 && m.from_row == 2 &&
						m.to_col == 0 && m.to_row == 1 && !m.is_promotion) {
					found_push = true;
				}
			}
			check(found_push, "uchifuzume: pawn PUSH mate (tsukifu-zume) stays legal");

			AIPlayer ai;
			AIPlayerTestAccess ai_t{ ai };
			check(ai_t.find_mate(b, 3).has_value(), "uchifuzume: find_mate finds pawn-push mate");
		}

		// 6) 打ち歩以外に詰みがない局面 → find_mate は詰みを証明しない
		{
			BoardState b = make_base();
			b.set_hand_count(Turn::SENTE, P, 1);
			AIPlayer ai;
			AIPlayerTestAccess ai_t{ ai };
			check(!ai_t.find_mate(b, 5).has_value(),
					"uchifuzume: find_mate does not prove mate via illegal pawn drop");
		}

		// 7) 後手番の打ち歩詰め（対称性の確認）: 先手 9九玉・8九香・8八桂 / 後手 8七金、後手持ち歩1
		{
			BoardState b(Turn::GOTE);
			b.set_cell({ 8, 8 }, K, Turn::SENTE, false); // 9九玉
			b.set_cell({ 7, 8 }, L, Turn::SENTE, false); // 8九香
			b.set_cell({ 7, 7 }, N, Turn::SENTE, false); // 8八桂
			b.set_cell({ 7, 6 }, G, Turn::GOTE, false); // 8七金（9八の歩に紐を付ける）
			b.set_cell({ 4, 0 }, K, Turn::GOTE, false); // 5一玉
			b.set_hand_count(Turn::GOTE, P, 1);
			Shogi::MoveList list;
			MoveGenerator::get_legal_moves(b, list);
			bool found = false;
			for (const Move &m : list) {
				if (m.is_drop && m.piece_type == P && m.to_col == 8 && m.to_row == 7) {
					found = true;
				}
			}
			check(!found, "uchifuzume: GOTE mating pawn drop not generated (mirror)");
			check(!MoveGenerator::is_legal_drop(b, P, true, { 8, 7 }),
					"uchifuzume: is_legal_drop rejects GOTE mating drop (mirror)");
		}

		// 8) 手番が打つ側と異なる盤面での照会（GUI経路の再現）でも正しく判定される
		{
			BoardState b(Turn::GOTE); // 手番はGOTEのまま、先手の歩打ちを照会する
			b.set_cell({ 0, 0 }, K, Turn::GOTE, false); // 1一玉
			b.set_cell({ 1, 0 }, L, Turn::GOTE, false); // 2一香
			b.set_cell({ 1, 1 }, N, Turn::GOTE, false); // 2二桂
			b.set_cell({ 1, 2 }, G, Turn::SENTE, false); // 2三金
			b.set_cell({ 4, 8 }, K, Turn::SENTE, false); // 5九玉
			b.set_hand_count(Turn::SENTE, P, 1);
			uint64_t h = b.get_zobrist_hash();
			check(!MoveGenerator::is_legal_drop(b, P, false, { 0, 1 }),
					"uchifuzume: rejected even when board turn differs from dropping side");
			check(b.get_zobrist_hash() == h, "uchifuzume: mismatched-turn probe restores hash");
		}

		// 9) 紐のない歩は玉で取れる → 王手でも合法（打ち歩詰めにならない、よくある形）
		{
			BoardState b = make_base();
			b.clear_cell({ 1, 2 }); // 2三金を除去 → 1二の歩に紐がなくなる
			b.set_hand_count(Turn::SENTE, P, 1);
			check(contains_pawn_drop_12(b), "uchifuzume: undefended pawn capturable by king stays legal");
		}
	}
}

} // namespace

void run_zobrist_tests(const std::vector<uint8_t> &data) {
	check(!BoardState::zobrist_initialized(), "zobrist: not initialized before loading");

	std::vector<uint8_t> bad_magic = data;
	if (!bad_magic.empty()) {
		bad_magic[0] ^= 0xFF;
	}
	check(!BoardState::load_zobrist_params(bad_magic.data(), bad_magic.size()), "zobrist: bad magic is rejected");
	check(data.size() > 8 && !BoardState::load_zobrist_params(data.data(), data.size() - 8),
			"zobrist: truncated data is rejected");
	check(!BoardState::zobrist_initialized(), "zobrist: rejected data leaves params uninitialized");

	check(BoardState::load_zobrist_params(data.data(), data.size()), "zobrist: production file is accepted");
	check(BoardState::zobrist_initialized(), "zobrist: initialized after loading");
	check(BoardState::load_zobrist_params(nullptr, 0), "zobrist: loading again is a no-op success");
}

void run_rules_tests() {
	run_from_sfen_tests();
	run_set_hand_count_tests();
	run_uchifuzume_tests();
}
