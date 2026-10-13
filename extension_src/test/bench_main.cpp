#include "ai_player.hpp"
#include "board_state.hpp"
#include "test_support.hpp"
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <format>
#include <optional>
#include <print>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace {

struct BenchPosition {
	const char *name;
	const char *sfen;
};

constexpr BenchPosition POSITIONS[] = {
	{ "start", "lnsgkgsnl/1r5b1/ppppppppp/9/9/9/PPPPPPPPP/1B5R1/LNSGKGSNL b - 1" },
	{ "kif2-10", "lnsg1gsnl/1r1k5/p1pppp1pp/6p2/1p7/2P4P1/PPSPPPP1P/7R1/LN1GKGSNL b Bb 11" },
	{ "kif2-20", "lnsg1g2l/1rk3s2/p1pppp1pp/6p2/1p3n3/2P4P1/PPSPPPP1P/3GG1SR1/LN2K2NL b Bb 21" },
	{ "kif2-30", "lnsg1g2l/1rk3s2/p1pppp1pp/6p2/9/1SP2P1P1/PPNPG1PRP/4G1S2/L+p2K1bNL b BNP 31" },
	{ "kif2-40", "lnsg1g2l/1rk3s2/p1pppp1pp/6p2/2N6/1SP2PPP1/PPNPR3P/3g2S2/+p4K1NL b 2BLPg 41" },
	{ "kif2-45", "ln1g1g2l/1rks2s2/p1pp1p1pp/4+R1p2/2N6/1SP2PPP1/PPNP4P/3gg1S2/+p5KNL w 2BL2P 46" },
	{ "kif2-50", "lnkg1g2l/1r4s2/p1s+N1p1pp/2+R3p2/9/1SP2PPP1/PPNP4P/3gg1S2/+p5KNL b 2BL4P 51" },
	{ "kif2-59", "lk3g2l/1rg3s2/p1+R1Bp1pp/4B1p2/9/1SP2PPP1/PPNP4P/3gg1S2/+p5KNL w S2NL4P 60" },
};

constexpr int DEFAULT_DEPTH = 6;
constexpr int DEFAULT_TIME_MSEC = 1000;
constexpr uint64_t UNLIMITED_TIME_USEC = 3600ULL * 1000000;

// gameモードで探索する局面 (KIFU2の指した手数)
constexpr int GAME_FIRST_MOVE_COUNT = 10;
constexpr int GAME_LAST_MOVE_COUNT = 39;

constexpr int ANALYSIS_CANDIDATE_COUNT = 3;

struct SearchLog {
	int completed_depth = 0;
	uint64_t nodes = 0;
	int aspiration_fails = 0;
	bool mate = false;
	int tt_clears = 0;
	std::string tt;
};

std::string format_square(int col, int row) {
	return std::format("{}{}", 9 - col, static_cast<char>('a' + row));
}

std::string format_move(const Shogi::Move &move) {
	constexpr std::string_view PIECE_CHARS = "KRBGSNLP";
	if (move.is_drop) {
		return std::format("{}*{}", PIECE_CHARS[std::to_underlying(move.piece_type)], format_square(move.to_col, move.to_row));
	}
	return std::format("{}{}{}", format_square(move.from_col, move.from_row), format_square(move.to_col, move.to_row),
			move.is_promotion ? "+" : "");
}

void print_usage() {
	std::println("usage: shogi_bench <zobrist_params.bin> [depth N | time MSEC | game ANALYSIS_DEPTH AI_DEPTH]");
}

void attach_logger(AIPlayer &ai, SearchLog &log) {
	ai.set_logger([&log](const std::string &line) {
		int depth = 0;
		unsigned long long nodes = 0;
		if (std::sscanf(line.c_str(), "Depth %d completed.", &depth) == 1) {
			log.completed_depth = depth;
		} else if (std::sscanf(line.c_str(), "Total nodes searched: %llu", &nodes) == 1) {
			log.nodes = nodes;
			if (size_t pos = line.find(", "); pos != std::string::npos) {
				log.tt = line.substr(pos + 2);
			}
		} else if (line.starts_with("Aspiration fail")) {
			++log.aspiration_fails;
		} else if (line.starts_with("Checkmate")) {
			log.mate = true;
		} else if (line.starts_with("TT size exceeded limit")) {
			++log.tt_clears;
		}
	});
}

bool is_complete(const SearchLog &log, int depth) {
	return log.mate || log.completed_depth == depth;
}

void print_heap_size() {
#ifdef __EMSCRIPTEN__
	std::println("heap_bytes={}", __builtin_wasm_memory_size(0) * 65536);
#endif
}

int run_positions(bool depth_mode, int value) {
	uint64_t total_nodes = 0;
	double total_ms = 0.0;
	int total_aspiration_fails = 0;
	int incomplete = 0;

	for (const BenchPosition &position : POSITIONS) {
		std::optional<BoardState> board = BoardState::from_sfen(position.sfen);
		if (!board.has_value()) {
			std::println("{} status=INCOMPLETE (SFEN could not be parsed)", position.name);
			++incomplete;
			continue;
		}

		AIPlayer ai;
		if (depth_mode) {
			ai.set_max_depth(value);
			ai.set_time_limit_usec(UNLIMITED_TIME_USEC);
		} else {
			ai.set_time_limit_usec(static_cast<uint64_t>(value) * 1000);
		}

		SearchLog log;
		attach_logger(ai, log);

		auto start = std::chrono::steady_clock::now();
		std::vector<ScoredMove> moves = ai.search_top_moves(*board, 1);
		double ms = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - start).count();

		std::string_view status = "OK";
		if (log.mate) {
			status = "MATE";
		} else if (depth_mode && log.completed_depth != value) {
			status = "INCOMPLETE";
		}

		std::println("{:8} best={:6} score={:8} depth={:2} nodes={:10} time_ms={:9.1f} asp_fail={} status={} tt=[{}]",
				position.name, moves.empty() ? "-" : format_move(moves[0].move), moves.empty() ? 0 : moves[0].score,
				log.completed_depth, log.nodes, ms, log.aspiration_fails, status, log.tt);

		if (status == "INCOMPLETE") {
			++incomplete;
			continue;
		}
		total_nodes += log.nodes;
		total_ms += ms;
		total_aspiration_fails += log.aspiration_fails;
	}

	uint64_t nps = total_ms > 0.0 ? static_cast<uint64_t>(static_cast<double>(total_nodes) * 1000.0 / total_ms) : 0;
	std::println("TOTAL nodes={} time_ms={:.1f} nps={} asp_fail={} incomplete={}", total_nodes, total_ms, nps,
			total_aspiration_fails, incomplete);
	return depth_mode && incomplete > 0 ? 1 : 0;
}

int run_game(int analysis_depth, int ai_depth) {
	BoardState board(Turn::SENTE);
	setup_initial_position(board);
	if (!replay(board, GAME_FIRST_MOVE_COUNT, KIFU2)) {
		return 1;
	}

	AIPlayer ai;
	ai.set_time_limit_usec(UNLIMITED_TIME_USEC);
	SearchLog log;
	attach_logger(ai, log);

	uint64_t analysis_nodes = 0;
	uint64_t ai_nodes = 0;
	int tt_clears = 0;
	int incomplete = 0;
	auto start = std::chrono::steady_clock::now();
	for (int move_count = GAME_FIRST_MOVE_COUNT; move_count <= GAME_LAST_MOVE_COUNT; ++move_count) {
		log = SearchLog{};
		ai.set_max_depth(analysis_depth);
		static_cast<void>(ai.search_top_moves(board, ANALYSIS_CANDIDATE_COUNT));
		SearchLog analysis_log = log;

		log = SearchLog{};
		ai.set_max_depth(ai_depth);
		std::vector<ScoredMove> moves = ai.search_top_moves(board, 1);
		SearchLog ai_log = log;

		analysis_nodes += analysis_log.nodes;
		ai_nodes += ai_log.nodes;
		tt_clears += analysis_log.tt_clears + ai_log.tt_clears;
		bool complete = is_complete(analysis_log, analysis_depth) && is_complete(ai_log, ai_depth);
		if (!complete) {
			++incomplete;
		}
		std::println("kif2-{} analysis_depth={:2} analysis_nodes={:10} analysis_tt=[{}] ai_best={:6} ai_score={:8} ai_depth={:2} ai_nodes={:10} ai_tt=[{}] status={}",
				move_count, analysis_log.completed_depth, analysis_log.nodes, analysis_log.tt,
				moves.empty() ? "-" : format_move(moves[0].move), moves.empty() ? 0 : moves[0].score, ai_log.completed_depth,
				ai_log.nodes, ai_log.tt, complete ? "OK" : "INCOMPLETE");

		if (!replay(board, 1, KIFU2 + move_count)) {
			return 1;
		}
	}
	double ms = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - start).count();

	std::println("GAME analysis_nodes={} ai_nodes={} time_ms={:.1f} tt_clears={} incomplete={}", analysis_nodes, ai_nodes, ms,
			tt_clears, incomplete);
	print_heap_size();
	return incomplete > 0 ? 1 : 0;
}

} // namespace

int main(int argc, char **argv) {
	if (argc < 2) {
		print_usage();
		return 2;
	}

	std::vector<uint8_t> zobrist = read_file(argv[1]);
	if (!BoardState::load_zobrist_params(zobrist)) {
		std::println("FAIL: zobrist params could not be loaded from {}", argv[1]);
		return 1;
	}

	std::string_view mode = argc >= 3 ? argv[2] : "depth";
	if (mode == "game") {
		if (argc < 5) {
			print_usage();
			return 2;
		}
		return run_game(std::atoi(argv[3]), std::atoi(argv[4]));
	}

	bool depth_mode = mode == "depth";
	if (!depth_mode && mode != "time") {
		print_usage();
		return 2;
	}
	int value = argc >= 4 ? std::atoi(argv[3]) : (depth_mode ? DEFAULT_DEPTH : DEFAULT_TIME_MSEC);
	return run_positions(depth_mode, value);
}
