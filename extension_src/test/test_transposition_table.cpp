#include "move_generator.hpp"
#include "test_support.hpp"
#include "transposition_table.hpp"
#include <cstdint>
#include <iterator>
#include <optional>
#include <unordered_set>

namespace {

// With a single bucket, every key maps to the same bucket
constexpr size_t ONE_BUCKET_BYTES = 64;
// 1024 buckets
constexpr size_t SMALL_TABLE_BYTES = 1 << 16;

bool has_entry(const TranspositionTable &tt, uint64_t key) {
	return tt.probe(key).has_value();
}

void run_pack_move_tests() {
	constexpr const char *SFENS[] = {
		"lnsgkgsnl/1r5b1/ppppppppp/9/9/9/PPPPPPPPP/1B5R1/LNSGKGSNL b - 1",
		"lnsg1g2l/1rk3s2/p1pppp1pp/6p2/2N6/1SP2PPP1/PPNPR3P/3g2S2/+p4K1NL b 2BLPg 41",
		"ln1g1g2l/1rks2s2/p1pp1p1pp/4+R1p2/2N6/1SP2PPP1/PPNP4P/3gg1S2/+p5KNL w 2BL2P 46",
	};

	bool all_parsed = true;
	bool all_nonzero = true;
	bool all_unique = true;
	bool all_in_range = true;
	bool has_drop = false;
	bool has_promotion = false;
	for (const char *sfen : SFENS) {
		std::optional<BoardState> board = BoardState::from_sfen(sfen);
		if (!board.has_value()) {
			all_parsed = false;
			continue;
		}
		Shogi::MoveList move_list;
		MoveGenerator::get_legal_moves(*board, move_list);
		std::unordered_set<uint16_t> packed;
		for (const Move &move : move_list) {
			uint16_t value = pack_move(move);
			all_nonzero = all_nonzero && value != 0;
			all_in_range = all_in_range && value < (1 << 15);
			packed.insert(value);
			has_drop = has_drop || move.is_drop;
			has_promotion = has_promotion || move.is_promotion;
		}
		all_unique = all_unique && std::ssize(packed) == move_list.count;
	}
	check(all_parsed, "pack_move: test positions parse");
	check(has_drop && has_promotion, "pack_move: test positions include drops and promotions");
	check(all_nonzero, "pack_move: no legal move packs to 0");
	check(all_in_range, "pack_move: packed values fit in 15 bits");
	check(all_unique, "pack_move: legal moves of a position pack to distinct values");
}

void run_store_probe_tests() {
	TranspositionTable tt;
	tt.allocate(SMALL_TABLE_BYTES);
	check(!tt.probe(0).has_value(), "tt: key 0 does not match an empty entry");
	check(!tt.probe(0x1234).has_value(), "tt: probe misses a key that was not stored");

	tt.store(0x1234, -500, 3, TTFlag::LOWER_BOUND, 0x0abc);
	std::optional<TTData> data = tt.probe(0x1234);
	check(data.has_value() && data->score == -500 && data->depth == 3 && data->flag == TTFlag::LOWER_BOUND &&
					data->move == 0x0abc,
			"tt: stored values are read back");
	check(!tt.probe(0x1234 + (1ULL << 40)).has_value(), "tt: a key in the same bucket with different upper bits misses");

	constexpr int MATE_IN_TT = AIPlayer::MATE_SCORE - 3;
	tt.store(0x5678, MATE_IN_TT, 7, TTFlag::EXACT, 0);
	tt.store(0x9abc, -MATE_IN_TT, 7, TTFlag::EXACT, 0);
	std::optional<TTData> win = tt.probe(0x5678);
	std::optional<TTData> loss = tt.probe(0x9abc);
	check(win.has_value() && win->score == MATE_IN_TT && loss.has_value() && loss->score == -MATE_IN_TT,
			"tt: mate scores outside int16 are read back");

	tt.store(0x2222, 0, 127, TTFlag::UPPER_BOUND, 0x7fff);
	data = tt.probe(0x2222);
	check(data.has_value() && data->depth == 127 && data->flag == TTFlag::UPPER_BOUND && data->move == 0x7fff,
			"tt: the largest depth and move are read back");

	tt.store(0x3333, 42, 0, TTFlag::EXACT, 0x0123);
	data = tt.probe(0x3333);
	check(data.has_value() && data->score == 42 && data->depth == 0 && data->flag == TTFlag::EXACT && data->move == 0x0123,
			"tt: a depth-0 entry is read back");

	tt.allocate(SMALL_TABLE_BYTES * 4);
	check(has_entry(tt, 0x1234), "tt: allocating again keeps the entries");
}

void run_replacement_tests() {
	{
		TranspositionTable tt;
		tt.allocate(ONE_BUCKET_BYTES);
		tt.store(1, 100, 5, TTFlag::EXACT, 11);
		tt.store(1, 200, 3, TTFlag::EXACT, 22);
		std::optional<TTData> data = tt.probe(1);
		check(data.has_value() && data->score == 100 && data->depth == 5 && data->move == 11,
				"tt replace: a shallower store for the same key keeps the entry");

		tt.store(1, 300, 5, TTFlag::LOWER_BOUND, 33);
		data = tt.probe(1);
		check(data.has_value() && data->score == 300 && data->flag == TTFlag::LOWER_BOUND && data->move == 33,
				"tt replace: an equal-depth store for the same key overwrites");

		tt.new_search();
		tt.store(1, 400, 2, TTFlag::UPPER_BOUND, 44);
		data = tt.probe(1);
		check(data.has_value() && data->score == 400 && data->depth == 2 && data->move == 44,
				"tt replace: a store for the same key overwrites an older generation even when shallower");
	}
	{
		TranspositionTable tt;
		tt.allocate(ONE_BUCKET_BYTES);
		tt.store(1, 0, 8, TTFlag::EXACT, 1);
		tt.store(2, 0, 2, TTFlag::EXACT, 2);
		tt.store(3, 0, 8, TTFlag::EXACT, 3);
		tt.store(4, 0, 8, TTFlag::EXACT, 4);
		check(has_entry(tt, 1) && has_entry(tt, 2) && has_entry(tt, 3) && has_entry(tt, 4),
				"tt replace: stores fill empty slots first");

		tt.store(5, 0, 8, TTFlag::EXACT, 5);
		check(!has_entry(tt, 2) && has_entry(tt, 1) && has_entry(tt, 3) && has_entry(tt, 4) && has_entry(tt, 5),
				"tt replace: the shallowest entry of the same generation is replaced");
	}
	{
		TranspositionTable tt;
		tt.allocate(ONE_BUCKET_BYTES);
		tt.store(1, 0, 9, TTFlag::EXACT, 1);
		tt.new_search();
		tt.store(2, 0, 9, TTFlag::EXACT, 2);
		tt.new_search();
		tt.store(3, 0, 1, TTFlag::EXACT, 3);
		tt.store(4, 0, 1, TTFlag::EXACT, 4);
		tt.store(5, 0, 1, TTFlag::EXACT, 5);
		check(!has_entry(tt, 1) && has_entry(tt, 2) && has_entry(tt, 3) && has_entry(tt, 4) && has_entry(tt, 5),
				"tt replace: the oldest generation is replaced even when deepest");

		tt.store(6, 0, 1, TTFlag::EXACT, 6);
		check(!has_entry(tt, 2) && has_entry(tt, 3) && has_entry(tt, 4) && has_entry(tt, 5) && has_entry(tt, 6),
				"tt replace: the next oldest generation is replaced next");
	}
	{
		TranspositionTable tt;
		tt.allocate(ONE_BUCKET_BYTES);
		for (uint64_t key = 1; key <= 4; ++key) {
			tt.store(key, 0, 3, TTFlag::EXACT, 0);
		}
		tt.store(5, 0, 3, TTFlag::EXACT, 0);
		check(!has_entry(tt, 1) && has_entry(tt, 2) && has_entry(tt, 3) && has_entry(tt, 4) && has_entry(tt, 5),
				"tt replace: ties replace the first entry in the bucket");
	}
	{
		TranspositionTable tt;
		tt.allocate(ONE_BUCKET_BYTES);
		tt.store(1, 0, 3, TTFlag::EXACT, 1);
		tt.store(1, 0, 0, TTFlag::EXACT, 2);
		std::optional<TTData> data = tt.probe(1);
		check(data.has_value() && data->depth == 3 && data->move == 1,
				"tt replace: a depth-0 store keeps a deeper entry of the same generation");
	}
	{
		TranspositionTable tt;
		tt.allocate(ONE_BUCKET_BYTES);
		tt.store(1, 0, 1, TTFlag::EXACT, 0);
		tt.store(2, 0, 0, TTFlag::EXACT, 0);
		tt.store(3, 0, 1, TTFlag::EXACT, 0);
		tt.store(4, 0, 1, TTFlag::EXACT, 0);
		check(has_entry(tt, 1) && has_entry(tt, 2) && has_entry(tt, 3) && has_entry(tt, 4),
				"tt replace: a depth-0 entry takes a slot");
		tt.store(5, 0, 1, TTFlag::EXACT, 0);
		check(!has_entry(tt, 2) && has_entry(tt, 1) && has_entry(tt, 3) && has_entry(tt, 4) && has_entry(tt, 5),
				"tt replace: a depth-0 entry is replaced before depth-1 entries");
	}
}

void run_hashfull_tests() {
	TranspositionTable tt;
	tt.allocate(SMALL_TABLE_BYTES);
	check(tt.hashfull() == 0, "tt hashfull: an empty table is 0");

	// One entry goes into the first slot of each of the 1024 buckets
	for (uint64_t key = 1; key <= 1024; ++key) {
		tt.store(key, 0, 1, TTFlag::EXACT, 0);
	}
	check(tt.hashfull() == 250, "tt hashfull: counts used entries among the first 1000");

	tt.new_search();
	check(tt.hashfull() == 0, "tt hashfull: counts only the current generation");
}

} // namespace

void run_transposition_table_tests() {
	run_pack_move_tests();
	run_store_probe_tests();
	run_replacement_tests();
	run_hashfull_tests();
}
