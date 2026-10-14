import os
import subprocess
import unittest
from pathlib import Path

import cshogi

REPO = Path(__file__).resolve().parents[2]
DEFAULT_ENGINE = REPO / "extension_src" / "build" / "test" / "shogi_usi.exe"
ENGINE = Path(os.environ.get("SHOGI_USI", DEFAULT_ENGINE)).resolve()
TIMEOUT_SEC = 30


def run_engine(commands):
    return subprocess.run(
        [str(ENGINE)],
        input="\n".join(commands) + "\n",
        capture_output=True,
        text=True,
        timeout=TIMEOUT_SEC,
        cwd=ENGINE.parent,
    )


@unittest.skipUnless(ENGINE.exists(), f"{ENGINE} is not built")
class UsiEngineTest(unittest.TestCase):
    def test_handshake(self):
        result = run_engine(["usi", "isready", "quit"])
        lines = result.stdout.splitlines()
        self.assertEqual(result.returncode, 0)
        self.assertTrue(lines[0].startswith("id name ryoran "))
        self.assertIn("usiok", lines)
        self.assertEqual(lines[-1], "readyok")

    def test_bestmove_is_legal(self):
        result = run_engine(
            ["usi", "isready", "usinewgame", "position startpos moves 7g7f", "go byoyomi 100", "quit"]
        )
        self.assertEqual(result.returncode, 0)
        bestmoves = [line for line in result.stdout.splitlines() if line.startswith("bestmove ")]
        self.assertEqual(len(bestmoves), 1)
        board = cshogi.Board()
        board.push_usi("7g7f")
        self.assertTrue(board.is_legal(board.move_from_usi(bestmoves[0].split()[1])))

    def test_multipv_reports_each_candidate(self):
        result = run_engine(
            ["usi", "setoption name MultiPV value 3", "isready", "position startpos", "go byoyomi 100", "quit"]
        )
        infos = [line.split() for line in result.stdout.splitlines() if line.startswith("info multipv ")]
        self.assertEqual([tokens[2] for tokens in infos], ["1", "2", "3"])
        self.assertTrue(all(tokens[3:5] == ["score", "cp"] and tokens[6] == "pv" for tokens in infos))

    def test_resigns_when_checkmated(self):
        result = run_engine(
            ["usi", "isready", "position sfen 4k4/4G4/4P4/9/9/9/9/9/4K4 w - 1", "go byoyomi 100", "quit"]
        )
        self.assertIn("bestmove resign", result.stdout.splitlines())

    def test_exits_on_illegal_position(self):
        result = run_engine(["usi", "isready", "position startpos moves 7g7e", "go byoyomi 100", "quit"])
        self.assertEqual(result.returncode, 1)
        self.assertNotIn("bestmove", result.stdout)


if __name__ == "__main__":
    unittest.main()
