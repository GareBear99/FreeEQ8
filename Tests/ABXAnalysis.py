#!/usr/bin/env python3
"""
ABXAnalysis.py — Statistical analysis for ABX listening test results.

For PAPER.md §5 — Perceptual Validation / Listening Study

Computes:
  - Hit rate (proportion correct)
  - p-value (binomial test vs 50% chance)
  - d-prime (sensitivity index)
  - 95% confidence intervals

Usage:
    python3 ABXAnalysis.py abx_results_YYYYMMDD_HHMMSS.csv

Output:
    Console summary and optional JSON/LaTeX export for paper inclusion.
"""

import sys
import csv
import math
from typing import Dict, List, Tuple, Optional
from dataclasses import dataclass
from collections import defaultdict


# ===========================================================================
# Statistical Functions (no scipy dependency for portability)
# ===========================================================================

def norm_cdf(x: float) -> float:
    """Standard normal CDF using error function approximation."""
    return 0.5 * (1.0 + math.erf(x / math.sqrt(2.0)))


def norm_ppf(p: float) -> float:
    """Inverse standard normal CDF (percent point function).
    Rational approximation from Abramowitz & Stegun.
    """
    if p <= 0.0:
        return -float('inf')
    if p >= 1.0:
        return float('inf')
    
    # Coefficients for rational approximation
    a = [
        -3.969683028665376e+01,  2.209460984245205e+02,
        -2.759285104469687e+02,  1.383577518672690e+02,
        -3.066479806614716e+01,  2.506628277459239e+00
    ]
    b = [
        -5.447609879822406e+01,  1.615858368580409e+02,
        -1.556989798598866e+02,  6.680131188771972e+01,
        -1.328068155288572e+01
    ]
    c = [
        -7.784894002430293e-03, -3.223964580411365e-01,
        -2.400758277161838e+00, -2.549732539343734e+00,
         4.374664141464968e+00,  2.938163982698783e+00
    ]
    d = [
         7.784695709041462e-03,  3.224671290700398e-01,
         2.445134137142996e+00,  3.754408661907416e+00
    ]
    
    p_low = 0.02425
    p_high = 1.0 - p_low
    
    if p < p_low:
        q = math.sqrt(-2.0 * math.log(p))
        return (((((c[0]*q + c[1])*q + c[2])*q + c[3])*q + c[4])*q + c[5]) / \
               ((((d[0]*q + d[1])*q + d[2])*q + d[3])*q + 1.0)
    elif p <= p_high:
        q = p - 0.5
        r = q * q
        return (((((a[0]*r + a[1])*r + a[2])*r + a[3])*r + a[4])*r + a[5]) * q / \
               (((((b[0]*r + b[1])*r + b[2])*r + b[3])*r + b[4])*r + 1.0)
    else:
        q = math.sqrt(-2.0 * math.log(1.0 - p))
        return -(((((c[0]*q + c[1])*q + c[2])*q + c[3])*q + c[4])*q + c[5]) / \
                ((((d[0]*q + d[1])*q + d[2])*q + d[3])*q + 1.0)


def binomial_cdf(k: int, n: int, p: float) -> float:
    """Cumulative binomial distribution P(X <= k).
    Uses normal approximation for large n.
    """
    if n >= 30:
        # Normal approximation with continuity correction
        mu = n * p
        sigma = math.sqrt(n * p * (1 - p))
        if sigma == 0:
            return 1.0 if k >= mu else 0.0
        z = (k + 0.5 - mu) / sigma
        return norm_cdf(z)
    else:
        # Exact calculation for small n
        total = 0.0
        for i in range(k + 1):
            # Binomial coefficient * p^i * (1-p)^(n-i)
            coeff = math.comb(n, i)
            total += coeff * (p ** i) * ((1 - p) ** (n - i))
        return total


def binomial_test(successes: int, trials: int, p0: float = 0.5) -> float:
    """Two-sided binomial test p-value.
    H0: true proportion = p0
    H1: true proportion ≠ p0
    """
    if trials == 0:
        return 1.0
    
    # Calculate p-value as 2 * min(P(X <= k), P(X >= k))
    p_lower = binomial_cdf(successes, trials, p0)
    p_upper = 1.0 - binomial_cdf(successes - 1, trials, p0)
    
    return 2.0 * min(p_lower, p_upper)


def d_prime(hit_rate: float, n_trials: int) -> Tuple[float, float, float]:
    """Compute d' (d-prime) sensitivity index with confidence interval.
    
    For ABX test where chance = 50%, d' = 2 * Z(hit_rate)
    
    Returns: (d_prime, lower_95ci, upper_95ci)
    """
    # Correct for extreme values (log-linear rule)
    hr_corrected = (hit_rate * n_trials + 0.5) / (n_trials + 1)
    hr_corrected = max(0.01, min(0.99, hr_corrected))
    
    # d' = 2 * Z(hit_rate) for ABX with 50% chance
    z_hr = norm_ppf(hr_corrected)
    dprime = 2.0 * z_hr
    
    # Standard error of d' (Macmillan & Creelman, 2005)
    # SE(d') ≈ sqrt(2 * hr * (1-hr) / n) * (phi(Z(hr))^-1)
    # Simplified approximation:
    if n_trials > 0:
        se_hr = math.sqrt(hr_corrected * (1 - hr_corrected) / n_trials)
        # Derivative of Z function at hr
        phi = math.exp(-0.5 * z_hr * z_hr) / math.sqrt(2 * math.pi)
        if phi > 0.001:
            se_dprime = 2.0 * se_hr / phi
        else:
            se_dprime = 1.0  # Fallback for extreme values
        
        ci_lower = dprime - 1.96 * se_dprime
        ci_upper = dprime + 1.96 * se_dprime
    else:
        ci_lower = ci_upper = 0.0
    
    return dprime, ci_lower, ci_upper


def wilson_confidence_interval(successes: int, trials: int, 
                                confidence: float = 0.95) -> Tuple[float, float]:
    """Wilson score interval for binomial proportion.
    More accurate than normal approximation, especially for extreme proportions.
    """
    if trials == 0:
        return 0.0, 1.0
    
    z = norm_ppf(1.0 - (1.0 - confidence) / 2.0)
    p_hat = successes / trials
    
    denominator = 1.0 + z * z / trials
    center = p_hat + z * z / (2 * trials)
    margin = z * math.sqrt((p_hat * (1 - p_hat) + z * z / (4 * trials)) / trials)
    
    lower = max(0.0, (center - margin) / denominator)
    upper = min(1.0, (center + margin) / denominator)
    
    return lower, upper


# ===========================================================================
# Data Structures
# ===========================================================================

@dataclass
class TrialData:
    trial_num: int
    stimulus: str
    x_is_a: int
    user_response: int
    correct: int
    response_time_ms: float
    timestamp: str


@dataclass
class StimulusStats:
    stimulus: str
    n_trials: int
    n_correct: int
    hit_rate: float
    hit_rate_ci_lower: float
    hit_rate_ci_upper: float
    p_value: float
    d_prime: float
    d_prime_ci_lower: float
    d_prime_ci_upper: float
    mean_response_time_ms: float


@dataclass
class OverallStats:
    n_trials: int
    n_correct: int
    hit_rate: float
    hit_rate_ci_lower: float
    hit_rate_ci_upper: float
    p_value: float
    d_prime: float
    d_prime_ci_lower: float
    d_prime_ci_upper: float
    mean_response_time_ms: float
    by_stimulus: Dict[str, StimulusStats]


# ===========================================================================
# Analysis Functions
# ===========================================================================

def load_results(filepath: str) -> List[TrialData]:
    """Load ABX results from CSV file."""
    trials = []
    
    with open(filepath, 'r', newline='') as f:
        reader = csv.DictReader(f)
        for row in reader:
            trial = TrialData(
                trial_num=int(row['trial']),
                stimulus=row['stimulus'],
                x_is_a=int(row['x_is_a']),
                user_response=int(row['user_response']),
                correct=int(row['correct']),
                response_time_ms=float(row['response_time_ms']),
                timestamp=row['timestamp']
            )
            trials.append(trial)
    
    return trials


def analyze_trials(trials: List[TrialData]) -> OverallStats:
    """Compute all statistics from trial data."""
    if not trials:
        raise ValueError("No trials to analyze")
    
    # Overall statistics
    n_total = len(trials)
    n_correct = sum(t.correct for t in trials)
    hit_rate = n_correct / n_total if n_total > 0 else 0.0
    
    hr_ci_lower, hr_ci_upper = wilson_confidence_interval(n_correct, n_total)
    p_val = binomial_test(n_correct, n_total, 0.5)
    dp, dp_ci_lower, dp_ci_upper = d_prime(hit_rate, n_total)
    mean_rt = sum(t.response_time_ms for t in trials) / n_total if n_total > 0 else 0.0
    
    # By-stimulus statistics
    by_stim: Dict[str, List[TrialData]] = defaultdict(list)
    for t in trials:
        by_stim[t.stimulus].append(t)
    
    stim_stats: Dict[str, StimulusStats] = {}
    for stim, stim_trials in by_stim.items():
        n = len(stim_trials)
        nc = sum(t.correct for t in stim_trials)
        hr = nc / n if n > 0 else 0.0
        hr_lo, hr_hi = wilson_confidence_interval(nc, n)
        pv = binomial_test(nc, n, 0.5)
        d, d_lo, d_hi = d_prime(hr, n)
        mrt = sum(t.response_time_ms for t in stim_trials) / n if n > 0 else 0.0
        
        stim_stats[stim] = StimulusStats(
            stimulus=stim,
            n_trials=n,
            n_correct=nc,
            hit_rate=hr,
            hit_rate_ci_lower=hr_lo,
            hit_rate_ci_upper=hr_hi,
            p_value=pv,
            d_prime=d,
            d_prime_ci_lower=d_lo,
            d_prime_ci_upper=d_hi,
            mean_response_time_ms=mrt
        )
    
    return OverallStats(
        n_trials=n_total,
        n_correct=n_correct,
        hit_rate=hit_rate,
        hit_rate_ci_lower=hr_ci_lower,
        hit_rate_ci_upper=hr_ci_upper,
        p_value=p_val,
        d_prime=dp,
        d_prime_ci_lower=dp_ci_lower,
        d_prime_ci_upper=dp_ci_upper,
        mean_response_time_ms=mean_rt,
        by_stimulus=stim_stats
    )


# ===========================================================================
# Output Formatters
# ===========================================================================

def print_summary(stats: OverallStats) -> None:
    """Print human-readable summary to console."""
    print()
    print("=" * 70)
    print("  ABX Listening Test — Statistical Analysis")
    print("=" * 70)
    print()
    
    # Overall results
    print("OVERALL RESULTS")
    print("-" * 40)
    print(f"  Trials:        {stats.n_trials}")
    print(f"  Correct:       {stats.n_correct}")
    print(f"  Hit rate:      {stats.hit_rate:.1%} [{stats.hit_rate_ci_lower:.1%}, {stats.hit_rate_ci_upper:.1%}] 95% CI")
    print(f"  p-value:       {stats.p_value:.4f} (binomial test, H0: 50%)")
    print(f"  d':            {stats.d_prime:.2f} [{stats.d_prime_ci_lower:.2f}, {stats.d_prime_ci_upper:.2f}] 95% CI")
    print(f"  Mean RT:       {stats.mean_response_time_ms:.0f} ms")
    print()
    
    # Interpretation
    print("INTERPRETATION")
    print("-" * 40)
    if stats.p_value > 0.05:
        print("  ✓ Hit rate NOT significantly different from chance (p > 0.05)")
        print("  ✓ Variable-cadence is perceptually TRANSPARENT")
    else:
        print("  ✗ Hit rate significantly different from chance (p ≤ 0.05)")
        print("  ✗ Listeners CAN distinguish between processing paths")
    
    # d' interpretation (Macmillan & Creelman guidelines)
    if abs(stats.d_prime) < 0.5:
        print(f"  d' = {stats.d_prime:.2f}: Negligible sensitivity (< 0.5)")
    elif abs(stats.d_prime) < 1.0:
        print(f"  d' = {stats.d_prime:.2f}: Low sensitivity (0.5–1.0)")
    elif abs(stats.d_prime) < 2.0:
        print(f"  d' = {stats.d_prime:.2f}: Medium sensitivity (1.0–2.0)")
    else:
        print(f"  d' = {stats.d_prime:.2f}: High sensitivity (> 2.0)")
    print()
    
    # By-stimulus breakdown
    print("BY STIMULUS")
    print("-" * 40)
    print(f"  {'Stimulus':<18} {'N':>4} {'Correct':>8} {'Hit%':>7} {'p':>8} {chr(100)+chr(39):>7}")
    print(f"  {'-'*18} {'-'*4} {'-'*8} {'-'*7} {'-'*8} {'-'*7}")
    
    for stim in sorted(stats.by_stimulus.keys()):
        s = stats.by_stimulus[stim]
        sig = '*' if s.p_value <= 0.05 else ' '
        print(f"  {s.stimulus:<18} {s.n_trials:>4} {s.n_correct:>8} {s.hit_rate:>6.1%} {s.p_value:>7.3f}{sig} {s.d_prime:>6.2f}")
    
    print()
    print("  * p ≤ 0.05")
    print()


def export_json(stats: OverallStats, filepath: str) -> None:
    """Export statistics to JSON for paper appendix."""
    import json
    
    data = {
        "overall": {
            "n_trials": stats.n_trials,
            "n_correct": stats.n_correct,
            "hit_rate": round(stats.hit_rate, 4),
            "hit_rate_95ci": [round(stats.hit_rate_ci_lower, 4), round(stats.hit_rate_ci_upper, 4)],
            "p_value": round(stats.p_value, 6),
            "d_prime": round(stats.d_prime, 3),
            "d_prime_95ci": [round(stats.d_prime_ci_lower, 3), round(stats.d_prime_ci_upper, 3)],
            "mean_response_time_ms": round(stats.mean_response_time_ms, 1)
        },
        "by_stimulus": {}
    }
    
    for stim, s in stats.by_stimulus.items():
        data["by_stimulus"][stim] = {
            "n_trials": s.n_trials,
            "n_correct": s.n_correct,
            "hit_rate": round(s.hit_rate, 4),
            "hit_rate_95ci": [round(s.hit_rate_ci_lower, 4), round(s.hit_rate_ci_upper, 4)],
            "p_value": round(s.p_value, 6),
            "d_prime": round(s.d_prime, 3),
            "d_prime_95ci": [round(s.d_prime_ci_lower, 3), round(s.d_prime_ci_upper, 3)],
            "mean_response_time_ms": round(s.mean_response_time_ms, 1)
        }
    
    with open(filepath, 'w') as f:
        json.dump(data, f, indent=2)
    
    print(f"JSON exported to: {filepath}")


def export_latex_table(stats: OverallStats) -> str:
    """Generate LaTeX table for paper inclusion."""
    lines = [
        r"\begin{table}[h]",
        r"\centering",
        r"\caption{ABX Listening Test Results — Variable-Cadence Dynamic EQ}",
        r"\label{tab:abx-results}",
        r"\begin{tabular}{lcccccc}",
        r"\toprule",
        r"Stimulus & $N$ & Correct & Hit Rate & 95\% CI & $p$-value & $d'$ \\",
        r"\midrule"
    ]
    
    for stim in sorted(stats.by_stimulus.keys()):
        s = stats.by_stimulus[stim]
        sig = r"$^*$" if s.p_value <= 0.05 else ""
        lines.append(
            f"{s.stimulus.replace('_', r'\_')} & {s.n_trials} & {s.n_correct} & "
            f"{s.hit_rate:.1%} & [{s.hit_rate_ci_lower:.1%}, {s.hit_rate_ci_upper:.1%}] & "
            f"{s.p_value:.3f}{sig} & {s.d_prime:.2f} \\\\"
        )
    
    lines.append(r"\midrule")
    lines.append(
        f"Overall & {stats.n_trials} & {stats.n_correct} & "
        f"{stats.hit_rate:.1%} & [{stats.hit_rate_ci_lower:.1%}, {stats.hit_rate_ci_upper:.1%}] & "
        f"{stats.p_value:.3f} & {stats.d_prime:.2f} \\\\"
    )
    
    lines.extend([
        r"\bottomrule",
        r"\end{tabular}",
        r"\begin{tablenotes}",
        r"\small",
        r"\item $^*$ $p \leq 0.05$; $d'$: sensitivity index (0 = chance, >1 = discriminable)",
        r"\end{tablenotes}",
        r"\end{table}"
    ])
    
    return "\n".join(lines)


# ===========================================================================
# Demo/Test Mode
# ===========================================================================

def generate_demo_results() -> OverallStats:
    """Generate simulated results for demonstration."""
    import random
    random.seed(42)
    
    stimuli = ["sustained_sine", "drum_loop", "vocal_sim", "full_mix"]
    trials = []
    
    for i, stim in enumerate(stimuli):
        for t in range(10):  # 10 trials per stimulus
            # Simulate ~50% accuracy (can't tell the difference)
            correct = 1 if random.random() < 0.52 else 0  # Slightly above chance
            trials.append(TrialData(
                trial_num=i * 10 + t + 1,
                stimulus=stim,
                x_is_a=random.randint(0, 1),
                user_response=1 if (random.random() < 0.5) else 2,
                correct=correct,
                response_time_ms=random.gauss(2500, 800),
                timestamp="2024-01-01 12:00:00"
            ))
    
    return analyze_trials(trials)


# ===========================================================================
# Main
# ===========================================================================

def main():
    if len(sys.argv) < 2:
        print("ABXAnalysis.py — Statistical analysis for ABX listening tests")
        print()
        print("Usage:")
        print("  python3 ABXAnalysis.py <results.csv>         Analyze results file")
        print("  python3 ABXAnalysis.py <results.csv> --json  Also export JSON")
        print("  python3 ABXAnalysis.py --demo                Run with simulated data")
        print()
        print("CSV format expected:")
        print("  trial,stimulus,x_is_a,user_response,correct,response_time_ms,timestamp")
        print()
        sys.exit(1)
    
    # Demo mode
    if sys.argv[1] == "--demo":
        print("Running with simulated data (demonstrating expected results)...")
        stats = generate_demo_results()
        print_summary(stats)
        print()
        print("LaTeX table for paper:")
        print("-" * 40)
        print(export_latex_table(stats))
        sys.exit(0)
    
    # Load and analyze real data
    filepath = sys.argv[1]
    try:
        trials = load_results(filepath)
        print(f"Loaded {len(trials)} trials from {filepath}")
    except FileNotFoundError:
        print(f"Error: File not found: {filepath}")
        sys.exit(1)
    except Exception as e:
        print(f"Error loading file: {e}")
        sys.exit(1)
    
    stats = analyze_trials(trials)
    print_summary(stats)
    
    # Optional JSON export
    if "--json" in sys.argv:
        json_path = filepath.replace(".csv", "_analysis.json")
        export_json(stats, json_path)
    
    # Print LaTeX table
    print()
    print("LaTeX table for paper:")
    print("-" * 40)
    print(export_latex_table(stats))


if __name__ == "__main__":
    main()
