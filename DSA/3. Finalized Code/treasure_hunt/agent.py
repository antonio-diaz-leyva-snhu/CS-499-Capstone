"""DQNAgent: owns the Q-network, the target network, the optimizer and epsilon.

Replaces the notebook's build_model(), train_step(), the global model,
optimizer and epsilon, and the action-choice code that sat inside qtrain().
"""
from __future__ import annotations

import random
from pathlib import Path
from typing import Optional, Sequence

import numpy as np
import tensorflow as tf

from .config import MODEL_SUFFIXES, NUM_ACTIONS, ConfigError, TrainingConfig


def set_seed(seed: Optional[int]) -> None:
    """Seed Python, NumPy and TensorFlow together so a run can be repeated.

    Does nothing when seed is None.
    """
    if seed is not None:
        tf.keras.utils.set_random_seed(seed)


class DQNAgent:
    """A deep Q-learning agent for the treasure maze.

    Attributes:
        model: the online network that picks moves and is trained.
        target_model: a copy of the model, refreshed with sync_target(); it is
            used to build the training targets (see GameExperience.get_data).
        epsilon: the current exploration rate.
    """

    # Above this win rate the agent stops decaying and holds epsilon at epsilon_min.
    EXPLOIT_WIN_RATE = 0.9

    def __init__(
        self,
        state_size: int,
        epsilon: float = 1.0,
        epsilon_min: float = 0.05,
        epsilon_decay: float = 0.995,
        model: Optional["tf.keras.Model"] = None,
    ) -> None:
        self.state_size = state_size
        self.epsilon = epsilon
        self.epsilon_min = epsilon_min
        self.epsilon_decay = epsilon_decay
        self.model = model if model is not None else self.build_model(state_size)
        self.target_model = tf.keras.models.clone_model(self.model)
        self.sync_target()
        self.loss_fn = tf.keras.losses.MeanSquaredError()
        self.optimizer = tf.keras.optimizers.Adam()
        self._train_step = tf.function(self._train_step_impl)

    # ------------------------------------------------------------------ build
    @staticmethod
    def build_model(state_size: int) -> "tf.keras.Model":
        """Dense(n) - PReLU - Dense(n) - PReLU - Dense(4), where n is the maze size.

        The notebook also called model.compile(), but training goes through the
        custom train step, so the unused compile is dropped.
        """
        return tf.keras.Sequential([
            tf.keras.Input(shape=(state_size,)),
            tf.keras.layers.Dense(state_size),
            tf.keras.layers.PReLU(),
            tf.keras.layers.Dense(state_size),
            tf.keras.layers.PReLU(),
            tf.keras.layers.Dense(NUM_ACTIONS),
        ])

    @classmethod
    def from_config(cls, config: TrainingConfig, state_size: int) -> "DQNAgent":
        """Build a new agent, or load config.load_model, using the config's epsilon settings."""
        settings = dict(
            epsilon=config.epsilon,
            epsilon_min=config.epsilon_min,
            epsilon_decay=config.epsilon_decay,
        )
        if config.load_model:
            return cls.load(config.load_model, state_size=state_size, **settings)
        return cls(state_size, **settings)

    # ------------------------------------------------------------ acting
    def predict(self, envstate: np.ndarray) -> np.ndarray:
        """Q-values for the four moves in the given environment state."""
        state = np.asarray(envstate, dtype=np.float32)
        if state.ndim == 1:
            state = np.expand_dims(state, axis=0)
        return self.model(state, training=False).numpy()[0]

    def best_action(self, envstate: np.ndarray) -> int:
        """The move with the highest Q-value (no exploration)."""
        return int(np.argmax(self.predict(envstate)))

    def select_action(self, envstate: np.ndarray, valid_actions: Sequence[int]) -> int:
        """Epsilon-greedy choice: a random valid move with probability epsilon,
        otherwise the best-scoring move."""
        if np.random.rand() < self.epsilon:
            return random.choice(valid_actions)
        return self.best_action(envstate)

    # ---------------------------------------------------------- learning
    def _train_step_impl(self, x, y):
        with tf.GradientTape() as tape:
            q_values = self.model(x, training=True)
            loss = self.loss_fn(y, q_values)
        grads = tape.gradient(loss, self.model.trainable_variables)
        self.optimizer.apply_gradients(zip(grads, self.model.trainable_variables))
        return loss

    def train_on_batch(self, inputs: np.ndarray, targets: np.ndarray) -> float:
        """One gradient update on a batch; returns the loss."""
        return float(self._train_step(inputs, targets))

    def sync_target(self) -> None:
        """Copy the online network's weights into the target network."""
        self.target_model.set_weights(self.model.get_weights())

    def decay_epsilon(self, win_rate: float) -> None:
        """Hold epsilon at its minimum once the win rate is high, otherwise decay it."""
        if win_rate > self.EXPLOIT_WIN_RATE:
            self.epsilon = self.epsilon_min
        else:
            self.epsilon = max(self.epsilon * self.epsilon_decay, self.epsilon_min)

    # ----------------------------------------------------------- persistence
    def save(self, path: str) -> Path:
        """Save the network to a .keras or .h5 file and return the path.

        Only the network is saved. Epsilon and the optimizer state restart when
        a saved model is loaded for more training.
        """
        target = Path(path)
        if target.suffix.lower() not in MODEL_SUFFIXES:
            raise ConfigError(f"Model file must end in one of {', '.join(MODEL_SUFFIXES)}.")
        target.parent.mkdir(parents=True, exist_ok=True)
        self.model.save(str(target))
        return target

    @classmethod
    def load(cls, path: str, state_size: Optional[int] = None, **settings) -> "DQNAgent":
        """Load a saved network and check it fits the maze.

        Raises ConfigError when the file is missing or unreadable, or when its
        input or output size does not match the maze.
        """
        source = Path(path)
        if not source.is_file():
            raise ConfigError(f"Model file not found: {source}")
        try:
            model = tf.keras.models.load_model(str(source), compile=False)
        except Exception as exc:  # Keras raises many different types for bad files
            raise ConfigError(f"Could not load model from {source}: {exc}") from exc
        inputs, outputs = model.input_shape[-1], model.output_shape[-1]
        if state_size is not None and inputs != state_size:
            raise ConfigError(
                f"Model {source.name} expects {inputs} inputs but the maze has "
                f"{state_size} cells. Use the maze it was trained on."
            )
        if outputs != NUM_ACTIONS:
            raise ConfigError(
                f"Model {source.name} has {outputs} outputs; this game needs {NUM_ACTIONS}."
            )
        return cls(inputs, model=model, **settings)
