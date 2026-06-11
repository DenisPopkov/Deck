#!/usr/bin/env python3
"""Export StnGridModel (2×4 tape saturation grid) to ONNX for CassetteAutoMaster.

Inputs:
  audio_in   float32 [1, channels, samples]
  drive_norm float32 [1]  — (saturationDrive - 0.85) / 0.45, clamped 0..1
  bias_norm  float32 [1]  — biasReductionOnHf / 0.12, clamped 0..1
  state_in   float32 [1, 2] — recurrent L/R state

Outputs:
  audio_out  float32 [1, channels, samples]
  state_out  float32 [1, 2]

Usage:
  pip install torch onnx onnxscript
  python scripts/export_stn_onnx.py
"""
from __future__ import annotations

import pathlib

try:
    import torch
    import torch.nn as nn
except ImportError as exc:
    raise SystemExit("Install torch: pip install torch onnx onnxscript") from exc


K_ALPHA = torch.tensor(
    [[0.008, 0.010, 0.012, 0.014], [0.011, 0.014, 0.017, 0.020]], dtype=torch.float32
)
K_BETA = torch.tensor(
    [[0.018, 0.022, 0.026, 0.030], [0.024, 0.028, 0.032, 0.036]], dtype=torch.float32
)
K_MIX = torch.tensor(
    [[0.22, 0.26, 0.30, 0.34], [0.28, 0.32, 0.36, 0.40]], dtype=torch.float32
)
K_DRIVE_SCALE = torch.tensor(
    [[0.88, 0.92, 0.96, 1.00], [1.05, 1.12, 1.18, 1.24]], dtype=torch.float32
)


def _tent_weight(pos: torch.Tensor, k: int) -> torch.Tensor:
    return torch.clamp(1.0 - torch.abs(pos - float(k)), 0.0, 1.0)


def _bilinear(grid: torch.Tensor, drive_norm: torch.Tensor, bias_norm: torch.Tensor) -> torch.Tensor:
    """ONNX-friendly 2×4 bilinear lookup (no data-dependent integer indexing)."""
    d = torch.clamp(drive_norm.reshape(-1)[0], 0.0, 1.0)
    b = torch.clamp(bias_norm.reshape(-1)[0], 0.0, 1.0) * 3.0

    row0_w = torch.clamp(1.0 - d * 2.0, 0.0, 1.0)
    row1_w = 1.0 - row0_w

    col_w = torch.stack([_tent_weight(b, k) for k in range(4)])
    col_w = col_w / torch.clamp(col_w.sum(), min=1.0e-6)

    row0 = (grid[0] * col_w).sum()
    row1 = (grid[1] * col_w).sum()
    return row0_w * row0 + row1_w * row1


class StnGridOnnx(nn.Module):
    def forward(
        self,
        audio_in: torch.Tensor,
        drive_norm: torch.Tensor,
        bias_norm: torch.Tensor,
        state_in: torch.Tensor,
    ) -> tuple[torch.Tensor, torch.Tensor]:
        alpha = _bilinear(K_ALPHA.to(audio_in.device), drive_norm, bias_norm)
        beta = _bilinear(K_BETA.to(audio_in.device), drive_norm, bias_norm)
        mix = _bilinear(K_MIX.to(audio_in.device), drive_norm, bias_norm)
        drive_scale = _bilinear(K_DRIVE_SCALE.to(audio_in.device), drive_norm, bias_norm)
        drive = torch.clamp(drive_norm.reshape(-1)[0] * 0.45 + 0.85, min=0.85) * drive_scale

        _, channels, length = audio_in.shape
        state_l = state_in[0, 0]
        state_r = state_in[0, 1]
        out = audio_in.clone()

        for i in range(length):
            for ch in range(min(channels, 2)):
                x = out[0, ch, i]
                state = state_l if ch == 0 else state_r
                driven = x * drive
                state = (1.0 - alpha) * state + alpha * torch.tanh(driven - beta * state)
                nonlinear = torch.tanh(driven + mix * state)
                y = (1.0 - mix) * driven + mix * nonlinear
                out[0, ch, i] = y
                if ch == 0:
                    state_l = state
                else:
                    state_r = state

        state_out = torch.stack([state_l, state_r]).reshape(1, 2)
        return out, state_out


def main() -> None:
    root = pathlib.Path(__file__).resolve().parents[1]
    out_dir = root / "models"
    out_dir.mkdir(exist_ok=True)
    out_path = out_dir / "tape_stn.onnx"

    model = StnGridOnnx().eval()
    dummy_audio = torch.zeros(1, 2, 256, dtype=torch.float32)
    dummy_drive = torch.tensor([0.55], dtype=torch.float32)
    dummy_bias = torch.tensor([0.35], dtype=torch.float32)
    dummy_state = torch.zeros(1, 2, dtype=torch.float32)

    export_kwargs = dict(
        input_names=["audio_in", "drive_norm", "bias_norm", "state_in"],
        output_names=["audio_out", "state_out"],
        dynamic_axes={
            "audio_in": {0: "batch", 1: "channels", 2: "samples"},
            "audio_out": {0: "batch", 1: "channels", 2: "samples"},
        },
        opset_version=17,
    )

    try:
        torch.onnx.export(
            model,
            (dummy_audio, dummy_drive, dummy_bias, dummy_state),
            str(out_path),
            dynamo=False,
            **export_kwargs,
        )
    except TypeError:
        torch.onnx.export(
            model,
            (dummy_audio, dummy_drive, dummy_bias, dummy_state),
            str(out_path),
            **export_kwargs,
        )

    print(f"Wrote {out_path} ({out_path.stat().st_size} bytes)")


if __name__ == "__main__":
    main()
