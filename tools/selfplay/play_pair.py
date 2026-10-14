import argparse
import contextlib
import json
import sys
from pathlib import Path

from cshogi import cli

from sprt import GAMES_PER_PAIR

DRAW_MOVES = 256
OPENING_MOVES_LIMIT = 256
ENGINE_NAMES = ["engine1", "engine2"]


class EofCheckingStdout:
    def __init__(self, stream):
        self._stream = stream

    def readline(self):
        line = self._stream.readline()
        if not line:
            raise EOFError("engine closed its output")
        return line

    def __getattr__(self, name):
        return getattr(self._stream, name)


# cshogi 1.0.9 compares bytes with '' and keeps reading after the engine exits
class EofCheckingEngine(cli.Engine):
    @property
    def proc(self):
        return self._proc

    @proc.setter
    def proc(self, process):
        if process is not None:
            process.stdout = EofCheckingStdout(process.stdout)
        self._proc = process


cli.Engine = EofCheckingEngine


def main(argv=None):
    parser = argparse.ArgumentParser(description="Play one pair of games from an opening with colors swapped")
    parser.add_argument("engine1")
    parser.add_argument("engine2")
    parser.add_argument("--openings", required=True)
    parser.add_argument("--index", type=int, required=True)
    parser.add_argument("--byoyomi", type=int, nargs=2, required=True)
    parser.add_argument("--out-dir", type=Path, required=True)
    args = parser.parse_args(argv)

    args.out_dir.mkdir(parents=True, exist_ok=True)
    with open(args.out_dir / "log.txt", "w", encoding="utf-8") as log, contextlib.redirect_stdout(log):
        result = cli.main(
            args.engine1,
            args.engine2,
            names=ENGINE_NAMES,
            games=GAMES_PER_PAIR,
            byoyomi=args.byoyomi,
            draw=DRAW_MOVES,
            opening=args.openings,
            opening_moves=OPENING_MOVES_LIMIT,
            opening_index=args.index,
            csa=str(args.out_dir),
            print_summary=False,
        )
    summary = {key: result[key] for key in ("engine1_won", "engine2_won", "draw")}
    (args.out_dir / "result.json").write_text(json.dumps(summary), encoding="utf-8")
    return 0


if __name__ == "__main__":
    sys.exit(main())
