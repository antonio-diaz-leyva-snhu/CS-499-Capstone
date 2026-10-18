"""Settings, maze loading and maze validation.

This module deliberately does not import TensorFlow, so the console can parse
arguments and report configuration problems immediately.
"""
from __future__ import annotations

import json
from collections import deque
from dataclasses import dataclass
from pathlib import Path
from typing import List, Optional, Tuple

import numpy as np

# The four moves, in the same order as TreasureMaze.py: LEFT, UP, RIGHT, DOWN.
NUM_ACTIONS = 4

# File types the console accepts for saved models.
MODEL_SUFFIXES = (".keras", ".h5")

# The maze that was hard-coded in the notebook: 1 = open cell, 0 = wall.
# The pirate starts at the top-left and the treasure is the bottom-right cell.
_DEFAULT_MAZE_ROWS = [
    [1, 0, 1, 1, 1, 1, 1, 1],
    [1, 0, 1, 1, 1, 0, 1, 1],
    [1, 1, 1, 1, 0, 1, 0, 1],
    [1, 1, 1, 0, 1, 1, 1, 1],
    [1, 1, 0, 1, 1, 1, 1, 1],
    [1, 1, 1, 0, 1, 0, 0, 0],
    [1, 1, 1, 0, 1, 1, 1, 1],
    [1, 1, 1, 1, 0, 1, 1, 1],
]


class ConfigError(ValueError):
    """An invalid setting, maze file or model file. The console exits with code 2."""


def default_maze() -> np.ndarray:
    """Return a fresh copy of the built-in 8x8 maze."""
    return np.array(_DEFAULT_MAZE_ROWS, dtype=float)


def _unreachable_cells(grid: np.ndarray) -> List[Tuple[int, int]]:
    """Open cells that have no path to the treasure (breadth-first search)."""
    nrows, ncols = grid.shape
    target = (nrows - 1, ncols - 1)
    seen = {target}
    queue = deque([target])
    while queue:
        row, col = queue.popleft()
        for d_row, d_col in ((1, 0), (-1, 0), (0, 1), (0, -1)):
            nxt = (row + d_row, col + d_col)
            if (0 <= nxt[0] < nrows and 0 <= nxt[1] < ncols
                    and grid[nxt] == 1.0 and nxt not in seen):
                seen.add(nxt)
                queue.append(nxt)
    return [(r, c) for r in range(nrows) for c in range(ncols)
            if grid[r, c] == 1.0 and (r, c) not in seen]


def validate_maze(maze) -> np.ndarray:
    """Check a maze and return it as a 2-D float array.

    Rules: rectangular, only 0 (wall) and 1 (open), start (0, 0) and treasure
    (bottom-right) open, and every open cell can reach the treasure. An
    unsolvable maze would train forever without a result.
    """
    try:
        grid = np.asarray(maze, dtype=float)
    except (ValueError, TypeError) as exc:
        raise ConfigError(f"Maze must be a rectangular grid of numbers: {exc}") from exc
    if grid.ndim != 2:
        raise ConfigError("Maze must be a 2-D grid (a list of equally long rows).")
    if grid.size < 2:
        raise ConfigError("Maze must have at least 2 cells.")
    if not np.isin(grid, (0.0, 1.0)).all():
        raise ConfigError("Maze cells must be 0 (wall) or 1 (open).")
    nrows, ncols = grid.shape
    if grid[0, 0] != 1.0:
        raise ConfigError("The start cell (0, 0) must be open.")
    if grid[nrows - 1, ncols - 1] != 1.0:
        raise ConfigError(f"The treasure cell ({nrows - 1}, {ncols - 1}) must be open.")
    stranded = _unreachable_cells(grid)
    if stranded:
        shown = ", ".join(str(cell) for cell in stranded[:5])
        more = f" and {len(stranded) - 5} more" if len(stranded) > 5 else ""
        raise ConfigError(
            f"{len(stranded)} open cell(s) have no path to the treasure: {shown}{more}. "
            "Open a path to them or turn them into walls."
        )
    return grid


def load_maze(path: Optional[str] = None) -> np.ndarray:
    """Load a maze from a JSON file, or return the built-in maze when path is None.

    The file holds either a list of rows or an object with a "maze" key.
    """
    if path is None:
        return validate_maze(default_maze())
    maze_path = Path(path)
    if not maze_path.is_file():
        raise ConfigError(f"Maze file not found: {maze_path}")
    try:
        data = json.loads(maze_path.read_text(encoding="utf-8"))
    except (json.JSONDecodeError, UnicodeDecodeError) as exc:
        raise ConfigError(f"Maze file {maze_path} is not valid JSON: {exc}") from exc
    if isinstance(data, dict):
        if "maze" not in data:
            raise ConfigError('Maze file must be a list of rows or an object with a "maze" key.')
        data = data["maze"]
    return validate_maze(data)


@dataclass
class TrainingConfig:
    """Every setting for a training run, with the notebook's values as defaults.

    Attribute names follow the notebook (n_epoch, max_memory, data_size, ...).
    max_memory=None means 8 times the number of maze cells (512 for the 8x8 maze).
    """

    n_epoch: int = 1000
    max_memory: Optional[int] = None
    data_size: int = 32
    target_update_freq: int = 50
    epsilon: float = 1.0
    epsilon_min: float = 0.05
    epsilon_decay: float = 0.995
    seed: Optional[int] = None
    maze_file: Optional[str] = None
    load_model: Optional[str] = None
    save_model: Optional[str] = None
    log_file: Optional[str] = None
    log_every: int = 1

    def __post_init__(self) -> None:
        self._check(self.n_epoch >= 1, "epochs must be at least 1")
        self._check(self.max_memory is None or self.max_memory >= 1,
                    "max-memory must be at least 1")
        self._check(self.data_size >= 1, "batch-size must be at least 1")
        self._check(self.target_update_freq >= 1, "target-update-freq must be at least 1")
        self._check(0.0 <= self.epsilon <= 1.0, "epsilon-start must be between 0 and 1")
        self._check(0.0 <= self.epsilon_min <= 1.0, "epsilon-min must be between 0 and 1")
        self._check(0.0 < self.epsilon_decay <= 1.0,
                    "epsilon-decay must be above 0 and at most 1")
        self._check(self.log_every >= 1, "log-every must be at least 1")
        if self.save_model is not None:
            self._check(Path(self.save_model).suffix.lower() in MODEL_SUFFIXES,
                        f"save-model must end in one of {', '.join(MODEL_SUFFIXES)}")

    @staticmethod
    def _check(condition: bool, message: str) -> None:
        if not condition:
            raise ConfigError(f"Invalid setting: {message}.")

    def resolve_max_memory(self, maze_size: int) -> int:
        """Replay memory size: the setting, or 8 times the number of maze cells."""
        return self.max_memory if self.max_memory is not None else 8 * maze_size

    def load_maze(self) -> np.ndarray:
        """Load and validate the maze this run uses."""
        return load_maze(self.maze_file)
