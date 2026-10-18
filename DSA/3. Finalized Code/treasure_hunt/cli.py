"""TreasureHuntCLI: the console interface.

    python main.py train    [options]     train an agent (optionally save it)
    python main.py evaluate --model FILE  check a saved agent wins from every cell
    python main.py play     --model FILE  play one game and show the path

Exit codes: 0 success (trained to 100%, check passed, game won);
1 finished but unsuccessful (epoch limit reached, check failed, game not won);
2 bad arguments, settings or files.

TensorFlow is imported only when a command actually runs, so --help and
argument errors are instant.
"""
from __future__ import annotations

import argparse
import os
import sys
from pathlib import Path
from typing import List, Optional

from .config import ConfigError, TrainingConfig, load_maze

os.environ.setdefault("TF_CPP_MIN_LOG_LEVEL", "2")  # hide TensorFlow's info messages


class TreasureHuntCLI:
    """Parses the command line, builds the objects for a command, runs it and
    returns an exit code."""

    # ------------------------------------------------------------------ parser
    def build_parser(self) -> argparse.ArgumentParser:
        parser = argparse.ArgumentParser(
            prog="main.py",
            description="Deep Q-learning treasure hunt: train a pirate to reach the treasure.",
        )
        commands = parser.add_subparsers(dest="command", required=True, metavar="command")

        train = commands.add_parser(
            "train", help="train an agent",
            description="Train an agent. Unset options use the defaults shown.")
        train.add_argument("--epochs", type=int, help="maximum epochs (default 1000)")
        train.add_argument("--max-memory", type=int,
                           help="replay memory size (default: 8 x maze cells, 512 for the 8x8 maze)")
        train.add_argument("--batch-size", type=int, help="training batch size (default 32)")
        train.add_argument("--target-update-freq", type=int,
                           help="epochs between target network refreshes (default 50)")
        train.add_argument("--epsilon-start", type=float,
                           help="starting exploration rate (default 1.0)")
        train.add_argument("--epsilon-min", type=float,
                           help="lowest exploration rate (default 0.05)")
        train.add_argument("--epsilon-decay", type=float,
                           help="exploration decay per epoch (default 0.995)")
        train.add_argument("--seed", type=int, help="seed for a repeatable run")
        train.add_argument("--maze-file", help="JSON maze file (default: built-in 8x8 maze)")
        train.add_argument("--load-model", help="continue training from a saved model")
        train.add_argument("--save-model", help="save the trained model here (.keras or .h5)")
        train.add_argument("--log-file", help="write per-epoch results to this CSV file")
        train.add_argument("--log-every", type=int,
                           help="print a progress line every N epochs (default 1)")
        train.set_defaults(handler=self._train)

        evaluate = commands.add_parser(
            "evaluate", help="check a saved model from every start cell")
        evaluate.add_argument("--model", required=True, help="saved model file")
        evaluate.add_argument("--maze-file", help="JSON maze file (default: built-in 8x8 maze)")
        evaluate.set_defaults(handler=self._evaluate)

        play = commands.add_parser("play", help="play one game and show the path")
        play.add_argument("--model", required=True, help="saved model file")
        play.add_argument("--start", nargs=2, type=int, default=[0, 0], metavar=("ROW", "COL"),
                          help="start cell (default 0 0)")
        play.add_argument("--maze-file", help="JSON maze file (default: built-in 8x8 maze)")
        play.add_argument("--save-image", metavar="FILE", help="save the final board as an image")
        play.add_argument("--show", action="store_true", help="open the final board in a window")
        play.set_defaults(handler=self._play)
        return parser

    # ----------------------------------------------------------------- helpers
    @staticmethod
    def config_from_args(args: argparse.Namespace) -> TrainingConfig:
        """Only options the user gave are passed on, so defaults live in one place."""
        given = {
            "n_epoch": args.epochs,
            "max_memory": args.max_memory,
            "data_size": args.batch_size,
            "target_update_freq": args.target_update_freq,
            "epsilon": args.epsilon_start,
            "epsilon_min": args.epsilon_min,
            "epsilon_decay": args.epsilon_decay,
            "seed": args.seed,
            "maze_file": args.maze_file,
            "load_model": args.load_model,
            "save_model": args.save_model,
            "log_file": args.log_file,
            "log_every": args.log_every,
        }
        return TrainingConfig(**{key: value for key, value in given.items() if value is not None})

    @staticmethod
    def _require_model_file(path: str) -> None:
        """Fail fast, before the slow TensorFlow import, when the model file is missing."""
        if not Path(path).is_file():
            raise ConfigError(f"Model file not found: {path}")

    @staticmethod
    def _describe_maze(maze_file: Optional[str], maze) -> str:
        rows, cols = maze.shape
        return f"{rows}x{cols} ({maze_file if maze_file else 'built-in default'})"

    # ---------------------------------------------------------------- commands
    def _train(self, args: argparse.Namespace) -> int:
        config = self.config_from_args(args)      # validates every setting
        maze = config.load_maze()                 # validates the maze
        if config.load_model:
            self._require_model_file(config.load_model)
        if config.save_model:
            Path(config.save_model).parent.mkdir(parents=True, exist_ok=True)

        from .agent import DQNAgent, set_seed
        from .trainer import Trainer, format_time

        set_seed(config.seed)
        agent = DQNAgent.from_config(config, state_size=maze.size)
        trainer = Trainer(agent, maze, config)

        print(f"Maze: {self._describe_maze(config.maze_file, maze)} | "
              f"epochs up to {config.n_epoch} | "
              f"memory {config.resolve_max_memory(maze.size)} | batch {config.data_size} | "
              f"target sync every {config.target_update_freq} | "
              f"seed {config.seed if config.seed is not None else 'none'}", flush=True)
        result = trainer.train()

        if config.save_model:
            print(f"Model saved to {agent.save(config.save_model)}")
        print("\nSummary")
        if result.converged:
            print(f"  Converged:      yes (epoch {result.convergence_epoch})")
        else:
            print(f"  Converged:      no (epoch limit of {config.n_epoch} reached)")
        print(f"  Epochs run:     {result.epochs_run}")
        print(f"  Win count:      {result.win_count}")
        print(f"  Final win rate: {result.final_win_rate:.3f}")
        print(f"  Elapsed:        {format_time(result.elapsed_seconds)}")
        return 0 if result.converged else 1

    def _evaluate(self, args: argparse.Namespace) -> int:
        maze = load_maze(args.maze_file)
        self._require_model_file(args.model)

        from .agent import DQNAgent
        from .evaluator import Evaluator

        agent = DQNAgent.load(args.model, state_size=maze.size)
        result = Evaluator(maze).completion_check(agent, stop_on_first_failure=False)
        if result.passed:
            print(f"Completion check: PASSED ({result.checked} start cells checked)")
            return 0
        shown = ", ".join(str(cell) for cell in result.failed_cells[:20])
        more = (f" and {len(result.failed_cells) - 20} more"
                if len(result.failed_cells) > 20 else "")
        print(f"Completion check: FAILED from {len(result.failed_cells)} of "
              f"{result.checked} start cells: {shown}{more}")
        return 1

    def _play(self, args: argparse.Namespace) -> int:
        maze = load_maze(args.maze_file)
        self._require_model_file(args.model)

        from .agent import DQNAgent
        from .evaluator import Evaluator
        from .renderer import LEGEND, MazeRenderer

        agent = DQNAgent.load(args.model, state_size=maze.size)
        evaluator = Evaluator(maze)
        start = tuple(args.start)
        game = evaluator.play_game(agent, start)

        outcome = {"win": "WIN", "lose": "LOSE",
                   "timeout": "NO RESULT (step limit reached)"}[game.status]
        print(f"Result: {outcome} after {game.steps} steps from {start}")
        print("Path: " + " -> ".join(f"({r},{c})" for r, c in game.path))
        print()
        print(MazeRenderer.render_text(evaluator.qmaze))
        print(LEGEND)
        if args.save_image:
            MazeRenderer.draw(evaluator.qmaze, save_path=args.save_image)
            print(f"Image saved to {args.save_image}")
        if args.show:
            MazeRenderer.draw(evaluator.qmaze, show=True)
        return 0 if game.won else 1

    # --------------------------------------------------------------------- run
    def run(self, argv: Optional[List[str]] = None) -> int:
        """Run one command and return its exit code (see the module docstring)."""
        args = self.build_parser().parse_args(argv)  # argparse exits with 2 on bad arguments
        try:
            return args.handler(args)
        except ConfigError as exc:
            print(f"error: {exc}", file=sys.stderr)
            return 2


def main(argv: Optional[List[str]] = None) -> int:
    """Console entry point used by main.py."""
    return TreasureHuntCLI().run(argv)
