import argparse
import multiprocessing
import random
import sys
from pathlib import Path

import cshogi
from cshogi.usi import Engine

SELFPLAY_DIR = Path(__file__).resolve().parent
REPO = SELFPLAY_DIR.parents[1]
DEFAULT_ENGINE = REPO / "extension_src" / "build" / "test" / "shogi_usi.exe"
DEFAULT_OUTPUT = SELFPLAY_DIR / "openings.txt"

DEFAULT_COUNT = 3000
DEFAULT_WORKERS = 10
DEFAULT_SEED = 1
MULTI_PV = 3
OPENING_PLIES = 16
STEP_BYOYOMI_MSEC = 200
FINAL_BYOYOMI_MSEC = 1000
CANDIDATE_MARGIN = 150
MAX_ABS_SCORE = 290
MAX_ATTEMPTS_PER_OPENING = 10
SEED_STRIDE = 1_000_003
PROGRESS_INTERVAL = 100

_engine = None


def parse_info_lines(lines):
    candidates = {}
    for line in lines:
        tokens = line.split()
        if (
            len(tokens) >= 8
            and tokens[0] == "info"
            and tokens[1] == "multipv"
            and tokens[3:5] == ["score", "cp"]
            and tokens[6] == "pv"
        ):
            candidates[int(tokens[2])] = (tokens[7], int(tokens[5]))
    return [candidates[k] for k in sorted(candidates)]


def position_key(board):
    return " ".join(board.sfen().split()[:3])


def search(engine, moves, byoyomi):
    lines = []
    engine.position(moves)
    bestmove, _ = engine.go(byoyomi=byoyomi, listener=lines.append)
    if bestmove == "resign":
        return []
    return parse_info_lines(lines)


def generate_opening(engine, rng):
    board = cshogi.Board()
    moves = []
    engine.usinewgame()
    for _ in range(OPENING_PLIES):
        candidates = search(engine, moves, STEP_BYOYOMI_MSEC)
        if not candidates:
            return None
        best_score = candidates[0][1]
        move = rng.choice([m for m, score in candidates if score >= best_score - CANDIDATE_MARGIN])
        board.push_usi(move)
        moves.append(move)
        if board.is_game_over() or board.is_draw() != cshogi.NOT_REPETITION:
            return None
    final = search(engine, moves, FINAL_BYOYOMI_MSEC)
    if not final or abs(final[0][1]) > MAX_ABS_SCORE:
        return None
    return moves, position_key(board)


def init_worker(engine_path):
    global _engine
    _engine = Engine(str(engine_path))
    _engine.setoption("MultiPV", MULTI_PV)
    _engine.isready()


def generate_with_seed(seed):
    return generate_opening(_engine, random.Random(seed))


def main(argv=None):
    parser = argparse.ArgumentParser(description="Generate the opening suite for self-play")
    parser.add_argument("--engine", type=Path, default=DEFAULT_ENGINE)
    parser.add_argument("--count", type=int, default=DEFAULT_COUNT)
    parser.add_argument("--workers", type=int, default=DEFAULT_WORKERS)
    parser.add_argument("--seed", type=int, default=DEFAULT_SEED)
    parser.add_argument("--output", type=Path, default=DEFAULT_OUTPUT)
    args = parser.parse_args(argv)

    engine = args.engine.resolve()
    if not engine.exists():
        print(f"error: {engine} does not exist", file=sys.stderr)
        return 2

    openings = {}
    seeds = range(args.seed * SEED_STRIDE, args.seed * SEED_STRIDE + args.count * MAX_ATTEMPTS_PER_OPENING)
    with multiprocessing.Pool(args.workers, initializer=init_worker, initargs=(engine,)) as pool:
        for result in pool.imap_unordered(generate_with_seed, seeds):
            if result is None:
                continue
            moves, key = result
            if key in openings:
                continue
            openings[key] = moves
            if len(openings) % PROGRESS_INTERVAL == 0:
                print(f"{len(openings)} / {args.count}", flush=True)
            if len(openings) >= args.count:
                break
        pool.terminate()

    if len(openings) < args.count:
        print(f"error: only {len(openings)} openings after {len(seeds)} attempts", file=sys.stderr)
        return 1

    lines = ["startpos moves " + " ".join(moves) for _, moves in sorted(openings.items())]
    random.Random(args.seed).shuffle(lines)
    args.output.write_text("\n".join(lines[: args.count]) + "\n", encoding="utf-8")
    print(f"wrote {args.count} openings to {args.output}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
