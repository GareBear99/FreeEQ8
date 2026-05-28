#!/usr/bin/env python3
"""
GenerateSpectrograms.py — Visualizes pre-ring artifacts across phase modes.

Loads WAV files from Tests/prering_output/ and generates spectrograms
zoomed to ±50ms around the transient onset.

Requirements:
    pip install numpy matplotlib scipy

Usage:
    python Tests/GenerateSpectrograms.py

Output:
    Tests/spectrograms/*.png
"""

import numpy as np
import matplotlib.pyplot as plt
from scipy import signal as sig
import struct
import os
from pathlib import Path

# ═══════════════════════════════════════════════════════════════════════════
# Constants
# ═══════════════════════════════════════════════════════════════════════════

SAMPLE_RATE = 44100
PRE_PADDING_SAMPLES = 4096  # Where the transient starts in our signals
WINDOW_MS = 50              # ±50ms around transient
WINDOW_SAMPLES = int(SAMPLE_RATE * WINDOW_MS / 1000)

# STFT parameters
STFT_WINDOW_SIZE = 256
STFT_HOP_SIZE = 64          # 75% overlap for smooth spectrograms
STFT_NFFT = 512

# Output paths
INPUT_DIR = Path("Tests/prering_output")
OUTPUT_DIR = Path("Tests/spectrograms")

# ═══════════════════════════════════════════════════════════════════════════
# WAV Loading (simple 16-bit PCM mono reader)
# ═══════════════════════════════════════════════════════════════════════════

def load_wav(path):
    """Load a 16-bit PCM mono WAV file."""
    with open(path, 'rb') as f:
        # Read header
        riff = f.read(4)
        if riff != b'RIFF':
            raise ValueError(f"Not a RIFF file: {path}")
        
        file_size = struct.unpack('<I', f.read(4))[0]
        wave = f.read(4)
        if wave != b'WAVE':
            raise ValueError(f"Not a WAVE file: {path}")
        
        # Find fmt chunk
        while True:
            chunk_id = f.read(4)
            chunk_size = struct.unpack('<I', f.read(4))[0]
            
            if chunk_id == b'fmt ':
                fmt_data = f.read(chunk_size)
                audio_format = struct.unpack('<H', fmt_data[0:2])[0]
                num_channels = struct.unpack('<H', fmt_data[2:4])[0]
                sample_rate = struct.unpack('<I', fmt_data[4:8])[0]
                bits_per_sample = struct.unpack('<H', fmt_data[14:16])[0]
                break
            else:
                f.read(chunk_size)
        
        # Find data chunk
        while True:
            chunk_id = f.read(4)
            if not chunk_id:
                raise ValueError(f"No data chunk found: {path}")
            chunk_size = struct.unpack('<I', f.read(4))[0]
            
            if chunk_id == b'data':
                data_bytes = f.read(chunk_size)
                break
            else:
                f.read(chunk_size)
        
        # Convert to float
        num_samples = chunk_size // 2  # 16-bit = 2 bytes
        samples = struct.unpack(f'<{num_samples}h', data_bytes)
        return np.array(samples, dtype=np.float32) / 32767.0, sample_rate

# ═══════════════════════════════════════════════════════════════════════════
# Spectrogram Generation
# ═══════════════════════════════════════════════════════════════════════════

def compute_spectrogram(audio, fs):
    """Compute STFT spectrogram."""
    f, t, Sxx = sig.spectrogram(
        audio,
        fs=fs,
        window='hann',
        nperseg=STFT_WINDOW_SIZE,
        noverlap=STFT_WINDOW_SIZE - STFT_HOP_SIZE,
        nfft=STFT_NFFT,
        mode='magnitude'
    )
    # Convert to dB
    Sxx_db = 20 * np.log10(Sxx + 1e-10)
    return f, t, Sxx_db

def plot_comparison(signal_name, signals_dict, output_path):
    """
    Plot side-by-side spectrograms for original + 3 phase modes.
    Zooms to ±50ms around transient onset.
    """
    fig, axes = plt.subplots(2, 2, figsize=(14, 10))
    fig.suptitle(f'Pre-Ring Analysis: {signal_name.upper()}\n'
                 f'6dB Bell @ 2kHz, Q=2 — Zoomed to ±{WINDOW_MS}ms around transient',
                 fontsize=14, fontweight='bold')
    
    modes = ['original', 'iir', 'natural', 'linear']
    titles = ['Original (No Filter)', 
              'Zero-Latency (IIR) — 0ms latency',
              'NaturalPhase (128 smp) — 2.9ms latency',
              'LinearPhase (2048 smp) — 46.4ms latency']
    
    # Determine common color scale
    vmin, vmax = -60, 0
    
    for idx, (mode, title) in enumerate(zip(modes, titles)):
        ax = axes[idx // 2, idx % 2]
        
        key = f"{signal_name}_{mode}"
        if key not in signals_dict:
            ax.text(0.5, 0.5, 'Data not found', ha='center', va='center',
                   transform=ax.transAxes)
            ax.set_title(title)
            continue
        
        audio, fs = signals_dict[key]
        
        # Extract window around transient
        start_sample = PRE_PADDING_SAMPLES - WINDOW_SAMPLES
        end_sample = PRE_PADDING_SAMPLES + WINDOW_SAMPLES
        start_sample = max(0, start_sample)
        end_sample = min(len(audio), end_sample)
        
        audio_window = audio[start_sample:end_sample]
        
        # Compute spectrogram
        f, t, Sxx_db = compute_spectrogram(audio_window, fs)
        
        # Convert time axis to ms relative to transient onset
        t_ms = (t - (PRE_PADDING_SAMPLES - start_sample) / fs) * 1000
        
        # Plot
        im = ax.pcolormesh(t_ms, f / 1000, Sxx_db, 
                          shading='gouraud', 
                          vmin=vmin, vmax=vmax,
                          cmap='inferno')
        
        # Mark transient onset
        ax.axvline(x=0, color='white', linestyle='--', linewidth=1.5, alpha=0.8)
        
        # Highlight pre-ring zone
        if mode in ['natural', 'linear']:
            ax.axvspan(-10, 0, color='cyan', alpha=0.2, label='Pre-ring window')
        
        ax.set_xlabel('Time (ms relative to transient)')
        ax.set_ylabel('Frequency (kHz)')
        ax.set_title(title)
        ax.set_xlim(-WINDOW_MS, WINDOW_MS)
        ax.set_ylim(0, 10)  # 0-10 kHz
        
        # Add colorbar
        cbar = plt.colorbar(im, ax=ax)
        cbar.set_label('Magnitude (dB)')
    
    plt.tight_layout()
    plt.savefig(output_path, dpi=150, bbox_inches='tight')
    plt.close()
    print(f"  Saved: {output_path}")

def plot_waveform_comparison(signal_name, signals_dict, output_path):
    """
    Plot waveform overlay showing pre-ring in time domain.
    Zooms to ±20ms around transient.
    """
    fig, ax = plt.subplots(figsize=(12, 5))
    
    window_ms = 20  # Tighter zoom for waveform
    window_samples = int(SAMPLE_RATE * window_ms / 1000)
    
    modes = ['iir', 'natural', 'linear']
    colors = ['green', 'blue', 'red']
    labels = ['Zero-Latency (IIR)', 'NaturalPhase (2.9ms)', 'LinearPhase (46ms)']
    
    for mode, color, label in zip(modes, colors, labels):
        key = f"{signal_name}_{mode}"
        if key not in signals_dict:
            continue
        
        audio, fs = signals_dict[key]
        
        # Extract window
        start_sample = PRE_PADDING_SAMPLES - window_samples
        end_sample = PRE_PADDING_SAMPLES + window_samples
        start_sample = max(0, start_sample)
        end_sample = min(len(audio), end_sample)
        
        audio_window = audio[start_sample:end_sample]
        
        # Time axis in ms
        t_ms = np.arange(len(audio_window)) / fs * 1000 - (PRE_PADDING_SAMPLES - start_sample) / fs * 1000
        
        ax.plot(t_ms, audio_window, color=color, label=label, alpha=0.7, linewidth=0.8)
    
    # Mark transient onset
    ax.axvline(x=0, color='black', linestyle='--', linewidth=1.5, label='Transient onset')
    
    # Highlight pre-ring zone
    ax.axvspan(-10, 0, color='yellow', alpha=0.2, label='10ms pre-ring window')
    
    ax.set_xlabel('Time (ms relative to transient)')
    ax.set_ylabel('Amplitude')
    ax.set_title(f'Waveform Comparison: {signal_name.upper()}\n'
                 f'6dB Bell @ 2kHz, Q=2 — Zoomed to ±{window_ms}ms')
    ax.set_xlim(-window_ms, window_ms)
    ax.legend(loc='upper right')
    ax.grid(True, alpha=0.3)
    
    plt.tight_layout()
    plt.savefig(output_path, dpi=150, bbox_inches='tight')
    plt.close()
    print(f"  Saved: {output_path}")

def plot_prering_detail(signal_name, signals_dict, output_path):
    """
    Detailed pre-ring comparison showing just the 10ms before transient.
    """
    fig, axes = plt.subplots(1, 3, figsize=(15, 4))
    fig.suptitle(f'Pre-Ring Detail: {signal_name.upper()} — 10ms before transient onset',
                 fontsize=12, fontweight='bold')
    
    modes = ['iir', 'natural', 'linear']
    titles = ['Zero-Latency (IIR)', 'NaturalPhase (128 smp)', 'LinearPhase (2048 smp)']
    
    # Pre-ring window: 10ms before transient
    window_samples = int(SAMPLE_RATE * 10 / 1000)  # 10ms = 441 samples
    
    for ax, mode, title in zip(axes, modes, titles):
        key = f"{signal_name}_{mode}"
        if key not in signals_dict:
            ax.text(0.5, 0.5, 'Data not found', ha='center', va='center',
                   transform=ax.transAxes)
            ax.set_title(title)
            continue
        
        audio, fs = signals_dict[key]
        
        # Extract just the pre-ring window
        start_sample = PRE_PADDING_SAMPLES - window_samples
        end_sample = PRE_PADDING_SAMPLES
        
        audio_window = audio[start_sample:end_sample]
        
        # Time axis in ms
        t_ms = np.arange(len(audio_window)) / fs * 1000 - 10  # -10ms to 0ms
        
        ax.plot(t_ms, audio_window, color='red', linewidth=1)
        ax.fill_between(t_ms, audio_window, alpha=0.3, color='red')
        
        # Calculate RMS
        rms = np.sqrt(np.mean(audio_window**2))
        rms_db = 20 * np.log10(rms + 1e-10)
        
        ax.set_xlabel('Time (ms)')
        ax.set_ylabel('Amplitude')
        ax.set_title(f'{title}\nRMS: {rms_db:.1f} dB')
        ax.set_xlim(-10, 0)
        ax.set_ylim(-0.05, 0.05)
        ax.grid(True, alpha=0.3)
        ax.axhline(y=0, color='black', linewidth=0.5)
    
    plt.tight_layout()
    plt.savefig(output_path, dpi=150, bbox_inches='tight')
    plt.close()
    print(f"  Saved: {output_path}")

# ═══════════════════════════════════════════════════════════════════════════
# Main
# ═══════════════════════════════════════════════════════════════════════════

def main():
    print("═" * 60)
    print("  Spectrogram Generation — FreeEQ8 Pre-Ring Analysis")
    print("═" * 60)
    print()
    
    # Create output directory
    OUTPUT_DIR.mkdir(parents=True, exist_ok=True)
    
    # Check for input files
    if not INPUT_DIR.exists():
        print(f"ERROR: Input directory not found: {INPUT_DIR}")
        print("Run PreRingAnalysis first to generate WAV files.")
        return 1
    
    # Load all WAV files
    signals = {}
    print("Loading WAV files...")
    for wav_path in sorted(INPUT_DIR.glob("*.wav")):
        key = wav_path.stem  # e.g., "kick_iir"
        try:
            audio, fs = load_wav(wav_path)
            signals[key] = (audio, fs)
            print(f"  Loaded: {wav_path.name} ({len(audio)} samples)")
        except Exception as e:
            print(f"  ERROR loading {wav_path}: {e}")
    
    if not signals:
        print("ERROR: No WAV files found.")
        return 1
    
    print()
    
    # Determine signal names (kick, snare, pluck, plosive)
    signal_names = set()
    for key in signals.keys():
        parts = key.rsplit('_', 1)
        if len(parts) == 2:
            signal_names.add(parts[0])
    
    print("Generating spectrograms...")
    for signal_name in sorted(signal_names):
        print(f"\n  Processing: {signal_name}")
        
        # Main spectrogram comparison
        plot_comparison(
            signal_name, 
            signals,
            OUTPUT_DIR / f"{signal_name}_spectrogram.png"
        )
        
        # Waveform overlay
        plot_waveform_comparison(
            signal_name,
            signals,
            OUTPUT_DIR / f"{signal_name}_waveform.png"
        )
        
        # Pre-ring detail
        plot_prering_detail(
            signal_name,
            signals,
            OUTPUT_DIR / f"{signal_name}_prering_detail.png"
        )
    
    # Generate summary comparison figure
    print("\nGenerating summary figure...")
    plot_summary(signals, OUTPUT_DIR / "summary_comparison.png")
    
    print()
    print("═" * 60)
    print("  All spectrograms saved to Tests/spectrograms/")
    print("═" * 60)
    
    return 0

def plot_summary(signals_dict, output_path):
    """
    Generate a summary figure showing pre-ring energy across all signals.
    """
    fig, ax = plt.subplots(figsize=(10, 6))
    
    signal_names = ['kick', 'snare', 'pluck', 'plosive']
    modes = ['iir', 'natural', 'linear']
    mode_labels = ['Zero-Latency', 'NaturalPhase', 'LinearPhase']
    colors = ['green', 'blue', 'red']
    
    # Pre-ring window: 10ms before transient
    window_samples = int(SAMPLE_RATE * 10 / 1000)
    
    x = np.arange(len(signal_names))
    width = 0.25
    
    for i, (mode, label, color) in enumerate(zip(modes, mode_labels, colors)):
        rms_values = []
        for signal_name in signal_names:
            key = f"{signal_name}_{mode}"
            if key in signals_dict:
                audio, fs = signals_dict[key]
                start = PRE_PADDING_SAMPLES - window_samples
                end = PRE_PADDING_SAMPLES
                rms = np.sqrt(np.mean(audio[start:end]**2))
                rms_db = 20 * np.log10(rms + 1e-10)
                rms_values.append(rms_db)
            else:
                rms_values.append(-100)
        
        bars = ax.bar(x + (i - 1) * width, rms_values, width, 
                     label=label, color=color, alpha=0.7)
    
    ax.set_xlabel('Test Signal')
    ax.set_ylabel('Pre-Ring Energy (dB)')
    ax.set_title('Pre-Ring Energy Comparison Across Phase Modes\n'
                '(RMS in 10ms window before transient onset)')
    ax.set_xticks(x)
    ax.set_xticklabels([s.capitalize() for s in signal_names])
    ax.legend()
    ax.grid(True, axis='y', alpha=0.3)
    
    # Add reference lines
    ax.axhline(y=-60, color='gray', linestyle='--', linewidth=0.8, alpha=0.5)
    ax.text(3.5, -58, 'Noise floor', fontsize=8, color='gray')
    
    plt.tight_layout()
    plt.savefig(output_path, dpi=150, bbox_inches='tight')
    plt.close()
    print(f"  Saved: {output_path}")

if __name__ == '__main__':
    exit(main())
