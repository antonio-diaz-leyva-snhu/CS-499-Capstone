"""MazeRenderer: draws the maze as text in the terminal or as a matplotlib image.

Replaces the notebook's show(). matplotlib is imported only when an image is
requested, and saving an image uses a non-interactive backend, so it works over
SSH or in a script with no display.
"""
from __future__ import annotations

from typing import Optional

import numpy as np

LEGEND = "# wall   . open   * visited   S start   P pirate   T treasure"


class MazeRenderer:
    """Draws a TreasureMaze's current board."""

    @staticmethod
    def render_text(qmaze) -> str:
        """The board as text, one row per line. See LEGEND for the symbols."""
        nrows, ncols = qmaze.maze.shape
        pirate_row, pirate_col, _mode = qmaze.state
        lines = []
        for row in range(nrows):
            cells = []
            for col in range(ncols):
                if (row, col) == (pirate_row, pirate_col):
                    symbol = "P"
                elif (row, col) == (nrows - 1, ncols - 1):
                    symbol = "T"
                elif (row, col) == tuple(qmaze.pirate):
                    symbol = "S"
                elif (row, col) in qmaze.visited:
                    symbol = "*"
                elif qmaze.maze[row, col] == 0.0:
                    symbol = "#"
                else:
                    symbol = "."
                cells.append(symbol)
            lines.append(" ".join(cells))
        return "\n".join(lines)

    @staticmethod
    def draw(qmaze, show: bool = False, save_path: Optional[str] = None) -> None:
        """Draw the board with matplotlib: show it in a window, save it, or both.

        Shading follows the notebook: walls black, open cells white, visited
        cells light gray, the pirate dark gray and the treasure near white.
        """
        import matplotlib
        if not show:
            matplotlib.use("Agg")  # no display needed
        import matplotlib.pyplot as plt

        nrows, ncols = qmaze.maze.shape
        fig, ax = plt.subplots()
        ax.grid(True)
        ax.set_xticks(np.arange(0.5, ncols, 1))
        ax.set_yticks(np.arange(0.5, nrows, 1))
        ax.set_xticklabels([])
        ax.set_yticklabels([])
        canvas = np.copy(qmaze.maze)
        for row, col in qmaze.visited:
            canvas[row, col] = 0.6
        pirate_row, pirate_col, _mode = qmaze.state
        canvas[pirate_row, pirate_col] = 0.3   # pirate cell
        canvas[nrows - 1, ncols - 1] = 0.9     # treasure cell
        ax.imshow(canvas, interpolation="none", cmap="gray")
        if save_path:
            fig.savefig(save_path, bbox_inches="tight")
        if show:
            plt.show()
        plt.close(fig)
