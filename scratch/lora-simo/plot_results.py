"""
plot_results.py
───────────────
Lê os CSVs gerados pela simulação ns-3 e plota:
  1. BER × SNR  (SC / EGC / MRC, L = 1..5)
  2. Ganho simulado vs teórico 10·log10(L)

Uso:
    python3 plot_results.py
"""

import pandas as pd
import numpy as np
import matplotlib.pyplot as plt
import matplotlib.ticker as ticker
import os

# ─── Arquivos de entrada ──────────────────────────────────────────
BER_CSV  = "lora_ber_results.csv"
GAIN_CSV = "lora_gain_results.csv"

# ─── Paletas ──────────────────────────────────────────────────────
COLORS = {"SC": "green", "EGC": "blue", "MRC": "red"}
STYLES = {1: "-", 2: "--", 3: ":", 4: "-.", 5: (0, (3,1,1,1))}
MARKERS= {1: "o", 2: "s", 3: "^", 4: "D", 5: "x"}

# ═════════════════════════════════════════════════════════════════
#  Figura 1 — BER × SNR
# ═════════════════════════════════════════════════════════════════
def plot_ber(df: pd.DataFrame, out: str = "ber_plot.png"):
    techniques = df["technique"].unique()
    Ls         = sorted(df["L"].unique())

    fig, axes = plt.subplots(1, len(techniques),
                             figsize=(6 * len(techniques), 5),
                             sharey=True)

    if len(techniques) == 1:
        axes = [axes]

    for ax, tech in zip(axes, techniques):
        sub = df[df["technique"] == tech]
        for L in Ls:
            row = sub[sub["L"] == L].sort_values("SNR_dB")
            ber = row["BER"].clip(lower=1e-6)   # evita log(0)
            ax.semilogy(row["SNR_dB"], ber,
                        linestyle=STYLES[L],
                        marker=MARKERS[L],
                        color=COLORS[tech],
                        markersize=5,
                        linewidth=1.8,
                        label=f"L={L}")

        ax.set_title(f"{tech}", fontsize=13, fontweight="bold")
        ax.set_xlabel("SNR (dB)", fontsize=11)
        ax.set_ylabel("BER",      fontsize=11)
        ax.set_ylim(1e-5, 1.0)
        ax.grid(True, which="both", linestyle="--", linewidth=0.5, alpha=0.7)
        ax.legend(fontsize=9, loc="upper right")
        ax.yaxis.set_major_locator(ticker.LogLocator(base=10, numticks=6))

    fig.suptitle(
        f"BER × SNR — LoRa SIMO (SF=10, Canal Rayleigh+AWGN)\nSC vs EGC vs MRC",
        fontsize=13, y=1.02
    )
    plt.tight_layout()
    plt.savefig(out, dpi=300, bbox_inches="tight")
    plt.close()
    print(f"✅ BER plot salvo: {out}")


# ═════════════════════════════════════════════════════════════════
#  Figura 2 — Ganho simulado × teórico 10·log10(L)
# ═════════════════════════════════════════════════════════════════
def plot_gain(df: pd.DataFrame, out: str = "gain_plot.png"):
    techniques = df["technique"].unique()
    Ls         = sorted(df["L"].unique())
    L_arr      = np.array(Ls)

    fig, ax = plt.subplots(figsize=(8, 5))

    # Curva teórica (única)
    theo = 10 * np.log10(L_arr)
    ax.plot(L_arr, theo, "k--", linewidth=2, label="Teórico: 10·log₁₀(L)")

    for tech in techniques:
        sub      = df[df["technique"] == tech].sort_values("L")
        gain_sim = sub["gain_sim_dB"].values
        ax.plot(sub["L"], gain_sim,
                color=COLORS[tech],
                marker="o",
                linewidth=2,
                markersize=7,
                label=f"{tech} (simulado)")

    ax.set_xlabel("Número de antenas (L)", fontsize=12)
    ax.set_ylabel("Ganho em SNR (dB)",     fontsize=12)
    ax.set_title("Ganho de Diversidade SIMO LoRa\n"
                 "Simulado vs Teórico 10·log₁₀(L)  [BER alvo = 10⁻³]",
                 fontsize=12)
    ax.set_xticks(Ls)
    ax.grid(True, linestyle="--", linewidth=0.5, alpha=0.7)
    ax.legend(fontsize=10)

    plt.tight_layout()
    plt.savefig(out, dpi=300, bbox_inches="tight")
    plt.close()
    print(f"✅ Ganho plot salvo: {out}")


# ═════════════════════════════════════════════════════════════════
#  Tabela resumo no terminal
# ═════════════════════════════════════════════════════════════════
def print_gain_table(df: pd.DataFrame):
    print("\n" + "═"*65)
    print(f"{'Técnica':<8} {'L':<4} {'SNR@BER1e-3':>12} {'Ganho Sim':>12} {'10log10(L)':>12}")
    print("─"*65)
    for _, row in df.iterrows():
        print(f"{row['technique']:<8} {int(row['L']):<4} "
              f"{row['SNR_at_BER1e-3_dB']:>12.2f} "
              f"{row['gain_sim_dB']:>12.2f} "
              f"{row['gain_theo_10logN_dB']:>12.2f}")
    print("═"*65)


# ─── Main ─────────────────────────────────────────────────────────
if __name__ == "__main__":
    if not os.path.exists(BER_CSV):
        print(f"[ERRO] Arquivo não encontrado: {BER_CSV}")
        print("Execute a simulação ns-3 primeiro.")
        exit(1)

    ber_df  = pd.read_csv(BER_CSV)
    gain_df = pd.read_csv(GAIN_CSV)

    plot_ber(ber_df)
    plot_gain(gain_df)
    print_gain_table(gain_df)
