"""Tests for the treasure_hunt package.

Run from the project folder:

    python -m unittest discover -s tests -t . -v

Three groups:
  * no TensorFlow needed: settings, maze checks, Evaluator, renderer, console errors
  * Trainer logic, using a small numpy stand-in for the agent
  * real DQNAgent and console round trip, skipped when TensorFlow is not installed
"""
import contextlib
import csv
import io
import json
import re
import sys
import tempfile
import unittest
from pathlib import Path

import numpy as np

ROOT = Path(__file__).resolve().parent.parent
if str(ROOT) not in sys.path:
    sys.path.insert(0, str(ROOT))

from treasure_hunt import cli
from treasure_hunt.config import (ConfigError, TrainingConfig, default_maze, load_maze,
                                  validate_maze)
from treasure_hunt.evaluator import Evaluator
from treasure_hunt.renderer import MazeRenderer

try:  # needs GameExperience.py, which imports TensorFlow
    from treasure_hunt.trainer import CSV_COLUMNS, Trainer
    HAS_TRAINER = True
except ImportError:
    HAS_TRAINER = False


def _has_tensorflow() -> bool:
    try:
        import tensorflow as tf
        return hasattr(tf, "keras") and hasattr(tf.keras, "Sequential")
    except Exception:
        return False


HAS_TF = _has_tensorflow()

LEFT, UP, RIGHT, DOWN = 0, 1, 2, 3
OPEN_3X3 = np.ones((3, 3))


class RightThenDown:
    """Walks right along the row, then down the last column. Solves any open maze."""

    def __init__(self, ncols):
        self.ncols = ncols

    def best_action(self, envstate):
        position = int(np.argmax(np.asarray(envstate).ravel() == 0.5))  # pirate mark
        return RIGHT if position % self.ncols < self.ncols - 1 else DOWN


class AlwaysLeft:
    """Never reaches the treasure."""

    def best_action(self, envstate):
        return LEFT


def run_cli(*argv):
    """Run the console; return (exit code, stdout, stderr)."""
    out, err = io.StringIO(), io.StringIO()
    with contextlib.redirect_stdout(out), contextlib.redirect_stderr(err):
        code = cli.main(list(argv))
    return code, out.getvalue(), err.getvalue()


# ============================================================ settings and mazes
class ConfigTests(unittest.TestCase):
    def test_defaults_match_the_notebook(self):
        config = TrainingConfig()
        self.assertEqual(
            (config.n_epoch, config.data_size, config.target_update_freq),
            (1000, 32, 50))
        self.assertEqual(
            (config.epsilon, config.epsilon_min, config.epsilon_decay),
            (1.0, 0.05, 0.995))
        self.assertEqual(config.resolve_max_memory(64), 512)  # 8 x cells

    def test_max_memory_override(self):
        self.assertEqual(TrainingConfig(max_memory=100).resolve_max_memory(64), 100)

    def test_invalid_settings_are_rejected(self):
        bad = [dict(n_epoch=0), dict(data_size=0), dict(target_update_freq=0),
               dict(max_memory=0), dict(epsilon=1.5), dict(epsilon_min=-0.1),
               dict(epsilon_decay=0), dict(log_every=0), dict(save_model="model.txt")]
        for kwargs in bad:
            with self.subTest(kwargs=kwargs), self.assertRaises(ConfigError):
                TrainingConfig(**kwargs)


class MazeTests(unittest.TestCase):
    def setUp(self):
        tmp = tempfile.TemporaryDirectory()
        self.addCleanup(tmp.cleanup)
        self.tmp = Path(tmp.name)

    def write(self, name, text):
        path = self.tmp / name
        path.write_text(text, encoding="utf-8")
        return str(path)

    def test_built_in_maze_is_valid(self):
        self.assertEqual(validate_maze(default_maze()).shape, (8, 8))
        self.assertEqual(load_maze(None).shape, (8, 8))

    def test_default_json_matches_built_in_maze(self):
        self.assertTrue(np.array_equal(load_maze(str(ROOT / "mazes" / "default.json")),
                                       default_maze()))

    def test_file_formats(self):
        rows = [[1, 1], [1, 1]]
        self.assertEqual(load_maze(self.write("a.json", json.dumps(rows))).shape, (2, 2))
        self.assertEqual(load_maze(self.write("b.json", json.dumps({"maze": rows}))).shape, (2, 2))

    def test_invalid_mazes_are_rejected(self):
        bad = {
            "ragged": [[1, 1], [1]],
            "not a grid": [1, 1, 1],
            "three dimensions": [[[1]], [[1]]],
            "values other than 0 and 1": [[1, 0.5], [1, 1]],
            "blocked start": [[0, 1], [1, 1]],
            "blocked treasure": [[1, 1], [1, 0]],
            "cells with no path to the treasure": [[1, 0, 1], [0, 0, 0], [1, 0, 1]],
            "too small": [[1]],
        }
        for label, grid in bad.items():
            with self.subTest(label), self.assertRaises(ConfigError):
                validate_maze(grid)

    def test_bad_files_are_rejected(self):
        with self.assertRaises(ConfigError):
            load_maze(str(self.tmp / "missing.json"))
        with self.assertRaises(ConfigError):
            load_maze(self.write("broken.json", "{not json"))
        with self.assertRaises(ConfigError):
            load_maze(self.write("nokey.json", json.dumps({"grid": [[1, 1], [1, 1]]})))


# ================================================================= evaluator
class EvaluatorTests(unittest.TestCase):
    def test_winning_game_reports_steps_and_path(self):
        game = Evaluator(OPEN_3X3).play_game(RightThenDown(3), (0, 0))
        self.assertTrue(game.won)
        self.assertEqual(game.status, "win")
        self.assertEqual(game.steps, 4)
        self.assertEqual(game.path, [(0, 0), (0, 1), (0, 2), (1, 2), (2, 2)])

    def test_losing_game(self):
        game = Evaluator(OPEN_3X3).play_game(AlwaysLeft(), (0, 0))
        self.assertEqual(game.status, "lose")
        self.assertFalse(game.won)

    def test_step_limit_gives_timeout(self):
        game = Evaluator(OPEN_3X3).play_game(AlwaysLeft(), (0, 0), max_steps=1)
        self.assertEqual((game.status, game.steps), ("timeout", 1))

    def test_start_cell_must_be_open_and_not_the_treasure(self):
        evaluator = Evaluator(OPEN_3X3)
        for cell in [(2, 2), (5, 5), (-1, 0)]:
            with self.subTest(cell=cell), self.assertRaises(ConfigError):
                evaluator.play_game(RightThenDown(3), cell)

    def test_completion_check_passes_for_a_perfect_policy(self):
        result = Evaluator(OPEN_3X3).completion_check(RightThenDown(3))
        self.assertTrue(result.passed)
        self.assertEqual(result.checked, 8)  # 9 cells minus the treasure
        self.assertEqual(result.failed_cells, [])

    def test_completion_check_lists_failures(self):
        evaluator = Evaluator(OPEN_3X3)
        first_only = evaluator.completion_check(AlwaysLeft())
        self.assertFalse(first_only.passed)
        self.assertEqual(len(first_only.failed_cells), 1)
        everything = evaluator.completion_check(AlwaysLeft(), stop_on_first_failure=False)
        self.assertEqual(len(everything.failed_cells), 8)

    def test_checking_does_not_touch_another_maze(self):
        # The evaluator owns its maze, so training state elsewhere is never disturbed.
        self.assertIsNot(Evaluator(OPEN_3X3).qmaze, Evaluator(OPEN_3X3).qmaze)


# ================================================================== renderer
class RendererTests(unittest.TestCase):
    def test_text_board_after_a_win(self):
        evaluator = Evaluator(OPEN_3X3)
        evaluator.play_game(RightThenDown(3), (0, 0))
        self.assertEqual(MazeRenderer.render_text(evaluator.qmaze), "S * *\n. . *\n. . P")

    def test_text_board_shows_walls(self):
        maze = np.array([[1, 0, 1], [1, 1, 1], [1, 1, 1]])
        evaluator = Evaluator(maze)
        evaluator.qmaze.reset((0, 0))
        board = MazeRenderer.render_text(evaluator.qmaze).splitlines()
        self.assertEqual(board[0], "P # .")
        self.assertEqual(board[2], ". . T")

    def test_image_is_saved_without_a_display(self):
        try:
            import matplotlib  # noqa: F401
        except ImportError:
            self.skipTest("matplotlib not installed")
        evaluator = Evaluator(OPEN_3X3)
        evaluator.play_game(RightThenDown(3), (0, 0))
        with tempfile.TemporaryDirectory() as tmp:
            target = Path(tmp) / "board.png"
            MazeRenderer.draw(evaluator.qmaze, save_path=str(target))
            self.assertGreater(target.stat().st_size, 0)


# ============================================================ console (no TF)
class ConsoleErrorTests(unittest.TestCase):
    def test_bad_settings_exit_with_2(self):
        for argv in (["train", "--epochs", "0"], ["train", "--save-model", "model.txt"],
                     ["train", "--epsilon-decay", "5"]):
            with self.subTest(argv=argv):
                code, _out, err = run_cli(*argv)
                self.assertEqual(code, 2)
                self.assertIn("error:", err)

    def test_missing_files_exit_with_2(self):
        for argv in (["train", "--maze-file", "nope.json"],
                     ["train", "--load-model", "nope.keras"],
                     ["evaluate", "--model", "nope.keras"],
                     ["play", "--model", "nope.keras"]):
            with self.subTest(argv=argv):
                code, _out, err = run_cli(*argv)
                self.assertEqual(code, 2)
                self.assertIn("not found", err)

    def test_argument_errors_exit_with_2(self):
        for argv in ([], ["play"], ["evaluate"], ["play", "--model", "m.keras", "--start", "1"],
                     ["train", "--epochs", "many"], ["unknown"]):
            with self.subTest(argv=argv), contextlib.redirect_stderr(io.StringIO()):
                with self.assertRaises(SystemExit) as caught:
                    cli.main(argv)
                self.assertEqual(caught.exception.code, 2)

    def test_help_exits_cleanly(self):
        with contextlib.redirect_stdout(io.StringIO()) as out:
            with self.assertRaises(SystemExit) as caught:
                cli.main(["--help"])
        self.assertEqual(caught.exception.code, 0)
        for command in ("train", "evaluate", "play"):
            self.assertIn(command, out.getvalue())

    def test_options_reach_the_config(self):
        args = cli.TreasureHuntCLI().build_parser().parse_args(
            ["train", "--epochs", "5", "--batch-size", "8", "--seed", "3", "--epsilon-start", "0.5"])
        config = cli.TreasureHuntCLI.config_from_args(args)
        self.assertEqual((config.n_epoch, config.data_size, config.seed, config.epsilon),
                         (5, 8, 3, 0.5))
        self.assertEqual(config.target_update_freq, 50)  # untouched default


# ================================================================== trainer
class _Tensor:
    def __init__(self, array):
        self.array = array

    def numpy(self):
        return self.array


class _ZeroModel:
    """Stands in for a Keras model: returns zero Q-values."""
    output_shape = (None, 4)

    def __call__(self, x, training=False):
        return _Tensor(np.zeros((len(x), 4), dtype=np.float32))


class FakeAgent:
    """Numpy stand-in for DQNAgent that follows a fixed policy and counts calls."""

    def __init__(self, policy):
        self.policy = policy
        self.model, self.target_model = _ZeroModel(), _ZeroModel()
        self.epsilon = 1.0
        self.syncs = 0
        self.batches = 0
        self.win_rates = []

    def select_action(self, envstate, valid_actions):
        return self.policy.best_action(envstate)

    def best_action(self, envstate):
        return self.policy.best_action(envstate)

    def train_on_batch(self, inputs, targets):
        self.batches += 1
        return 0.5

    def sync_target(self):
        self.syncs += 1

    def decay_epsilon(self, win_rate):
        self.win_rates.append(win_rate)


@unittest.skipUnless(HAS_TRAINER, "needs GameExperience.py (TensorFlow import)")
class TrainerTests(unittest.TestCase):
    def make(self, policy, **config):
        import random
        random.seed(0)
        np.random.seed(0)
        lines = []
        agent = FakeAgent(policy)
        trainer = Trainer(agent, OPEN_3X3, TrainingConfig(**config), log=lines.append)
        return agent, trainer, lines

    def test_stops_when_the_win_rate_and_completion_check_pass(self):
        agent, trainer, lines = self.make(RightThenDown(3), n_epoch=50)
        result = trainer.train()
        self.assertTrue(result.converged)
        self.assertEqual(trainer.window, 4)            # 9 cells // 2 games
        self.assertEqual(result.convergence_epoch, 3)  # first epoch with 4 straight wins
        self.assertEqual((result.epochs_run, result.win_count), (4, 4))
        self.assertEqual(result.final_win_rate, 1.0)
        self.assertIn("Reached 100% win rate at epoch 3", lines)
        self.assertTrue(lines[-1].startswith("Training complete in:"))
        self.assertEqual(agent.syncs, 1)               # epoch 0 only, with the default of 50
        self.assertEqual(len(agent.win_rates), 4)      # decay_epsilon runs every epoch

    def test_runs_to_the_epoch_limit_without_converging(self):
        agent, trainer, lines = self.make(AlwaysLeft(), n_epoch=3)
        result = trainer.train()
        self.assertFalse(result.converged)
        self.assertIsNone(result.convergence_epoch)
        self.assertEqual((result.epochs_run, result.win_count), (3, 0))
        self.assertFalse(any("Reached 100%" in line for line in lines))

    def test_one_training_step_per_move(self):
        agent, trainer, lines = self.make(AlwaysLeft(), n_epoch=3)
        trainer.train()
        moves = sum(int(re.search(r"Episodes: (\d+)", line).group(1))
                    for line in lines if line.startswith("Epoch:"))
        self.assertGreater(moves, 0)
        self.assertEqual(agent.batches, moves)

    def test_epoch_line_format_matches_the_notebook(self):
        _agent, trainer, lines = self.make(AlwaysLeft(), n_epoch=1)
        trainer.train()
        self.assertRegex(lines[0], r"^Epoch: 000/0 \| Loss: 0\.5000 \| Episodes: \d+ \| "
                                   r"Win count: 0 \| Win rate: 0\.000 \| time: .+ seconds$")

    def test_target_network_refresh_schedule(self):
        agent, trainer, _lines = self.make(AlwaysLeft(), n_epoch=7, target_update_freq=3)
        trainer.train()
        self.assertEqual(agent.syncs, 3)  # epochs 0, 3 and 6

    def test_log_every_thins_the_output(self):
        _agent, trainer, lines = self.make(AlwaysLeft(), n_epoch=6, log_every=3)
        trainer.train()
        self.assertEqual(len([l for l in lines if l.startswith("Epoch:")]), 2)  # epochs 0 and 3

    def test_csv_log(self):
        with tempfile.TemporaryDirectory() as tmp:
            target = Path(tmp) / "logs" / "run.csv"
            _agent, trainer, _lines = self.make(AlwaysLeft(), n_epoch=3, log_file=str(target))
            trainer.train()
            rows = list(csv.reader(target.open(encoding="utf-8")))
        self.assertEqual(rows[0], CSV_COLUMNS)
        self.assertEqual(len(rows), 4)  # header plus 3 epochs
        self.assertEqual([row[0] for row in rows[1:]], ["0", "1", "2"])

    def test_replay_memory_size(self):
        _agent, trainer, _lines = self.make(AlwaysLeft(), n_epoch=1)
        self.assertEqual(trainer.experience.max_memory, 72)  # 8 x 9 cells
        _agent, trainer, _lines = self.make(AlwaysLeft(), n_epoch=1, max_memory=10)
        self.assertEqual(trainer.experience.max_memory, 10)


# ============================================================== real TensorFlow
@unittest.skipUnless(HAS_TF, "TensorFlow is not installed")
class AgentTests(unittest.TestCase):
    def setUp(self):
        from treasure_hunt.agent import DQNAgent
        self.DQNAgent = DQNAgent
        tmp = tempfile.TemporaryDirectory()
        self.addCleanup(tmp.cleanup)
        self.tmp = Path(tmp.name)

    def test_network_shape(self):
        agent = self.DQNAgent(9)
        self.assertEqual(agent.model.output_shape, (None, 4))
        # Dense(9) + PReLU(9) + Dense(9) + PReLU(9) + Dense(4)
        self.assertEqual(agent.model.count_params(), 90 + 9 + 90 + 9 + 40)

    def test_action_selection(self):
        agent = self.DQNAgent(9)
        state = np.zeros((1, 9), dtype=np.float32)
        agent.epsilon = 1.0
        self.assertEqual({agent.select_action(state, [2]) for _ in range(10)}, {2})
        agent.epsilon = 0.0
        self.assertEqual(agent.select_action(state, [0, 1, 2, 3]), agent.best_action(state))

    def test_training_lowers_the_loss(self):
        agent = self.DQNAgent(9)
        rng = np.random.default_rng(0)
        x = rng.random((16, 9), dtype=np.float32)
        y = rng.random((16, 4), dtype=np.float32)
        first = agent.train_on_batch(x, y)
        for _ in range(60):
            last = agent.train_on_batch(x, y)
        self.assertLess(last, first)

    def test_target_network_sync(self):
        agent = self.DQNAgent(9)
        x = np.random.default_rng(1).random((8, 9), dtype=np.float32)
        y = np.ones((8, 4), dtype=np.float32)
        for _ in range(5):
            agent.train_on_batch(x, y)
        differs = any(not np.allclose(a, b) for a, b in
                      zip(agent.model.get_weights(), agent.target_model.get_weights()))
        self.assertTrue(differs)
        agent.sync_target()
        for a, b in zip(agent.model.get_weights(), agent.target_model.get_weights()):
            self.assertTrue(np.allclose(a, b))

    def test_epsilon_rule(self):
        agent = self.DQNAgent(9, epsilon=1.0, epsilon_min=0.05, epsilon_decay=0.5)
        agent.decay_epsilon(0.5)
        self.assertAlmostEqual(agent.epsilon, 0.5)
        agent.decay_epsilon(0.95)  # above 0.9: held at the minimum
        self.assertAlmostEqual(agent.epsilon, 0.05)

    def test_same_seed_gives_same_network(self):
        from treasure_hunt.agent import set_seed
        set_seed(11)
        first = self.DQNAgent(9).model.get_weights()
        set_seed(11)
        second = self.DQNAgent(9).model.get_weights()
        for a, b in zip(first, second):
            self.assertTrue(np.allclose(a, b))

    def test_save_and_load(self):
        agent = self.DQNAgent(9)
        path = agent.save(str(self.tmp / "models" / "agent.keras"))
        loaded = self.DQNAgent.load(str(path), state_size=9)
        state = np.random.default_rng(2).random((1, 9), dtype=np.float32)
        self.assertTrue(np.allclose(agent.predict(state), loaded.predict(state), atol=1e-6))

    def test_load_rejects_bad_files(self):
        path = self.DQNAgent(9).save(str(self.tmp / "agent.keras"))
        with self.assertRaises(ConfigError):
            self.DQNAgent.load(str(path), state_size=64)           # wrong maze size
        with self.assertRaises(ConfigError):
            self.DQNAgent.load(str(self.tmp / "missing.keras"))
        junk = self.tmp / "junk.keras"
        junk.write_text("not a model")
        with self.assertRaises(ConfigError):
            self.DQNAgent.load(str(junk))
        with self.assertRaises(ConfigError):
            self.DQNAgent(9).save(str(self.tmp / "agent.txt"))


@unittest.skipUnless(HAS_TF, "TensorFlow is not installed")
class ConsoleRoundTripTests(unittest.TestCase):
    """train -> evaluate -> play through the console on a tiny open maze."""

    def test_train_evaluate_play(self):
        with tempfile.TemporaryDirectory() as tmp:
            tmp = Path(tmp)
            maze_file = tmp / "tiny.json"
            maze_file.write_text(json.dumps([[1, 1, 1], [1, 1, 1], [1, 1, 1]]))
            model, log, image = tmp / "m" / "agent.keras", tmp / "run.csv", tmp / "board.png"

            code, out, _err = run_cli("train", "--epochs", "3", "--seed", "1",
                                      "--maze-file", str(maze_file), "--save-model", str(model),
                                      "--log-file", str(log))
            self.assertIn(code, (0, 1))
            self.assertIn("Summary", out)
            self.assertTrue(model.is_file())
            self.assertTrue(log.is_file())

            code, out, _err = run_cli("evaluate", "--model", str(model),
                                      "--maze-file", str(maze_file))
            self.assertIn(code, (0, 1))
            self.assertIn("Completion check:", out)

            code, out, _err = run_cli("play", "--model", str(model), "--start", "0", "0",
                                      "--maze-file", str(maze_file), "--save-image", str(image))
            self.assertIn(code, (0, 1))
            self.assertIn("Result:", out)
            self.assertIn("Path: (0,0)", out)
            self.assertTrue(image.is_file())

            code, _out, err = run_cli("play", "--model", str(model), "--start", "2", "2",
                                      "--maze-file", str(maze_file))
            self.assertEqual(code, 2)  # the treasure cell is not a valid start


if __name__ == "__main__":
    unittest.main()
