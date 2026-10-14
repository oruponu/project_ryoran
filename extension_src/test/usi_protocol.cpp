#include "usi_protocol.hpp"
#include "move_generator.hpp"
#include <format>
#include <string>
#include <utility>

namespace Usi {

namespace {

constexpr std::string_view WHITESPACE = " \t\r\n";
constexpr std::string_view PIECE_CHARS = "KRBGSNLP";
constexpr size_t SFEN_FIELD_COUNT = 4;

std::string format_square(int col, int row) {
	return std::format("{}{}", 9 - col, static_cast<char>('a' + row));
}

} // namespace

std::vector<std::string_view> split_tokens(std::string_view text) {
	std::vector<std::string_view> tokens;
	size_t pos = text.find_first_not_of(WHITESPACE);
	while (pos != std::string_view::npos) {
		size_t end = text.find_first_of(WHITESPACE, pos);
		if (end == std::string_view::npos) {
			end = text.size();
		}
		tokens.push_back(text.substr(pos, end - pos));
		pos = text.find_first_not_of(WHITESPACE, end);
	}
	return tokens;
}

std::string format_move(const Shogi::Move &move) {
	if (move.is_drop) {
		return std::format("{}*{}", PIECE_CHARS[std::to_underlying(move.piece_type)], format_square(move.to_col, move.to_row));
	}
	return std::format("{}{}{}", format_square(move.from_col, move.from_row), format_square(move.to_col, move.to_row),
			move.is_promotion ? "+" : "");
}

std::optional<Shogi::Move> parse_move(BoardState &board, std::string_view text) {
	Shogi::MoveList move_list;
	MoveGenerator::get_legal_moves(board, move_list);
	for (int i = 0; i < move_list.count; ++i) {
		if (format_move(move_list[i]) == text) {
			return move_list[i];
		}
	}
	return std::nullopt;
}

std::optional<Position> parse_position(std::string_view args) {
	std::vector<std::string_view> tokens = split_tokens(args);
	size_t index = 0;
	std::string sfen;
	if (index < tokens.size() && tokens[index] == "startpos") {
		sfen = STARTPOS_SFEN;
		++index;
	} else if (index < tokens.size() && tokens[index] == "sfen" && tokens.size() >= index + 1 + SFEN_FIELD_COUNT) {
		for (size_t field = 0; field < SFEN_FIELD_COUNT; ++field) {
			if (field > 0) {
				sfen += ' ';
			}
			sfen += tokens[index + 1 + field];
		}
		index += 1 + SFEN_FIELD_COUNT;
	} else {
		return std::nullopt;
	}

	std::optional<BoardState> board = BoardState::from_sfen(sfen);
	if (!board.has_value()) {
		return std::nullopt;
	}
	Position position{ .board = *board };
	if (index == tokens.size()) {
		return position;
	}
	if (tokens[index] != "moves") {
		return std::nullopt;
	}
	for (++index; index < tokens.size(); ++index) {
		std::optional<Shogi::Move> move = parse_move(position.board, tokens[index]);
		if (!move.has_value()) {
			return std::nullopt;
		}
		position.history_hashes.push_back(position.board.get_zobrist_hash());
		position.history_in_checks.push_back(
				MoveGenerator::is_king_in_check(position.board, position.board.get_turn_to_move()));
		static_cast<void>(position.board.apply_move(*move));
	}
	return position;
}

} // namespace Usi
