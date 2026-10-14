import argparse
import datetime
import json
import os
import signal
import subprocess
import sys
import time
from pathlib import Path

import sprt
from play_pair import DRAW_MOVES

SELFPLAY_DIR = Path(__file__).resolve().parent
DEFAULT_OPENINGS = SELFPLAY_DIR / "openings.txt"
RESULTS_DIR = SELFPLAY_DIR / "results"
PLAY_PAIR = SELFPLAY_DIR / "play_pair.py"

DEFAULT_BYOYOMI_MSEC = 200
DEFAULT_WORKERS = 10
DEFAULT_ELO0 = 0.0
DEFAULT_ELO1 = 20.0
MAX_ABS_ELO = 1000
DEADLINE_SAFETY_FACTOR = 2
DEADLINE_MARGIN_SEC = 60
POLL_INTERVAL_SEC = 0.2


class PairError(Exception):
    pass


class Tally:
    def __init__(self):
        self.counts = [0] * len(sprt.PAIR_SCORES)
        self.wins = 0
        self.losses = 0
        self.draws = 0

    @property
    def pairs(self):
        return sum(self.counts)

    def add(self, result):
        self.counts[sprt.pair_index(result["engine1_won"], result["engine2_won"], result["draw"])] += 1
        self.wins += result["engine1_won"]
        self.losses += result["engine2_won"]
        self.draws += result["draw"]


def parse_byoyomi(values):
    if len(values) not in (1, 2):
        raise ValueError("--byoyomi takes one or two values")
    if min(values) < 1:
        raise ValueError("--byoyomi must be at least 1")
    return (values[0], values[-1])


def pair_deadline_sec(byoyomi):
    return sprt.GAMES_PER_PAIR * DRAW_MOVES * max(byoyomi) / 1000 * DEADLINE_SAFETY_FACTOR + DEADLINE_MARGIN_SEC


def count_openings(path):
    with open(path, encoding="utf-8") as file:
        return sum(1 for _ in file)


def resolve_max_pairs(requested, opening_count):
    max_pairs = opening_count if requested is None else requested
    if not 1 <= max_pairs <= opening_count:
        raise ValueError(f"--max-pairs must be between 1 and {opening_count} (the number of openings)")
    return max_pairs


def validate_elo_bounds(elo0, elo1):
    if not (-MAX_ABS_ELO <= elo0 < elo1 <= MAX_ABS_ELO):
        raise ValueError(f"--elo0 and --elo1 must satisfy -{MAX_ABS_ELO} <= elo0 < elo1 <= {MAX_ABS_ELO}")


def format_progress(tally, elo0, elo1):
    llr_value = sprt.llr(tally.counts, elo0, elo1)
    lower, upper = sprt.bounds()
    elo, low, high = sprt.elo_estimate(tally.counts)
    return (
        f"pairs={tally.pairs} W-L-D={tally.wins}-{tally.losses}-{tally.draws} "
        f"elo={elo:+.1f} [{low:+.1f}, {high:+.1f}] llr={llr_value:.2f} ({lower:.2f}, {upper:.2f})"
    )


def kill_tree(process):
    if process.poll() is not None:
        return
    if sys.platform == "win32":
        subprocess.run(["taskkill", "/T", "/F", "/PID", str(process.pid)], capture_output=True)
    else:
        os.killpg(process.pid, signal.SIGKILL)
    process.wait()


def start_pair(args, byoyomi, index, run_dir, play_pair):
    pair_dir = run_dir / f"pair-{index:04d}"
    pair_dir.mkdir()
    command = [
        sys.executable,
        str(play_pair),
        str(args.engine1),
        str(args.engine2),
        "--openings",
        str(args.openings),
        "--index",
        str(index),
        "--byoyomi",
        str(byoyomi[0]),
        str(byoyomi[1]),
        "--out-dir",
        str(pair_dir),
    ]
    with open(pair_dir / "stderr.txt", "w", encoding="utf-8") as stderr:
        process = subprocess.Popen(command, stdout=subprocess.DEVNULL, stderr=stderr, start_new_session=True)
    return process, time.monotonic(), pair_dir


def run(args, byoyomi, max_pairs, deadline_sec, play_pair=PLAY_PAIR, results_dir=RESULTS_DIR):
    run_dir = results_dir / datetime.datetime.now().strftime("%Y%m%d-%H%M%S-%f")
    run_dir.mkdir(parents=True)
    tally = Tally()
    running = {}
    finished = {}
    next_index = 0
    next_to_tally = 0
    decision = None
    with open(run_dir / "summary.txt", "w", encoding="utf-8") as summary:

        def report(line):
            print(line, flush=True)
            summary.write(line + "\n")
            summary.flush()

        report(
            f"engine1={args.engine1} engine2={args.engine2} byoyomi={byoyomi[0]},{byoyomi[1]} "
            f"elo0={args.elo0} elo1={args.elo1} max_pairs={max_pairs} results={run_dir}"
        )
        try:
            while decision is None and next_to_tally < max_pairs:
                while len(running) < args.workers and next_index < max_pairs:
                    running[next_index] = start_pair(args, byoyomi, next_index, run_dir, play_pair)
                    next_index += 1
                time.sleep(POLL_INTERVAL_SEC)
                for index, (process, started, pair_dir) in list(running.items()):
                    if process.poll() is None:
                        if time.monotonic() - started > deadline_sec:
                            raise PairError(f"pair {index} did not finish within {deadline_sec:.0f}s (see {pair_dir})")
                        continue
                    del running[index]
                    result_path = pair_dir / "result.json"
                    if process.returncode != 0 or not result_path.exists():
                        raise PairError(f"pair {index} failed with exit code {process.returncode} (see {pair_dir})")
                    finished[index] = json.loads(result_path.read_text(encoding="utf-8"))
                while decision is None and next_to_tally in finished:
                    tally.add(finished.pop(next_to_tally))
                    next_to_tally += 1
                    report(format_progress(tally, args.elo0, args.elo1))
                    decision = sprt.decide(sprt.llr(tally.counts, args.elo0, args.elo1))
        except PairError as error:
            report(f"error: {error}")
            return 1
        except KeyboardInterrupt:
            report("interrupted")
            return 130
        finally:
            for process, _, _ in running.values():
                kill_tree(process)
        report(f"result: {decision or 'undecided'} after {tally.pairs} pairs")
    return 0


def main(argv=None):
    parser = argparse.ArgumentParser(description="Play two USI engines against each other and judge with SPRT")
    parser.add_argument("engine1", type=Path, help="the new engine")
    parser.add_argument("engine2", type=Path, help="the baseline engine")
    parser.add_argument("--byoyomi", type=int, nargs="+", default=[DEFAULT_BYOYOMI_MSEC])
    parser.add_argument("--workers", type=int, default=DEFAULT_WORKERS)
    parser.add_argument("--elo0", type=float, default=DEFAULT_ELO0)
    parser.add_argument("--elo1", type=float, default=DEFAULT_ELO1)
    parser.add_argument("--max-pairs", type=int)
    parser.add_argument("--openings", type=Path, default=DEFAULT_OPENINGS)
    args = parser.parse_args(argv)

    try:
        args.engine1 = args.engine1.resolve()
        args.engine2 = args.engine2.resolve()
        args.openings = args.openings.resolve()
        for path in (args.engine1, args.engine2, args.openings):
            if not path.exists():
                raise ValueError(f"{path} does not exist")
        if args.workers < 1:
            raise ValueError("--workers must be at least 1")
        validate_elo_bounds(args.elo0, args.elo1)
        byoyomi = parse_byoyomi(args.byoyomi)
        max_pairs = resolve_max_pairs(args.max_pairs, count_openings(args.openings))
    except ValueError as error:
        print(f"error: {error}", file=sys.stderr)
        return 2
    return run(args, byoyomi, max_pairs, pair_deadline_sec(byoyomi))


if __name__ == "__main__":
    sys.exit(main())
