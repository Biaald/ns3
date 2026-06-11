import pandas as pd
import matplotlib
matplotlib.use('Agg')

import matplotlib.pyplot as plt
from pathlib import Path

# ==========================
# Arquivo CSV
# ==========================
csv_file = "lora_network_pdr_all7.csv"

if not Path(csv_file).exists():
    print(f"ERRO: arquivo {csv_file} não encontrado")
    exit()

# ==========================
# Ler CSV
# ==========================
df = pd.read_csv(csv_file)

print("\n=== Dados carregados ===")
print(df)

# Criar pasta para gráficos
output_dir = Path("plots_result_7_metade")
output_dir.mkdir(exist_ok=True)

# ==========================
# 1. PDR
# ==========================
plt.figure(figsize=(8,5))

for tech in df["technique"].unique():
    subset = df[df["technique"] == tech]
    plt.plot(subset["L"],
             subset["PDR"],
             marker='o',
             label=tech)

plt.xlabel("Número de Antenas (L)")
plt.ylabel("PDR")
plt.title("PDR vs Número de Antenas")
plt.grid(True)
plt.legend()

plt.savefig(output_dir / "pdr_vs_antennas.png")
plt.close()

# ==========================
# 2. PER
# ==========================
plt.figure(figsize=(8,5))

for tech in df["technique"].unique():
    subset = df[df["technique"] == tech]
    plt.plot(subset["L"],
             subset["PER"],
             marker='o',
             label=tech)

plt.xlabel("Número de Antenas (L)")
plt.ylabel("PER")
plt.title("PER vs Número de Antenas")
plt.grid(True)
plt.legend()

plt.savefig(output_dir / "per_vs_antennas.png")
plt.close()

# ==========================
# 3. Tempo de execução
# ==========================
plt.figure(figsize=(8,5))

for tech in df["technique"].unique():
    subset = df[df["technique"] == tech]
    plt.plot(subset["L"],
             subset["elapsed_s"],
             marker='o',
             label=tech)

plt.xlabel("Número de Antenas (L)")
plt.ylabel("Tempo (s)")
plt.title("Tempo de Simulação")
plt.grid(True)
plt.legend()

plt.savefig(output_dir / "tempo_vs_antennas.png")
plt.close()

# ==========================
# 4. Ganho teórico
# ==========================
plt.figure(figsize=(8,5))

for tech in df["technique"].unique():
    subset = df[df["technique"] == tech]
    plt.plot(subset["L"],
             subset["gain_python_dB"],
             marker='o',
             label=tech)

plt.xlabel("Número de Antenas (L)")
plt.ylabel("Ganho (dB)")
plt.title("Ganho Teórico Python")
plt.grid(True)
plt.legend()

plt.savefig(output_dir / "gain_vs_antennas.png")
plt.close()

# ==========================
# 5. Taxas de perda
# ==========================
metrics = [
    "interferenceLossRate",
    "sensitivityLossRate",
    "gatewayBusyRate",
    "txLossRate"
]

for metric in metrics:
    plt.figure(figsize=(8,5))

    for tech in df["technique"].unique():
        subset = df[df["technique"] == tech]
        plt.plot(subset["L"],
                 subset[metric],
                 marker='o',
                 label=tech)

    plt.xlabel("Número de Antenas (L)")
    plt.ylabel(metric)
    plt.title(f"{metric} vs Número de Antenas")
    plt.grid(True)
    plt.legend()

    plt.savefig(output_dir / f"{metric}.png")
    plt.close()

print("\n✅ Gráficos salvos em: plots_result_7_metade")
