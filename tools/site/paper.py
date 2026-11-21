#!/usr/bin/env python3
"""site/assets/paper.webp: a seamless 512 px tile of rag paper, after godot/assets/shaders/paper.gdshader.

The paper tone #EFE6D4, long soft horizontal fibres (noise blurred along x with wrap-around, so the tile is seamless)
and a fine tooth of grain. Deterministic (fixed seed). Needs numpy and Pillow.
"""
from pathlib import Path

import numpy as np
from PIL import Image

N = 512
PAPER = np.array([0xEF, 0xE6, 0xD4], float)
FIBRE = np.array([0x8C, 0x73, 0x4D], float)


def wrap_blur(a: np.ndarray, sx: float, sy: float) -> np.ndarray:
    """Gaussian blur with periodic edges, done in the frequency domain."""
    fy = np.fft.fftfreq(a.shape[0])[:, None]
    fx = np.fft.fftfreq(a.shape[1])[None, :]
    g = np.exp(-2 * (np.pi ** 2) * ((fx * sx) ** 2 + (fy * sy) ** 2))
    return np.real(np.fft.ifft2(np.fft.fft2(a) * g))


def norm(a: np.ndarray) -> np.ndarray:
    return (a - a.mean()) / (a.std() + 1e-9)


def main() -> None:
    rng = np.random.default_rng(9)
    fib = norm(wrap_blur(rng.standard_normal((N, N)), 40.0, 1.6)) + 0.5 * norm(wrap_blur(rng.standard_normal((N, N)), 70.0, 4.0))
    tooth = norm(wrap_blur(rng.standard_normal((N, N)), 0.7, 0.7))
    col = PAPER[None, None, :] + (FIBRE - PAPER)[None, None, :] * (0.022 * fib)[..., None] - (2.6 * tooth)[..., None]
    out = Path(__file__).resolve().parents[2] / "site/assets/paper.webp"
    Image.fromarray(np.clip(col, 0, 255).astype(np.uint8)).save(out, "WEBP", quality=70, method=6)
    print(out, out.stat().st_size // 1024, "KB")


if __name__ == "__main__":
    main()
