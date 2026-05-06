# PRD: Resosyn V1

## Goal
A harmonic resonator synthesizer. A noise, wavetable, or sample-based excitation source
is passed through a bank of 32 tuned bandpass filters per voice, with harmonic positions
calculated from a MIDI fundamental. Timbre is shaped by per-harmonic gain snapshots,
optionally derived from real-instrument one-shot analysis.

## Features

### Excitation Source (3 modes, switchable)
| Mode      | Controls |
|-----------|----------|
| Noise     | Colour: White / Pink / Brown (discrete) |
| Wavetable | Position: 0–100%; built-in bank: sine, saw, square, triangle, white, pink, brown noise |
| Sampler   | Drag-and-drop audio file |

Sampler loop controls:
| Parameter      | Range             | Default  |
|----------------|-------------------|----------|
| Loop Enable    | On / Off          | On       |
| Loop Start     | 0–100% of file    | 0%       |
| Loop End       | 0–100% of file    | 100%     |
| Loop Type      | Forward / Ping-Pong | Forward |
| Loop Crossfade | 0–500 ms          | 0 ms     |

### Per-Harmonic Controls (×32 per voice — set by analysis, user-editable)
| Parameter          | Range          | Default | Notes |
|--------------------|----------------|---------|-------|
| Frequency (coarse) | ±24 semitones  | 0       | Offset from calculated harmonic position |
| Detune (fine)      | ±100 cents     | 0       | |
| Q                  | 0.1–50         | 1.0     | |
| Gain               | -inf to 0 dB   | 0 dB    | |

### Global Filterbank
| Parameter     | Range        | Default | Notes |
|---------------|--------------|---------|-------|
| Overall Q     | 0.1–50       | 1.0     | Multiplies all per-harmonic Q values |
| Global Detune | ±100 cents   | 0       | |
| Stretch       | 0–1          | 0       | Inharmonicity coefficient; pulls higher harmonics sharp: f_n = n × f0 × (1 + Stretch × n²) |
| Spread        | 0–100%       | 0%      | Pans harmonics outward from centre, alternating L/R by harmonic index |

### Timbre / Gain Morph
| Parameter            | Range    | Default | Notes |
|----------------------|----------|---------|-------|
| Snapshot A           | —        | —       | Per-harmonic gain values; set by analysis or manually |
| Snapshot B           | —        | —       | |
| Timbre Morph         | 0–100%   | 0%      | Blends relative spectral shape A→B |
| Gain Morph           | 0–100%   | 0%      | Blends overall gain level A→B |
| Note Morph Amount    | 0–100%   | 0%      | Key height contribution to morph position |
| Velocity Morph Amount| 0–100%   | 0%      | Velocity contribution to morph position |

### Envelope (global)
| Parameter | Range        | Default  |
|-----------|--------------|----------|
| Attack    | 1–5000 ms    | 10 ms    |
| Decay     | 1–5000 ms    | 100 ms   |
| Sustain   | 0–100%       | 80%      |
| Release   | 1–10000 ms   | 500 ms   |

### Voice / Performance
| Parameter  | Range        | Default |
|------------|--------------|---------|
| Polyphony  | 1–8 voices   | 8       |
| Master Gain| -inf to +6 dB| 0 dB    |

### One-Shot Analysis
- Drag-and-drop audio file onto analysis target
- Detects per-harmonic inharmonicity (Frequency + Detune) and relative gains
- Populates values directly into per-harmonic controls and saves as a Snapshot

## Behaviour
- Each MIDI note triggers a voice; fundamental = MIDI note frequency
- 32 bandpass filters per voice tuned to harmonic series of the fundamental
- Stretch shifts higher harmonics sharp: f_n = n × f0 × (1 + Stretch × n²)
- Spread pans harmonic n progressively outward from centre, alternating L/R
- Morph position = Timbre/Gain Morph knob + (normalised note height × Note Morph Amount)
  + (velocity/127 × Velocity Morph Amount), clamped 0–100%
- Voice stealing at polyphony limit: steal oldest active voice
- Analysis populates per-harmonic values without overwriting the other snapshot

## Out of Scope
- Filter type switching — bandpass only for V1
- Harmonic browser / per-harmonic editor UI — values exist but no dedicated visual editor
- MIDI learn / parameter mapping
- Audio effect mode (routing external audio through the filterbank)
- Live audio input and recording — V2/V3
- LFO / modulation routing
- Full preset browser — A/B snapshots only
- Tempo sync
