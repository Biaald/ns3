"""
plot_sir_results.py
───────────────────
Plota BER × SIR e ganho de diversidade com SIR.
Mesmo estilo visual do gráfico de referência (cores, marcadores, escala).

Uso:
    python3 plot_sir_results.py
    python3 plot_sir_results.py --ber=lora_sir_ber_results.csv \
                                --gain=lora_sir_gain_results.csv \
                                --pos=lora_ed_positions.csv
"""

import argparse, os, sys
import numpy as np
import pandas as pd
import matplotlib.pyplot as plt
import matplotlib.ticker as ticker
from matplotlib.lines import Line2D

# ── Paleta ────────────────────────────────────────────────────────
COLORS  = {"SC": "green", "EGC": "blue", "MRC": "red"}
STYLES  = {1:(0,()), 2:(0,(8,3)), 3:(0,(2,2)), 4:(0,(8,2,2,2)), 5:(0,(8,2,2,2,2,2))}
MARKERS = {1:"None", 2:"None", 3:"None", 4:"o", 5:"x"}
MSIZES  = {1:0, 2:0, 3:0, 4:6, 5:7}
LW = 1.8
TECH_ORDER = ["SC","EGC","MRC"]

plt.rcParams.update({
    "font.family":"DejaVu Sans","axes.titlesize":13,"axes.labelsize":12,
    "legend.fontsize":9,"xtick.labelsize":10,"ytick.labelsize":10,
})

# ═════════════════════════════════════════════════════════════════
#  Fig 1 — BER × SIR
# ═════════════════════════════════════════════════════════════════
def plot_ber_sir(df, out="ber_sir.png"):
    Ls    = sorted(df["L"].unique())
    techs = [t for t in TECH_ORDER if t in df["technique"].unique()]

    fig, ax = plt.subplots(figsize=(14,7))
    for tech in techs:
        sub = df[df["technique"]==tech]
        for L in Ls:
            row = sub[sub["L"]==L].sort_values("SIR_dB")
            ber = row["BER"].clip(lower=1e-6)
            ax.semilogy(row["SIR_dB"], ber,
                        linestyle=STYLES[L], color=COLORS[tech],
                        linewidth=LW, marker=MARKERS[L],
                        markersize=MSIZES[L], markevery=3,
                        markeredgewidth=1.5, label=f"{tech}, L={L}")

    ax.set_xlim(df["SIR_dB"].min()-1, df["SIR_dB"].max()+1)
    ax.set_ylim(1e-5, 1.0)
    ax.set_xlabel("SIR (dB)")
    ax.set_ylabel("BER")
    ax.set_title("BER × SIR (SF=10) — Simulação ns-3\n"
                 "SC vs EGC vs MRC  (SISO → SIMO 1×5)  |  "
                 "Canal Rayleigh+AWGN com Interferência Real")
    ax.grid(True, which="major", linestyle="--", lw=0.6, alpha=0.8)
    ax.grid(True, which="minor", linestyle=":",  lw=0.3, alpha=0.5)
    ax.yaxis.set_major_locator(ticker.LogLocator(base=10, numticks=6))
    ax.yaxis.set_minor_locator(
        ticker.LogLocator(base=10, subs=np.arange(2,10)*0.1, numticks=50))

    # Legenda 3 colunas
    color_h = [Line2D([0],[0],color=COLORS[t],lw=LW,label=t) for t in techs]
    style_h = [Line2D([0],[0],color="gray",linestyle=STYLES[L],
                      marker=MARKERS[L],markersize=MSIZES[L],lw=LW,
                      label=f"L={L}") for L in Ls]
    leg1 = ax.legend(handles=color_h, title="Técnica",
                     loc="upper right", bbox_to_anchor=(1.0,1.0))
    ax.add_artist(leg1)
    ax.legend(handles=style_h, title="Antenas",
              loc="upper right", bbox_to_anchor=(1.0,0.72))

    plt.tight_layout()
    plt.savefig(out, dpi=300, bbox_inches="tight")
    plt.close()
    print(f"✅ {out}")

# ═════════════════════════════════════════════════════════════════
#  Fig 2 — Ganho SIR
# ═════════════════════════════════════════════════════════════════
def plot_gain_sir(df, out="gain_sir.png"):
    techs = [t for t in TECH_ORDER if t in df["technique"].unique()]
    Ls    = sorted(df["L"].unique())
    La    = np.array(Ls)

    fig, ax = plt.subplots(figsize=(9,5))
    theo = 10*np.log10(La)
    ax.plot(La, theo, "k--", lw=2, marker="o", ms=6,
            label="Teórico AWGN: 10·log₁₀(L)")

    for tech in techs:
        sub = df[df["technique"]==tech].sort_values("L")
        ax.plot(sub["L"], sub["gain_sim_dB"],
                color=COLORS[tech], lw=LW, marker="o", ms=8,
                label=f"{tech} (simulado SIR)")
        last = sub.iloc[-1]
        ax.annotate(f"{last['gain_sim_dB']:.1f} dB",
                    xy=(last["L"],last["gain_sim_dB"]),
                    xytext=(4,3), textcoords="offset points",
                    fontsize=8, color=COLORS[tech])

    ax.set_xlabel("Número de antenas (L)")
    ax.set_ylabel("Ganho em SIR (dB)")
    ax.set_title("Ganho de Diversidade SIMO LoRa — Métrica SIR\n"
                 "Simulado (Rayleigh + interferência real) vs Teórico AWGN  "
                 "[BER alvo = 10⁻³]")
    ax.set_xticks(Ls)
    ax.grid(True, linestyle="--", lw=0.5, alpha=0.7)
    ax.legend(fontsize=10)
    plt.tight_layout()
    plt.savefig(out, dpi=300, bbox_inches="tight")
    plt.close()
    print(f"✅ {out}")

# ═════════════════════════════════════════════════════════════════
#  Fig 3 — Topologia: posições dos EDs com SIR colorido
# ═════════════════════════════════════════════════════════════════
def plot_topology(pos_df, out="topology.png"):
    SF_COLORS = {7:"#e41a1c", 8:"#ff7f00", 9:"#ffff33",
                 10:"#4daf4a", 11:"#377eb8", 12:"#984ea3"}

    fig, ax = plt.subplots(figsize=(8,8))
    for sf, grp in pos_df.groupby("sf"):
        ax.scatter(grp["x_m"]/1000, grp["y_m"]/1000,
                   color=SF_COLORS.get(sf,"gray"),
                   label=f"SF{sf}", s=60, zorder=3, edgecolors="black", lw=0.5)

    ax.scatter(0, 0, marker="^", s=300, color="black",
               zorder=5, label="Gateway")
    circle = plt.Circle((0,0), 5.0, fill=False,
                         color="gray", linestyle="--", lw=1)
    ax.add_patch(circle)

    ax.set_xlabel("x (km)")
    ax.set_ylabel("y (km)")
    ax.set_title("Topologia da Rede LoRa\n30 EDs + 1 Gateway  |  Raio = 5 km")
    ax.set_aspect("equal")
    ax.legend(fontsize=9, loc="upper right")
    ax.grid(True, linestyle="--", lw=0.4, alpha=0.6)
    plt.tight_layout()
    plt.savefig(out, dpi=300, bbox_inches="tight")
    plt.close()
    print(f"✅ {out}")

# ═════════════════════════════════════════════════════════════════
#  Tabela resumo
# ═════════════════════════════════════════════════════════════════
def print_table(df):
    print("\n" + "═"*68)
    print(f"{'Técnica':<8} {'L':<4} {'SIR@BER1e-3':>12} "
          f"{'Ganho Sim':>12} {'10log10(L)':>12}")
    print("─"*68)
    for tech in [t for t in TECH_ORDER if t in df["technique"].unique()]:
        for _, row in df[df["technique"]==tech].sort_values("L").iterrows():
            print(f"{row['technique']:<8} {int(row['L']):<4} "
                  f"{row['SIR_at_BER1e-3_dB']:>12.2f} "
                  f"{row['gain_sim_dB']:>12.2f} "
                  f"{row['gain_theo_10logN_dB']:>12.2f}")
    print("═"*68)

# ─── Main ─────────────────────────────────────────────────────────
if __name__ == "__main__":
    parser = argparse.ArgumentParser()
    parser.add_argument("--ber",  default="lora_sir_ber_results.csv")
    parser.add_argument("--gain", default="lora_sir_gain_results.csv")
    parser.add_argument("--pos",  default="lora_ed_positions.csv")
    args = parser.parse_args()

    for f in [args.ber, args.gain]:
        if not os.path.exists(f):
            print(f"[ERRO] Não encontrado: {f}"); sys.exit(1)

    ber_df  = pd.read_csv(args.ber)
    gain_df = pd.read_csv(args.gain)

    plot_ber_sir (ber_df)
    plot_gain_sir(gain_df)
    print_table  (gain_df)

    if os.path.exists(args.pos):
        pos_df = pd.read_csv(args.pos)
        plot_topology(pos_df)
    else:
        print(f"⚠ Posições não encontradas: {args.pos}")

    print("\n✅ Concluído.")
