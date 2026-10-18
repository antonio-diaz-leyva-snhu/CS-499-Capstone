# Treasure Hunt: Deep Q-Learning

A pirate learns to reach the treasure in an 8x8 maze using deep Q-learning. This is the
object-oriented, console version of the Project Two notebook: every run is a command, and
nothing needs editing in code.

## Setup

```
pip install -r requirements.txt
```

Python 3.10 or newer with TensorFlow 2.16 or newer. `TreasureMaze.py` and
`GameExperience.py` are the supplied course modules and are unchanged.

## Use

```
python main.py train                                        # notebook settings, 1000 epochs max
python main.py train --epochs 600 --seed 7 --save-model models/pirate.keras
python main.py evaluate --model models/pirate.keras         # win from every start cell?
python main.py play --model models/pirate.keras --start 3 4 --save-image board.png
```

`python main.py <command> --help` lists every option. With no options, `train` uses the
notebook's settings: 1000 epochs, replay memory 8 x maze cells (512), batch 32, target network
refresh every 50 epochs, epsilon 1.0 down to 0.05 at 0.995 per epoch.

| Exit code | Meaning |
| --- | --- |
| 0 | `train` reached 100% wins, `evaluate` passed, `play` won |
| 1 | The run finished but did not succeed (epoch limit, failed check, game not won) |
| 2 | Bad option, setting, maze file or model file |

### Your own maze

A JSON file holding a list of rows (1 = open, 0 = wall), or an object with a `"maze"` key.
See `mazes/default.json`. The start is the top-left cell and the treasure is the bottom-right
cell. Both must be open, and every open cell must have a path to the treasure. Pass it with
`--maze-file`. A saved model only fits the maze size it was trained on.

### Notes

- `--seed` makes the network's starting weights and the random choices repeatable.
- `--load-model` continues training from a saved model. Only the network is saved, so epsilon
  and the optimizer restart; use `--epsilon-start 0.1` to explore less when continuing.
- `--log-file run.csv` records epoch, loss, moves, win count, win rate and epsilon.

## Layout

```
main.py                    entry point
treasure_hunt/
  config.py                TrainingConfig, maze loading and checks
  agent.py                 DQNAgent: network, target network, optimizer, epsilon
  trainer.py               Trainer: the training loop
  evaluator.py             Evaluator: play_game and completion_check
  renderer.py              MazeRenderer: text board and image
  cli.py                   console commands
mazes/default.json         the notebook's 8x8 maze
tests/                     python -m unittest discover -s tests -t . -v
TreasureMaze.py            supplied, unchanged
GameExperience.py          supplied, unchanged
```
