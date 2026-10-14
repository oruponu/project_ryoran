import argparse
import subprocess
import sys
import tempfile
import textwrap
import unittest
from pathlib import Path

import match
import sprt

FAKE_PAIR_PRELUDE = """\
import json
import subprocess
import sys
import time
from pathlib import Path

out_dir = Path(sys.argv[sys.argv.index("--out-dir") + 1])
index = int(sys.argv[sys.argv.index("--index") + 1])
"""
HANG_WITH_CHILD = """\
def hang_with_child():
    child = subprocess.Popen([sys.executable, "-c", "import time; time.sleep(60)"])
    (out_dir / "child.pid").write_text(str(child.pid))
    time.sleep(60)
"""
WIN_UNTIL_DECISION_THEN_HANG = HANG_WITH_CHILD + """\
if index < DECISIVE_PAIRS:
    if index == DECISIVE_PAIRS - 1:
        time.sleep(1)
    (out_dir / "result.json").write_text(json.dumps({"engine1_won": 2, "engine2_won": 0, "draw": 0}))
else:
    hang_with_child()
"""
FIRST_FAILS_WHILE_OTHERS_HANG = HANG_WITH_CHILD + """\
if index == 0:
    time.sleep(1)
    sys.exit(1)
hang_with_child()
"""
FIRST_PAIR_LOSES_LATE = """\
if index == 0:
    time.sleep(1)
    result = {"engine1_won": 0, "engine2_won": 2, "draw": 0}
else:
    result = {"engine1_won": 2, "engine2_won": 0, "draw": 0}
(out_dir / "result.json").write_text(json.dumps(result))
"""
HANGING_PAIR_WITH_CHILD = HANG_WITH_CHILD + """\
hang_with_child()
"""


def process_alive(pid):
    output = subprocess.run(["tasklist", "/FI", f"PID eq {pid}", "/NH"], capture_output=True, text=True).stdout
    return str(pid) in output.split()


def child_pids(run_dir):
    return [int(path.read_text()) for path in run_dir.glob("pair-*/child.pid")]


def decisive_pairs():
    return next(k for k in range(1, 1000) if sprt.decide(sprt.llr([0, 0, 0, 0, k], 0.0, 20.0)) == "H1")


def run_fake(directory, body, max_pairs, workers, deadline_sec=60.0):
    directory = Path(directory)
    fake_pair = directory / "fake_pair.py"
    fake_pair.write_text(FAKE_PAIR_PRELUDE + textwrap.dedent(body), encoding="utf-8")
    openings = directory / "openings.txt"
    openings.write_text("startpos moves 7g7f\n" * max_pairs, encoding="utf-8")
    args = argparse.Namespace(
        engine1=Path("engine1.exe"), engine2=Path("engine2.exe"), openings=openings, workers=workers, elo0=0.0, elo1=20.0
    )
    results_dir = directory / "results"
    code = match.run(args, (50, 50), max_pairs, deadline_sec, play_pair=fake_pair, results_dir=results_dir)
    (run_dir,) = results_dir.iterdir()
    return code, run_dir, (run_dir / "summary.txt").read_text(encoding="utf-8").splitlines()


class ParseByoyomiTest(unittest.TestCase):
    def test_one_value_is_shared(self):
        self.assertEqual(match.parse_byoyomi([200]), (200, 200))

    def test_two_values(self):
        self.assertEqual(match.parse_byoyomi([1000, 100]), (1000, 100))

    def test_rejects_three_values(self):
        with self.assertRaises(ValueError):
            match.parse_byoyomi([1, 2, 3])

    def test_rejects_negative(self):
        with self.assertRaises(ValueError):
            match.parse_byoyomi([-1])

    def test_rejects_zero(self):
        with self.assertRaises(ValueError):
            match.parse_byoyomi([0])
        with self.assertRaises(ValueError):
            match.parse_byoyomi([200, 0])


class PairDeadlineTest(unittest.TestCase):
    def test_default_byoyomi(self):
        self.assertAlmostEqual(match.pair_deadline_sec((200, 200)), 264.8)

    def test_uses_longer_byoyomi(self):
        self.assertAlmostEqual(match.pair_deadline_sec((1000, 100)), 1084.0)


class CountOpeningsTest(unittest.TestCase):
    def test_counts_every_line_like_cshogi(self):
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "openings.txt"
            path.write_text("startpos moves 7g7f\n\nstartpos moves 2g2f\n", encoding="utf-8")
            self.assertEqual(match.count_openings(path), 3)


class ResolveMaxPairsTest(unittest.TestCase):
    def test_defaults_to_opening_count(self):
        self.assertEqual(match.resolve_max_pairs(None, 10), 10)

    def test_accepts_smaller_value(self):
        self.assertEqual(match.resolve_max_pairs(5, 10), 5)

    def test_rejects_more_than_openings(self):
        with self.assertRaises(ValueError):
            match.resolve_max_pairs(11, 10)

    def test_rejects_zero(self):
        with self.assertRaises(ValueError):
            match.resolve_max_pairs(0, 10)


class ValidateEloBoundsTest(unittest.TestCase):
    def test_accepts_increasing_bounds(self):
        match.validate_elo_bounds(0.0, 20.0)
        match.validate_elo_bounds(-1000.0, 1000.0)

    def test_rejects_equal_or_reversed_bounds(self):
        for elo0, elo1 in ((10.0, 10.0), (20.0, 0.0)):
            with self.assertRaises(ValueError):
                match.validate_elo_bounds(elo0, elo1)

    def test_rejects_out_of_range_bounds(self):
        for elo0, elo1 in ((-1001.0, 0.0), (0.0, 1001.0), (-200000.0, 0.0)):
            with self.assertRaises(ValueError):
                match.validate_elo_bounds(elo0, elo1)

    def test_rejects_non_finite_bounds(self):
        for elo0, elo1 in ((float("nan"), 20.0), (0.0, float("inf"))):
            with self.assertRaises(ValueError):
                match.validate_elo_bounds(elo0, elo1)


class TallyTest(unittest.TestCase):
    def test_adds_pairs(self):
        tally = match.Tally()
        tally.add({"engine1_won": 2, "engine2_won": 0, "draw": 0})
        tally.add({"engine1_won": 1, "engine2_won": 0, "draw": 1})
        self.assertEqual(tally.counts, [0, 0, 0, 1, 1])
        self.assertEqual((tally.wins, tally.losses, tally.draws, tally.pairs), (3, 0, 1, 2))

    def test_rejects_incomplete_pair(self):
        with self.assertRaises(ValueError):
            match.Tally().add({"engine1_won": 1, "engine2_won": 0, "draw": 0})


class FormatProgressTest(unittest.TestCase):
    def test_line(self):
        tally = match.Tally()
        tally.add({"engine1_won": 2, "engine2_won": 0, "draw": 0})
        tally.add({"engine1_won": 1, "engine2_won": 1, "draw": 0})
        line = match.format_progress(tally, 0.0, 20.0)
        self.assertTrue(line.startswith("pairs=2 W-L-D=3-1-0 elo=+"))
        self.assertIn(" llr=", line)
        self.assertTrue(line.endswith("(-2.94, 2.94)"))


@unittest.skipUnless(sys.platform == "win32", "process checks use tasklist")
class RunTest(unittest.TestCase):
    def test_decision_stops_run_and_kills_running_pairs(self):
        pairs = decisive_pairs()
        body = f"DECISIVE_PAIRS = {pairs}\n" + WIN_UNTIL_DECISION_THEN_HANG
        with tempfile.TemporaryDirectory(ignore_cleanup_errors=True) as directory:
            code, run_dir, summary = run_fake(directory, body, max_pairs=200, workers=4)
            pids = child_pids(run_dir)
        self.assertEqual(code, 0)
        self.assertEqual(summary[-1], f"result: H1 after {pairs} pairs")
        self.assertTrue(pids)
        self.assertFalse(any(process_alive(pid) for pid in pids))

    def test_tallies_in_opening_order(self):
        with tempfile.TemporaryDirectory(ignore_cleanup_errors=True) as directory:
            code, _, summary = run_fake(directory, FIRST_PAIR_LOSES_LATE, max_pairs=3, workers=3)
        progress = [line for line in summary if line.startswith("pairs=")]
        self.assertEqual(code, 0)
        self.assertTrue(progress[0].startswith("pairs=1 W-L-D=0-2-0 "))
        self.assertEqual(summary[-1], "result: undecided after 3 pairs")

    def test_failed_pair_stops_run_and_kills_running_pairs(self):
        with tempfile.TemporaryDirectory(ignore_cleanup_errors=True) as directory:
            code, run_dir, summary = run_fake(directory, FIRST_FAILS_WHILE_OTHERS_HANG, max_pairs=3, workers=3)
            pids = child_pids(run_dir)
        self.assertEqual(code, 1)
        self.assertRegex(summary[-1], r"^error: pair 0 failed with exit code 1 ")
        self.assertEqual(len(pids), 2)
        self.assertFalse(any(process_alive(pid) for pid in pids))

    def test_deadline_kills_descendants(self):
        with tempfile.TemporaryDirectory(ignore_cleanup_errors=True) as directory:
            code, run_dir, summary = run_fake(directory, HANGING_PAIR_WITH_CHILD, max_pairs=1, workers=1, deadline_sec=2.0)
            child_pid = int((run_dir / "pair-0000" / "child.pid").read_text())
        self.assertEqual(code, 1)
        self.assertRegex(summary[-1], r"^error: pair 0 did not finish within 2s ")
        self.assertFalse(process_alive(child_pid))


if __name__ == "__main__":
    unittest.main()
