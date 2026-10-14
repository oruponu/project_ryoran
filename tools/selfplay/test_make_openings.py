import tempfile
import unittest
from pathlib import Path

import cshogi

import make_openings


class ParseInfoLinesTest(unittest.TestCase):
    def test_orders_by_multipv(self):
        lines = [
            "go byoyomi 200",
            "info multipv 2 score cp -30 pv 2g2f",
            "info multipv 1 score cp 45 pv 7g7f",
            "bestmove 7g7f",
        ]
        self.assertEqual(make_openings.parse_info_lines(lines), [("7g7f", 45), ("2g2f", -30)])

    def test_ignores_other_lines(self):
        self.assertEqual(make_openings.parse_info_lines(["readyok", "bestmove resign"]), [])


class PositionKeyTest(unittest.TestCase):
    def test_drops_move_number(self):
        board = cshogi.Board()
        board.push_usi("7g7f")
        self.assertEqual(
            make_openings.position_key(board), "lnsgkgsnl/1r5b1/ppppppppp/9/9/2P6/PP1PPPPPP/1B5R1/LNSGKGSNL w -"
        )


@unittest.skipUnless(make_openings.DEFAULT_ENGINE.exists(), f"{make_openings.DEFAULT_ENGINE} is not built")
class GenerateTest(unittest.TestCase):
    def test_writes_legal_openings(self):
        with tempfile.TemporaryDirectory() as directory:
            output = Path(directory) / "openings.txt"
            self.assertEqual(make_openings.main(["--count", "2", "--workers", "2", "--output", str(output)]), 0)
            lines = output.read_text(encoding="utf-8").splitlines()
        self.assertEqual(len(lines), 2)
        keys = set()
        for line in lines:
            self.assertTrue(line.startswith("startpos moves "))
            moves = line[len("startpos moves "):].split()
            self.assertEqual(len(moves), make_openings.OPENING_PLIES)
            board = cshogi.Board()
            for move in moves:
                self.assertTrue(board.is_legal(board.move_from_usi(move)))
                board.push_usi(move)
            keys.add(make_openings.position_key(board))
        self.assertEqual(len(keys), 2)


if __name__ == "__main__":
    unittest.main()
