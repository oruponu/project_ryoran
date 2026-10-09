#include "test_support.hpp"
#include <cstdio>

int main(int argc, char **argv) {
	if (argc < 2) {
		std::printf("usage: shogi_tests <zobrist_params.bin>\n");
		return 2;
	}

	std::vector<uint8_t> zobrist = read_file(argv[1]);
	run_zobrist_tests(zobrist);
	if (!BoardState::zobrist_initialized()) {
		std::printf("FAIL: zobrist params could not be loaded from %s\n", argv[1]);
		return 1;
	}

	run_search_tests();
	run_rules_tests();

	std::printf("%s (%d failures)\n", failure_count() == 0 ? "ALL PASS" : "SOME FAILED", failure_count());
	return failure_count() == 0 ? 0 : 1;
}
