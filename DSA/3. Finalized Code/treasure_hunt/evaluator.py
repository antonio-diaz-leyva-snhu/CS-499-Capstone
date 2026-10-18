"""Evaluator: plays games with a trained agent and checks it can win from every cell.

Replaces the notebook's play_game() and completion_check(). Results come back
as data (win or lose, steps, path) instead of a bare True or False.
"""
from __future__ import annotations

from dataclasses import dataclass, field
from typing import List, Optional, Sequence, Tuple

import numpy as np

from TreasureMaze import TreasureMaze

from .config import ConfigError

Cell = Tuple[int, int]


@dataclass
class GameResult:
    """Outcome of one game. status is 'win', 'lose' or 'timeout'."""

    status: str
    steps: int
    path: List[Cell] = field(default_factory=list)

    @property
    def won(self) -> bool:
        return self.status == "win"


@dataclass
class CompletionResult:
    """Outcome of the all-start-cells check."""

    passed: bool
    checked: int
    failed_cells: List[Cell] = field(default_factory=list)


class Evaluator:
    """Plays games on its own TreasureMaze, so it never disturbs a training maze.

    After play_game(), the maze (self.qmaze) holds that game's final board for
    MazeRenderer to draw.
    """

    def __init__(self, maze: np.ndarray) -> None:
        self.qmaze = TreasureMaze(maze)
        self._free = set(self.qmaze.free_cells)
        # Safety cutoff so a looping agent cannot run forever.
        self.max_steps = self.qmaze.maze.size * 4

    def _run(self, agent, start: Cell, limit: int) -> GameResult:
        qmaze = self.qmaze
        qmaze.reset(start)
        envstate = qmaze.observe()
        path = [start]
        steps = 0
        status = "timeout"
        while steps < limit:
            action = agent.best_action(envstate)
            envstate, _reward, game_status = qmaze.act(action)
            steps += 1
            row, col, _mode = qmaze.state
            path.append((row, col))
            if game_status == "win":
                status = "win"
                break
            if game_status == "lose":
                status = "lose"
                break
        return GameResult(status=status, steps=steps, path=path)

    def play_game(self, agent, start_cell: Sequence[int],
                  max_steps: Optional[int] = None) -> GameResult:
        """Play one game from start_cell using the agent's best move each turn."""
        start = (int(start_cell[0]), int(start_cell[1]))
        if start not in self._free:
            raise ConfigError(
                f"Start cell {start} must be an open cell other than the treasure "
                f"cell {self.qmaze.target}."
            )
        return self._run(agent, start, max_steps if max_steps is not None else self.max_steps)

    def completion_check(self, agent, max_steps: Optional[int] = None,
                         stop_on_first_failure: bool = True) -> CompletionResult:
        """Play from every open start cell that has a legal move.

        Training stops at the first failure to save time. Pass
        stop_on_first_failure=False to list every cell the agent fails from.
        """
        limit = max_steps if max_steps is not None else self.max_steps
        failed: List[Cell] = []
        checked = 0
        for cell in self.qmaze.free_cells:
            if not self.qmaze.valid_actions(cell):
                continue
            checked += 1
            if not self._run(agent, cell, limit).won:
                failed.append(cell)
                if stop_on_first_failure:
                    break
        return CompletionResult(passed=not failed, checked=checked, failed_cells=failed)
