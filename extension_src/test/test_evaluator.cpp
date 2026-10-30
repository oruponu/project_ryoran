#include "evaluator.hpp"
#include "test_support.hpp"
#include <format>
#include <optional>

namespace {

const char *START_SFEN = "lnsgkgsnl/1r5b1/ppppppppp/9/9/9/PPPPPPPPP/1B5R1/LNSGKGSNL b - 1";
const char *EDGE_KINGS_SFEN = "kg6+R/p8/9/9/4+B4/9/9/7P1/+r6GK w S2Pn 1";

struct ScoreCase {
	const char *name;
	const char *sfen;
	int move_count;
	int spatial_score;
	int score;
};

const ScoreCase SCORE_CASES[] = {
	{ "start", START_SFEN, 0, 0, 0 },
	{ "kif2-30", nullptr, 30, -525, -66 },
	{ "kif2-59", nullptr, 59, 491, 3075 },
	{ "kif2-60", nullptr, 60, 567, 3151 },
	{ "kif2-69", nullptr, 69, 916, 6866 },
	{ "edge-kings", EDGE_KINGS_SFEN, 0, 858, 1978 },
	{ "pawn-boundary-sente", "4k4/7P1/9/9/9/9/9/9/4K4 b - 1", 0, 21, 102 },
	{ "pawn-boundary-gote", "4k2p1/9/9/9/9/9/9/9/4K4 b - 1", 0, -34, -115 },
	{ "pawn-last-rank-sente", "k3P4/9/9/9/9/9/9/9/4K4 b - 1", 0, -7, 74 },
	{ "pawn-last-rank-gote", "4k4/9/9/9/9/9/9/9/K3p4 b - 1", 0, -48, -129 },
	{ "tokin-sente", "4k4/9/2+P6/9/9/9/9/9/4K4 b - 1", 0, 375, 861 },
	{ "tokin-gote", "4k4/9/9/9/9/9/6+p2/9/4K4 b - 1", 0, -375, -861 },
};

std::optional<BoardState> make_board(const ScoreCase &score_case) {
	if (score_case.sfen != nullptr) {
		return BoardState::from_sfen(score_case.sfen);
	}
	BoardState board(Turn::SENTE);
	setup_initial_position(board);
	if (!replay(board, score_case.move_count, KIFU2)) {
		return std::nullopt;
	}
	return board;
}

} // namespace

void run_evaluator_tests() {
	for (const ScoreCase &score_case : SCORE_CASES) {
		std::optional<BoardState> board = make_board(score_case);
		check(board.has_value(), std::format("evaluator: {} position is set up", score_case.name).c_str());
		if (!board.has_value()) {
			continue;
		}
		check(Evaluator::calculate_spatial_score(*board) == score_case.spatial_score,
				std::format("evaluator: {} spatial score is unchanged", score_case.name).c_str());
		check(board->get_score() == score_case.score, std::format("evaluator: {} score is unchanged", score_case.name).c_str());
	}
}
