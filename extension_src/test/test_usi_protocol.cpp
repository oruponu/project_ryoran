#include "move_generator.hpp"
#include "test_support.hpp"
#include "usi_protocol.hpp"
#include <cstdint>
#include <initializer_list>
#include <optional>
#include <string>
#include <vector>

namespace {

constexpr const char *START_SFEN = "lnsgkgsnl/1r5b1/ppppppppp/9/9/9/PPPPPPPPP/1B5R1/LNSGKGSNL b - 1";
// After 7g7f 3c3d
constexpr const char *BISHOP_EXCHANGE_SFEN = "lnsgkgsnl/1r5b1/pppppp1pp/6p2/9/2P6/PP1PPPPPP/1B5R1/LNSGKGSNL b - 3";
constexpr const char *ROUND_TRIP_SFENS[] = {
	START_SFEN,
	"lnsg1g2l/1rk3s2/p1pppp1pp/6p2/2N6/1SP2PPP1/PPNPR3P/3g2S2/+p4K1NL b 2BLPg 41",
	"ln1g1g2l/1rks2s2/p1pp1p1pp/4+R1p2/2N6/1SP2PPP1/PPNP4P/3gg1S2/+p5KNL w 2BL2P 46",
};

void run_format_move_tests() {
	check(Usi::format_move(Move(2, 6, 2, 5, P, false, false, false)) == "7g7f", "usi: formats a board move");
	check(Usi::format_move(Move(1, 7, 7, 1, B, true, false, true)) == "8h2b+", "usi: formats a promotion");
	check(Usi::format_move(Move(0, 0, 4, 4, P, false, true, false)) == "P*5e", "usi: formats a drop");
}

void run_parse_move_tests() {
	std::optional<BoardState> board = BoardState::from_sfen(START_SFEN);
	check(board.has_value(), "usi: start position parses");
	if (!board.has_value()) {
		return;
	}

	std::optional<Move> move = Usi::parse_move(*board, "7g7f");
	check(move.has_value() && move->from_col == 2 && move->from_row == 6 && move->to_col == 2 && move->to_row == 5 &&
					!move->is_promotion && !move->is_drop,
			"usi: parses a legal board move");
	check(!Usi::parse_move(*board, "7g7e").has_value(), "usi: rejects an illegal board move");
	check(!Usi::parse_move(*board, "P*5e").has_value(), "usi: rejects a drop without the piece in hand");
	check(!Usi::parse_move(*board, "").has_value(), "usi: rejects an empty string");
	check(!Usi::parse_move(*board, "7g").has_value(), "usi: rejects a truncated move");
	check(!Usi::parse_move(*board, "z9z9").has_value(), "usi: rejects a broken move");

	std::optional<BoardState> bishop_exchange = BoardState::from_sfen(BISHOP_EXCHANGE_SFEN);
	check(bishop_exchange.has_value(), "usi: position before the bishop exchange parses");
	if (bishop_exchange.has_value()) {
		std::optional<Move> promotion = Usi::parse_move(*bishop_exchange, "8h2b+");
		std::optional<Move> no_promotion = Usi::parse_move(*bishop_exchange, "8h2b");
		check(promotion.has_value() && promotion->is_promotion, "usi: parses a promotion");
		check(no_promotion.has_value() && !no_promotion->is_promotion, "usi: parses a non-promotion");
	}

	std::optional<BoardState> midgame = BoardState::from_sfen(ROUND_TRIP_SFENS[1]);
	if (midgame.has_value()) {
		std::optional<Move> drop = Usi::parse_move(*midgame, "P*5e");
		check(drop.has_value() && drop->is_drop && drop->piece_type == P, "usi: parses a drop");
	}
}

void run_round_trip_tests() {
	bool all_parsed = true;
	bool all_round_trip = true;
	for (const char *sfen : ROUND_TRIP_SFENS) {
		std::optional<BoardState> board = BoardState::from_sfen(sfen);
		if (!board.has_value()) {
			all_parsed = false;
			continue;
		}
		Shogi::MoveList move_list;
		MoveGenerator::get_legal_moves(*board, move_list);
		for (const Move &move : move_list) {
			std::optional<Move> parsed = Usi::parse_move(*board, Usi::format_move(move));
			all_round_trip = all_round_trip && parsed.has_value() && *parsed == move;
		}
	}
	check(all_parsed, "usi: round trip positions parse");
	check(all_round_trip, "usi: every legal move survives format and parse");
}

void run_parse_position_tests() {
	std::optional<BoardState> start = BoardState::from_sfen(START_SFEN);
	std::optional<Usi::Position> startpos = Usi::parse_position("startpos");
	check(start.has_value() && startpos.has_value() &&
					startpos->board.get_zobrist_hash() == start->get_zobrist_hash() && startpos->history_hashes.empty() &&
					startpos->history_in_checks.empty(),
			"usi: startpos without moves");

	std::optional<Usi::Position> from_startpos = Usi::parse_position("startpos moves 7g7f 3c3d");
	std::optional<Usi::Position> from_sfen = Usi::parse_position(std::string("sfen ") + START_SFEN + " moves 7g7f 3c3d");
	check(from_startpos.has_value() && from_sfen.has_value() &&
					from_startpos->board.get_zobrist_hash() == from_sfen->board.get_zobrist_hash() &&
					from_startpos->board.get_turn_to_move() == from_sfen->board.get_turn_to_move(),
			"usi: startpos and sfen give the same position");

	std::optional<BoardState> bishop_exchange = BoardState::from_sfen(BISHOP_EXCHANGE_SFEN);
	check(from_startpos.has_value() && bishop_exchange.has_value() &&
					from_startpos->board.get_zobrist_hash() == bishop_exchange->get_zobrist_hash(),
			"usi: moves from startpos reach the bishop exchange position");

	if (start.has_value()) {
		BoardState board = *start;
		std::vector<uint64_t> expected_hashes;
		for (const char *text : { "7g7f", "3c3d", "8h2b+" }) {
			expected_hashes.push_back(board.get_zobrist_hash());
			std::optional<Move> move = Usi::parse_move(board, text);
			if (move.has_value()) {
				static_cast<void>(board.apply_move(*move));
			}
		}
		std::optional<Usi::Position> position = Usi::parse_position("startpos moves 7g7f 3c3d 8h2b+");
		check(position.has_value() && position->history_hashes == expected_hashes &&
						position->history_in_checks == std::vector<bool>{ false, false, false } &&
						position->board.get_zobrist_hash() == board.get_zobrist_hash(),
				"usi: history holds every position before the current one");
	}

	std::optional<Usi::Position> check_position = Usi::parse_position("sfen 4k4/9/9/9/9/9/9/9/4K4 b G 1 moves G*5b 5a5b");
	check(check_position.has_value() && check_position->history_in_checks == std::vector<bool>{ false, true },
			"usi: history records whether the side to move is in check");

	check(!Usi::parse_position("").has_value(), "usi: rejects an empty position");
	check(!Usi::parse_position("foo").has_value(), "usi: rejects an unknown position type");
	check(!Usi::parse_position("sfen bad").has_value(), "usi: rejects a broken sfen");
	check(!Usi::parse_position("startpos 7g7f").has_value(), "usi: rejects moves without the moves keyword");
	check(!Usi::parse_position("startpos moves 7g7f 7g7e").has_value(), "usi: rejects an illegal move in the list");
}

} // namespace

void run_usi_protocol_tests() {
	run_format_move_tests();
	run_parse_move_tests();
	run_round_trip_tests();
	run_parse_position_tests();
}
