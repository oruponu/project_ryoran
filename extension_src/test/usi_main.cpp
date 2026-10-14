#include "ai_player.hpp"
#include "board_state.hpp"
#include "usi_protocol.hpp"
#include <algorithm>
#include <charconv>
#include <cstdint>
#include <cstdio>
#include <filesystem>
#include <format>
#include <fstream>
#include <iostream>
#include <iterator>
#include <memory>
#include <optional>
#include <print>
#include <string>
#include <string_view>
#include <system_error>
#include <vector>

#ifndef RYORAN_BUILD_NAME
#define RYORAN_BUILD_NAME "dev"
#endif

namespace {

constexpr int MAX_MULTI_PV = 10;
constexpr uint64_t DEFAULT_BYOYOMI_MSEC = 1000;

void send(const std::string &line) {
	std::println("{}", line);
	std::fflush(stdout);
}

bool load_zobrist(const std::filesystem::path &path) {
	std::ifstream file(path, std::ios::binary);
	if (!file) {
		return false;
	}
	std::vector<uint8_t> data((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
	return BoardState::load_zobrist_params(data);
}

std::optional<int> parse_int(std::string_view text) {
	int value = 0;
	auto [ptr, ec] = std::from_chars(text.data(), text.data() + text.size(), value);
	if (ec != std::errc() || ptr != text.data() + text.size()) {
		return std::nullopt;
	}
	return value;
}

void set_option(const std::vector<std::string_view> &tokens, int &multi_pv) {
	if (tokens.size() == 5 && tokens[1] == "name" && tokens[2] == "MultiPV" && tokens[3] == "value") {
		if (std::optional<int> value = parse_int(tokens[4]); value.has_value()) {
			multi_pv = std::clamp(*value, 1, MAX_MULTI_PV);
		}
	}
}

uint64_t parse_byoyomi(const std::vector<std::string_view> &tokens) {
	for (size_t i = 1; i + 1 < tokens.size(); ++i) {
		if (tokens[i] == "byoyomi") {
			if (std::optional<int> value = parse_int(tokens[i + 1]); value.has_value() && *value >= 0) {
				return static_cast<uint64_t>(*value);
			}
		}
	}
	return DEFAULT_BYOYOMI_MSEC;
}

void go(AIPlayer &ai, const Usi::Position &position, int multi_pv, uint64_t byoyomi_msec) {
	ai.set_game_history(position.history_hashes, position.history_in_checks);
	ai.set_time_limit_usec(byoyomi_msec * 1000);
	std::vector<ScoredMove> moves = ai.search_top_moves(position.board, multi_pv);
	if (moves.empty()) {
		send("bestmove resign");
		return;
	}
	for (size_t i = 0; i < moves.size(); ++i) {
		send(std::format("info multipv {} score cp {} pv {}", i + 1, moves[i].score, Usi::format_move(moves[i].move)));
	}
	send(std::format("bestmove {}", Usi::format_move(moves[0].move)));
}

} // namespace

int main(int argc, char **argv) {
	std::filesystem::path zobrist_path = argc >= 2 ? std::filesystem::path(argv[1])
												   : std::filesystem::path(argv[0]).parent_path() / "zobrist_params.bin";
	bool zobrist_loaded = false;
	int multi_pv = 1;
	auto ai = std::make_unique<AIPlayer>();
	std::optional<Usi::Position> position;

	std::string line;
	while (std::getline(std::cin, line)) {
		std::vector<std::string_view> tokens = Usi::split_tokens(line);
		if (tokens.empty()) {
			continue;
		}
		std::string_view command = tokens[0];
		if (command == "usi") {
			send(std::format("id name ryoran {}", RYORAN_BUILD_NAME));
			send("id author oruponu");
			send(std::format("option name MultiPV type spin default 1 min 1 max {}", MAX_MULTI_PV));
			send("usiok");
		} else if (command == "isready") {
			if (!zobrist_loaded) {
				zobrist_loaded = load_zobrist(zobrist_path);
				if (!zobrist_loaded) {
					std::println(stderr, "error: zobrist params could not be loaded from {}", zobrist_path.string());
					return 1;
				}
			}
			send("readyok");
		} else if (command == "setoption") {
			set_option(tokens, multi_pv);
		} else if (command == "usinewgame") {
			ai = std::make_unique<AIPlayer>();
		} else if (command == "position") {
			std::string_view args = std::string_view(line).substr(
					static_cast<size_t>(command.data() + command.size() - line.data()));
			position = Usi::parse_position(args);
			if (!position.has_value()) {
				std::println(stderr, "error: invalid position: {}", line);
				return 1;
			}
		} else if (command == "go") {
			if (!position.has_value()) {
				std::println(stderr, "error: go before position");
				return 1;
			}
			go(*ai, *position, multi_pv, parse_byoyomi(tokens));
		} else if (command == "quit") {
			break;
		}
	}
	return 0;
}
