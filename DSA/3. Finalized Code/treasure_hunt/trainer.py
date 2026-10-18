"""Trainer: the deep Q-learning loop, replacing the notebook's qtrain().

The loop is the notebook's loop, step for step: random start cell, epsilon-greedy
moves, replay memory, one training step per move, target network refresh,
win-rate tracking and the stopping test.
"""
from __future__ import annotations

import csv
import random
import time
from dataclasses import dataclass
from pathlib import Path
from typing import Callable, List, Optional

import numpy as np

from GameExperience import GameExperience
from TreasureMaze import TreasureMaze

from .config import TrainingConfig
from .evaluator import Evaluator

# Training stops once the win rate over the last window reaches this value AND
# the completion check passes from every start cell.
WIN_RATE_GOAL = 0.999

CSV_COLUMNS = ["epoch", "loss", "episodes", "win_count", "win_rate", "epsilon", "elapsed_seconds"]


def format_time(seconds: float) -> str:
    """Readable time: seconds, minutes or hours depending on size."""
    if seconds < 400:
        return "%.1f seconds" % float(seconds)
    if seconds < 4000:
        return "%.2f minutes" % (seconds / 60.0)
    return "%.2f hours" % (seconds / 3600.0)


def _print_now(message: str) -> None:
    print(message, flush=True)


@dataclass
class TrainingResult:
    """Summary of a finished run."""

    epochs_run: int
    converged: bool
    convergence_epoch: Optional[int]
    win_count: int
    final_win_rate: float
    final_loss: float
    elapsed_seconds: float


class Trainer:
    """Trains a DQNAgent on a maze.

    Args:
        agent: a DQNAgent (anything with the same methods works, e.g. in tests).
        maze: a validated 2-D maze array.
        config: the TrainingConfig for this run.
        evaluator: used for the stopping test; built from the maze when omitted.
        log: called with each line of progress output (print by default).
    """

    def __init__(self, agent, maze: np.ndarray, config: TrainingConfig,
                 evaluator: Optional[Evaluator] = None,
                 log: Callable[[str], None] = _print_now) -> None:
        self.agent = agent
        self.config = config
        self.log = log
        self.qmaze = TreasureMaze(maze)
        self.evaluator = evaluator if evaluator is not None else Evaluator(maze)
        self.experience = GameExperience(
            agent.model, agent.target_model,
            max_memory=config.resolve_max_memory(self.qmaze.maze.size),
        )
        self.win_history: List[int] = []
        self.window = self.qmaze.maze.size // 2  # win-rate window, in games

    # ------------------------------------------------------------- one epoch
    def _play_epoch(self):
        """Play one full game, training after every move. Returns (loss, moves)."""
        qmaze, agent, experience = self.qmaze, self.agent, self.experience
        qmaze.reset(random.choice(qmaze.free_cells))
        envstate = qmaze.observe()
        game_over = False
        loss = 0.0
        moves = 0
        while not game_over:
            previous_envstate = envstate
            valid_actions = qmaze.valid_actions()
            if not valid_actions:
                break  # dead end: nothing to do, stop this game
            action = agent.select_action(previous_envstate, valid_actions)
            envstate, reward, game_status = qmaze.act(action)
            if game_status == "win":
                self.win_history.append(1)
                game_over = True
            elif game_status == "lose":
                self.win_history.append(0)
                game_over = True
            experience.remember([previous_envstate, action, reward, envstate, game_over])
            moves += 1
            inputs, targets = experience.get_data(batch_size=self.config.data_size)
            loss = agent.train_on_batch(inputs, targets)
        return loss, moves

    def _win_rate(self) -> float:
        if len(self.win_history) >= self.window:
            return sum(self.win_history[-self.window:]) / self.window
        return 0.0

    # --------------------------------------------------------------- the run
    def train(self) -> TrainingResult:
        """Run training until the stopping test passes or the epoch limit is reached."""
        cfg = self.config
        start = time.perf_counter()
        csv_file = None
        writer = None
        if cfg.log_file:
            Path(cfg.log_file).parent.mkdir(parents=True, exist_ok=True)
            csv_file = open(cfg.log_file, "w", newline="", encoding="utf-8")
            writer = csv.writer(csv_file)
            writer.writerow(CSV_COLUMNS)

        converged = False
        convergence_epoch = None
        epochs_run = 0
        loss = 0.0
        win_rate = 0.0
        try:
            for epoch in range(cfg.n_epoch):
                epsilon_used = self.agent.epsilon
                loss, moves = self._play_epoch()
                epochs_run = epoch + 1

                # Refresh the target network so Bellman targets use recent weights.
                if epoch % cfg.target_update_freq == 0:
                    self.agent.sync_target()

                win_rate = self._win_rate()
                elapsed = time.perf_counter() - start
                wins = sum(self.win_history)
                if epoch % cfg.log_every == 0:
                    self.log(
                        "Epoch: {:03d}/{:d} | Loss: {:.4f} | Episodes: {:d} | "
                        "Win count: {:d} | Win rate: {:.3f} | time: {}".format(
                            epoch, cfg.n_epoch - 1, loss, moves, wins, win_rate,
                            format_time(elapsed)))
                if writer is not None:
                    writer.writerow([epoch, "%.6f" % loss, moves, wins, "%.4f" % win_rate,
                                     "%.4f" % epsilon_used, "%.2f" % elapsed])

                self.agent.decay_epsilon(win_rate)

                if (win_rate >= WIN_RATE_GOAL
                        and self.evaluator.completion_check(self.agent).passed):
                    self.log(f"Reached 100% win rate at epoch {epoch}")
                    converged = True
                    convergence_epoch = epoch
                    break
        finally:
            if csv_file is not None:
                csv_file.close()

        total = time.perf_counter() - start
        self.log("Training complete in: " + format_time(total))
        return TrainingResult(
            epochs_run=epochs_run,
            converged=converged,
            convergence_epoch=convergence_epoch,
            win_count=sum(self.win_history),
            final_win_rate=win_rate,
            final_loss=loss,
            elapsed_seconds=total,
        )
