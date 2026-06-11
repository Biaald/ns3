"""
plot_results.py
───────────────
Plota BER × SNR no mesmo estilo do gráfico Python original:
  - Todas as técnicas e L no mesmo eixo
  - Cores: SC=verde, EGC=azul, MRC=vermelho
  - Estilos de linha por L: sólido, tracejado, pontilhado, traço-ponto, traço-ponto-ponto
  - Marcadores por L: nenhum, nenhum, nenhum, círculo, x
  - Escala x: -30 a +12 dB | escala y: 1e-5 a 1
  - Legenda em 3 colunas (uma por técnica)
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
#  Paleta — idêntica ao gráfico de referência
# ═════════════════════════════════════════════════════════════════
COLORS  = {"SC": "green", "EGC": "blue", "MRC": "red"}

# Estilos de linha por L (igual ao original)
STYLES  = {
    1: (0, ()),           # sólido
    2: (0, (8, 3)),       # tracejado longo
    3: (0, (2, 2)),       # pontilhado
    4: (0, (8, 2, 2, 2)), # traço-ponto
    5: (0, (8, 2, 2, 2, 2, 2)),  # traço-ponto-ponto
}

# Marcadores por L (só L=4 e L=5 têm marcador, como no original)
MARKERS     = {1: "None", 2: "None", 3: "None", 4: "o", 5: "x"}
MARKERSIZES = {1: 0,      2: 0,      3: 0,      4: 6,   5: 7}
MARKEVERY   = 3   # a cada N pontos coloca o marcador

LW = 1.8

TECH_ORDER = ["SC", "EGC", "MRC"]

plt.rcParams.update({
    "font.family":    "DejaVu Sans",
    "axes.titlesize": 13,
    "axes.labelsize": 12,
    "legend.fontsize": 9,
    "xtick.labelsize": 10,
    "ytick.labelsize": 10,
    "figure.dpi":     100,
})


# ═════════════════════════════════════════════════════════════════
#  Gráfico principal — mesmo visual do original
# ═════════════════════════════════════════════════════════════════
def plot_ber(df: pd.DataFrame, out: str = "ber_all.png"):
    Ls    = sorted(df["L"].unique())
    techs = [t for t in TECH_ORDER if t in df["technique"].unique()]

    fig, ax = plt.subplots(figsize=(14, 7))

    for tech in techs:
        sub = df[df["technique"] == tech]
        for L in Ls:
            row = sub[sub["L"] == L].sort_values("SNR_dB")
            ber = row["BER"].clip(lower=1e-6)
            snr = row["SNR_dB"].values

            ax.semilogy(
                snr, ber,
                linestyle  = STYLES[L],
                color      = COLORS[tech],
                linewidth  = LW,
                marker     = MARKERS[L],
                markersize = MARKERSIZES[L],
                markevery  = MARKEVERY,
                markeredgewidth = 1.5,
                label      = f"{tech}, L={L}",
            )

    # ── Eixos ─────────────────────────────────────────────────────
    ax.set_xlim(-30, 12)
    ax.set_ylim(1e-5, 1.0)
    ax.set_xlabel("SNR (dB)", fontsize=12)
    ax.set_ylabel("BER",      fontsize=12)
    ax.set_title(
        "BER × SNR (SF=10) — Simulação ns-3\n"
        "SC vs EGC vs MRC  (SISO → SIMO 1×5)  |  Canal Rayleigh+AWGN",
        fontsize=13
    )

    # ── Grid ──────────────────────────────────────────────────────
    ax.grid(True, which="major", linestyle="--", linewidth=0.6, alpha=0.8)
    ax.grid(True, which="minor", linestyle=":",  linewidth=0.3, alpha=0.5)
    ax.yaxis.set_major_locator(ticker.LogLocator(base=10, numticks=6))
    ax.yaxis.set_minor_locator(
        ticker.LogLocator(base=10, subs=np.arange(2, 10) * 0.1, numticks=50))

    # ── Legenda em 3 colunas (SC | EGC | MRC) ─────────────────────
    # Agrupa handles por técnica para ficar igual ao original
    handles_by_tech = {t: [] for t in techs}
    for tech in techs:
        for L in Ls:
            handles_by_tech[tech].append(
                Line2D([0], [0],
                       color      = COLORS[tech],
                       linestyle  = STYLES[L],
                       linewidth  = LW,
                       marker     = MARKERS[L],
                       markersize = MARKERSIZES[L],
                       label      = f"{tech}, L={L}")
            )

    # Une todos os handles na ordem SC→EGC→MRC, L=1..5 por coluna
    all_handles = []
    all_labels  = []
    for tech in techs:
        for h in handles_by_tech[tech]:
            all_handles.append(h)
            all_labels.append(h.get_label())

    ax.legend(
        handles  = all_handles,
        labels   = all_labels,
        ncol     = len(techs),
        loc      = "upper right",
        framealpha = 0.9,
        fontsize = 9,
        handlelength = 2.5,
    )

    plt.tight_layout()
    plt.savefig(out, dpi=300, bbox_inches="tight")
    plt.close()
    print(f"✅ Salvo: {out}")


# ═════════════════════════════════════════════════════════════════
#  Gráfico de ganho
# ═════════════════════════════════════════════════════════════════
def plot_gain(df: pd.DataFrame, out: str = "gain_plot.png"):
    techs = [t for t in TECH_ORDER if t in df["technique"].unique()]
    Ls    = sorted(df["L"].unique())
    L_arr = np.array(Ls)

    fig, ax = plt.subplots(figsize=(9, 5))

    # Teórico AWGN
    theo = 10 * np.log10(L_arr)
    ax.plot(L_arr, theo, color="black", linestyle="--", linewidth=2,
            marker="o", markersize=6, label="Teórico AWGN: 10·log₁₀(L)")

    for tech in techs:
        sub = df[df["technique"] == tech].sort_values("L")
        ax.plot(sub["L"], sub["gain_sim_dB"],
                color=COLORS[tech], linestyle="-",
                marker="o", markersize=8, linewidth=LW,
                label=f"{tech} (simulado)")
        last = sub.iloc[-1]
        ax.annotate(f"{last['gain_sim_dB']:.1f} dB",
                    xy=(last["L"], last["gain_sim_dB"]),
                    xytext=(4, 3), textcoords="offset points",
                    fontsize=8, color=COLORS[tech])

    ax.set_xlabel("Número de antenas (L)", fontsize=12)
    ax.set_ylabel("Ganho em SNR (dB)",     fontsize=12)
    ax.set_title(
        "Ganho de Diversidade SIMO LoRa\n"
        "Simulado (Rayleigh) vs Teórico AWGN  [BER alvo = 10⁻³]",
        fontsize=12)
    ax.set_xticks(Ls)
    ax.grid(True, linestyle="--", linewidth=0.5, alpha=0.7)
    ax.legend(fontsize=10)

    plt.tight_layout()
    plt.savefig(out, dpi=300, bbox_inches="tight")
    plt.close()
    print(f"✅ Salvo: {out}")


# ═════════════════════════════════════════════════════════════════
#  Tabela resumo
# ═════════════════════════════════════════════════════════════════
def print_table(df: pd.DataFrame):
    print("\n" + "═"*65)
    print(f"{'Técnica':<8} {'L':<4} {'SNR@BER1e-3':>12} "
          f"{'Ganho Sim':>12} {'10log10(L)':>12}")
    print("─"*65)
    for tech in [t for t in TECH_ORDER if t in df["technique"].unique()]:
        for _, row in df[df["technique"] == tech].sort_values("L").iterrows():
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

    for f in [args.ber, args.gain]:
        if not os.path.exists(f):
            print(f"[ERRO] Não encontrado: {f}")
            sys.exit(1)

    ber_df  = pd.read_csv(args.ber)
    gain_df = pd.read_csv(args.gain)

    plot_ber (ber_df)
    plot_gain(gain_df)
    print_table(gain_df)

    print("\n✅ Concluído.")
