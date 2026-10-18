"""Console entry point.

    python main.py train
    python main.py train --epochs 600 --seed 7 --save-model models/pirate.keras
    python main.py evaluate --model models/pirate.keras
    python main.py play --model models/pirate.keras --start 3 4

Run  python main.py --help  or  python main.py <command> --help  for every option.
"""
import sys

from treasure_hunt.cli import main

if __name__ == "__main__":
    sys.exit(main())
