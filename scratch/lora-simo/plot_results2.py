"""
plot_results.py
───────────────
Plota os resultados da simulação ns-3 SIMO LoRa.

Gráficos gerados:
  1. ber_all.png      — BER × SNR com todas as técnicas no mesmo eixo
  2. ber_subplot.png  — BER × SNR em subplots por técnica (mesmo estilo do Python original)
  3. gain_plot.png    — Ganho simulado vs teórico 10·log10(L)

Uso:
    python3 plot_results.py
    python3 plot_results.py --ber=lora_ber_results.csv --gain=lora_gain_results.csv
"""

import argparse
import os
import sys

import numpy as np
import pandas as pd
import matplotlib.pyplot as plt
import matplotlib.ticker as ticker
from matplotlib.lines import Line2D

# ═════════════════════════════════════════════════════════════════
#  Paleta — idêntica ao script Python original
# ═════════════════════════════════════════════════════════════════
COLORS  = {"SC": "green", "EGC": "blue", "MRC": "red"}
STYLES  = {1: "-",  2: "--", 3: ":",  4: "-.", 5: (0, (3, 1, 1, 1))}
MARKERS = {1: "o",  2: "s",  3: "^",  4: "D",  5: "x"}
MSIZE   = 5
LW      = 2.0

# ordem de exibição
TECH_ORDER = ["SC", "EGC", "MRC"]

# ─── Configuração global matplotlib ──────────────────────────────
plt.rcParams.update({
    "font.family":      "DejaVu Sans",
    "axes.titlesize":   12,
    "axes.labelsize":   11,
    "legend.fontsize":   9,
    "xtick.labelsize":   9,
    "ytick.labelsize":   9,
    "axes.grid":        True,
    "grid.linestyle":   "--",
    "grid.linewidth":   0.5,
    "grid.alpha":       0.7,
    "figure.dpi":       100,
})


# ═════════════════════════════════════════════════════════════════
#  Figura 1 — BER × SNR  (todas as técnicas, mesmo eixo)
#  Formato mais próximo do script Python original
# ═════════════════════════════════════════════════════════════════
def plot_ber_all(df: pd.DataFrame, out: str = "ber_all.png"):
    Ls    = sorted(df["L"].unique())
    techs = [t for t in TECH_ORDER if t in df["technique"].unique()]

    fig, ax = plt.subplots(figsize=(10, 6))

    for tech in techs:
        sub = df[df["technique"] == tech]
        for L in Ls:
            row = sub[sub["L"] == L].sort_values("SNR_dB")
            ber = row["BER"].clip(lower=1e-6)
            ax.semilogy(
                row["SNR_dB"], ber,
                linestyle  = STYLES[L],
                marker     = MARKERS[L],
                color      = COLORS[tech],
                markersize = MSIZE,
                linewidth  = LW,
                label      = f"{tech}, L={L}",
            )

    ax.set_xlabel("$\\overline{SNR}$ (dB)")
    ax.set_ylabel("BER")
    ax.set_ylim(1e-5, 1.0)
    ax.set_title(
        f"BER × SNR — LoRa SIMO (SF=10, Canal Rayleigh+AWGN)\n"
        f"SC vs EGC vs MRC  (L = 1 → {max(Ls)})"
    )
    ax.yaxis.set_major_locator(ticker.LogLocator(base=10, numticks=6))
    ax.yaxis.set_minor_locator(ticker.LogLocator(base=10, subs=np.arange(2, 10) * 0.1))

    # Legenda em duas colunas: col1 = técnica (cor), col2 = L (estilo)
    color_handles = [
        Line2D([0], [0], color=COLORS[t], linewidth=LW, label=t)
        for t in techs
    ]
    style_handles = [
        Line2D([0], [0], color="gray", linestyle=STYLES[L],
               marker=MARKERS[L], markersize=MSIZE, linewidth=LW,
               label=f"L={L}")
        for L in Ls
    ]
    leg1 = ax.legend(handles=color_handles, title="Técnica",
                     loc="upper right", bbox_to_anchor=(1.0, 1.0))
    ax.add_artist(leg1)
    ax.legend(handles=style_handles, title="Antenas",
              loc="upper right", bbox_to_anchor=(1.0, 0.72))

    plt.tight_layout()
    plt.savefig(out, dpi=300, bbox_inches="tight")
    plt.close()
    print(f"✅ BER (único eixo) salvo: {out}")


# ═════════════════════════════════════════════════════════════════
#  Figura 2 — BER × SNR em subplots por técnica
#  Mesmo layout do script Python original (1 subplot por técnica)
# ═════════════════════════════════════════════════════════════════
def plot_ber_subplot(df: pd.DataFrame, out: str = "ber_subplot.png"):
    Ls    = sorted(df["L"].unique())
    techs = [t for t in TECH_ORDER if t in df["technique"].unique()]

    fig, axes = plt.subplots(1, len(techs),
                             figsize=(6 * len(techs), 5),
                             sharey=True)
    if len(techs) == 1:
        axes = [axes]

    for ax, tech in zip(axes, techs):
        sub = df[df["technique"] == tech]
        for L in Ls:
            row = sub[sub["L"] == L].sort_values("SNR_dB")
            ber = row["BER"].clip(lower=1e-6)
            ax.semilogy(
                row["SNR_dB"], ber,
                linestyle  = STYLES[L],
                marker     = MARKERS[L],
                color      = COLORS[tech],
                markersize = MSIZE,
                linewidth  = LW,
                label      = f"L={L}",
            )

        ax.set_title(tech, fontweight="bold")
        ax.set_xlabel("$\\overline{SNR}$ (dB)")
        if ax == axes[0]:
            ax.set_ylabel("BER")
        ax.set_ylim(1e-5, 1.0)
        ax.yaxis.set_major_locator(ticker.LogLocator(base=10, numticks=6))
        ax.yaxis.set_minor_locator(
            ticker.LogLocator(base=10, subs=np.arange(2, 10) * 0.1))
        ax.legend(loc="upper right", title="Antenas")

    fig.suptitle(
        "BER × SNR — LoRa SIMO (SF=10, Canal Rayleigh+AWGN)\n"
        "SC vs EGC vs MRC",
        fontsize=13
    )
    plt.tight_layout()
    plt.savefig(out, dpi=300, bbox_inches="tight")
    plt.close()
    print(f"✅ BER (subplots) salvo: {out}")


# ═════════════════════════════════════════════════════════════════
#  Figura 3 — Ganho simulado × teórico  +  ganho de diversidade
# ═════════════════════════════════════════════════════════════════
def plot_gain(df: pd.DataFrame, out: str = "gain_plot.png"):
    techs = [t for t in TECH_ORDER if t in df["technique"].unique()]
    Ls    = sorted(df["L"].unique())
    L_arr = np.array(Ls)

    fig, ax = plt.subplots(figsize=(8, 5))

    # ── Teórico 10·log10(L)  (AWGN, referência) ──────────────────
    theo = 10 * np.log10(L_arr)
    ax.plot(L_arr, theo,
            color="black", linestyle="--", linewidth=2,
            marker="o", markersize=6,
            label="Teórico AWGN: 10·log₁₀(L)")

    # ── Teórico Rayleigh MRC: ~(2L-1) em escala log (aproximação) ─
    # Ganho de diversidade Rayleigh ≈ soma de 1/k para k=1..L em SNR alto
    rayleigh_approx = np.array([
        10 * np.log10(sum(1/k for k in range(1, L+1)) * L)
        for L in Ls
    ])
    ax.plot(L_arr, rayleigh_approx,
            color="gray", linestyle=":", linewidth=1.8,
            marker="x", markersize=7,
            label="Aprox. Rayleigh MRC teórico")

    # ── Simulado por técnica ───────────────────────────────────────
    for tech in techs:
        sub = df[df["technique"] == tech].sort_values("L")
        ax.plot(sub["L"], sub["gain_sim_dB"],
                color      = COLORS[tech],
                linestyle  = "-",
                marker     = "o",
                markersize = 8,
                linewidth  = LW,
                label      = f"{tech} (simulado)")

        # Anota o valor no último ponto
        last = sub.iloc[-1]
        ax.annotate(f"{last['gain_sim_dB']:.1f} dB",
                    xy=(last["L"], last["gain_sim_dB"]),
                    xytext=(4, 2), textcoords="offset points",
                    fontsize=8, color=COLORS[tech])

    ax.set_xlabel("Número de antenas (L)", fontsize=12)
    ax.set_ylabel("Ganho em SNR (dB)",     fontsize=12)
    ax.set_title(
        "Ganho de Diversidade SIMO LoRa\n"
        "Simulado vs Teórico  [BER alvo = 10⁻³, Canal Rayleigh]",
        fontsize=12
    )
    ax.set_xticks(Ls)
    ax.legend(fontsize=10)

    plt.tight_layout()
    plt.savefig(out, dpi=300, bbox_inches="tight")
    plt.close()
    print(f"✅ Ganho salvo: {out}")


# ═════════════════════════════════════════════════════════════════
#  Figura 4 — Extensão de cobertura
# ═════════════════════════════════════════════════════════════════
def plot_coverage(df: pd.DataFrame, out: str = "coverage_plot.png",
                  n_exp: float = 2.0):
    techs = [t for t in TECH_ORDER if t in df["technique"].unique()]
    Ls    = sorted(df["L"].unique())

    fig, ax = plt.subplots(figsize=(8, 5))

    for tech in techs:
        sub = df[df["technique"] == tech].sort_values("L")
        cov = 10 ** (sub["gain_sim_dB"].values / (10.0 * n_exp))
        ax.plot(sub["L"].values, cov,
                color      = COLORS[tech],
                linestyle  = "-",
                marker     = "o",
                markersize = 8,
                linewidth  = LW,
                label      = f"{tech}")
        # Anota último ponto
        ax.annotate(f"{cov[-1]:.1f}×",
                    xy=(sub["L"].values[-1], cov[-1]),
                    xytext=(4, 2), textcoords="offset points",
                    fontsize=8, color=COLORS[tech])

    ax.set_xlabel("Número de antenas (L)", fontsize=12)
    ax.set_ylabel("Extensão de cobertura (×d₀)",  fontsize=12)
    ax.set_title(
        f"Extensão de Cobertura SIMO LoRa\n"
        f"Modelo: 91.22 + 10·{n_exp}·log₁₀(d/d₀)  [BER alvo = 10⁻³]",
        fontsize=12
    )
    ax.set_xticks(Ls)
    ax.legend(fontsize=10)

    plt.tight_layout()
    plt.savefig(out, dpi=300, bbox_inches="tight")
    plt.close()
    print(f"✅ Cobertura salvo: {out}")


# ═════════════════════════════════════════════════════════════════
#  Tabela resumo
# ═════════════════════════════════════════════════════════════════
def print_table(df: pd.DataFrame):
    print("\n" + "═"*65)
    print(f"{'Técnica':<8} {'L':<4} {'SNR@BER1e-3':>12} "
          f"{'Ganho Sim':>12} {'10log10(L)':>12}")
    print("─"*65)
    techs = [t for t in TECH_ORDER if t in df["technique"].unique()]
    for tech in techs:
        sub = df[df["technique"] == tech].sort_values("L")
        for _, row in sub.iterrows():
            print(f"{row['technique']:<8} {int(row['L']):<4} "
                  f"{row['SNR_at_BER1e-3_dB']:>12.2f} "
                  f"{row['gain_sim_dB']:>12.2f} "
                  f"{row['gain_theo_10logN_dB']:>12.2f}")
    print("═"*65)


# ═════════════════════════════════════════════════════════════════
#  Main
# ═════════════════════════════════════════════════════════════════
if __name__ == "__main__":
    parser = argparse.ArgumentParser()
    parser.add_argument("--ber",  default="lora_ber_results.csv")
    parser.add_argument("--gain", default="lora_gain_results.csv")
    args = parser.parse_args()

    if not os.path.exists(args.ber):
        print(f"[ERRO] Não encontrado: {args.ber}")
        sys.exit(1)
    if not os.path.exists(args.gain):
        print(f"[ERRO] Não encontrado: {args.gain}")
        sys.exit(1)

    ber_df  = pd.read_csv(args.ber)
    gain_df = pd.read_csv(args.gain)

    plot_ber_all    (ber_df)
    plot_ber_subplot(ber_df)
    plot_gain       (gain_df)
    plot_coverage   (gain_df)
    print_table     (gain_df)

    print("\n✅ Todos os gráficos gerados.")
