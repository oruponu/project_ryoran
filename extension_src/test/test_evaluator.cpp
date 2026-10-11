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
	{ "kif2-30", nullptr, 30, -405, 54 },
	{ "kif2-59", nullptr, 59, 676, 3260 },
	{ "kif2-60", nullptr, 60, 752, 3336 },
	{ "kif2-69", nullptr, 69, 946, 6896 },
	{ "edge-kings", EDGE_KINGS_SFEN, 0, 858, 1978 },
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
