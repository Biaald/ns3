import numpy as np
import math
import matplotlib.pyplot as plt
import os

# -------------------------------------------------------------
# Funções auxiliares
def convert2nsymbols(number, base, n):
    remainder = []
    dividend = number
    quotient = dividend // base
    remainder.append(dividend % base)
    for _ in range(n - 1):
        dividend = quotient
        quotient = dividend // base
        remainder.append(dividend % base)
    return np.flip(remainder)

# -------------------------------------------------------------
# Simulação LoRa com diferentes técnicas de combinação
def simulate_lora_diversity(SNR_dB, SF, N_chips, base_down_chirp,
                            num_rx, test_points, combining="mrc"):

    gamma = 10 ** (SNR_dB / 10)
    Ampl = np.sqrt(gamma * Pnoise)

    errors = 0
    k = np.arange(N_chips)

    for _ in range(test_points):
        symbol_index = np.random.randint(0, N_chips)

        # Geração do símbolo LoRa
        lora_symbol = np.exp(
            1j * 2 * np.pi * np.mod(symbol_index + k, N_chips) * (k / N_chips)
        )
        lora_symbol /= np.sqrt(np.mean(np.abs(lora_symbol) ** 2))

        rs = []
        hs = []

        for _rx in range(num_rx):
            # Canal Rayleigh
            h = (1 / np.sqrt(2)) * (
                np.random.normal(0, 1) + 1j * np.random.normal(0, 1)
            )

            # Ruído AWGN
            n = np.sqrt(Pnoise / 2) * (
                np.random.randn(N_chips) + 1j * np.random.randn(N_chips)
            )

            r = h * Ampl * lora_symbol + n
            rs.append(r)
            hs.append(h)

        rs = np.array(rs)
        hs = np.array(hs)

        # ------------------ Combinação ------------------
        if combining == "mrc":
            combined_signal = np.dot(np.conjugate(hs), rs) / np.sum(np.abs(hs) ** 2)

        elif combining == "egc":
            phase = hs / np.abs(hs)
            combined_signal = np.sum(np.conjugate(phase)[:, None] * rs, axis=0) / num_rx

        elif combining == "sc":
            idx = np.argmax(np.abs(hs))
            combined_signal = rs[idx]

        else:
            raise ValueError("Combining must be 'sc', 'egc' or 'mrc'")

        # Demodulação
        combined_freq = np.abs(
            np.fft.fft(combined_signal * base_down_chirp, norm="ortho")
        )
        dv = np.argmax(combined_freq)

        bits_tx = convert2nsymbols(symbol_index, 2, SF)
        bits_rx = convert2nsymbols(dv, 2, SF)
        errors += np.sum(bits_tx != bits_rx)

    return errors / (SF * test_points)

# -------------------------------------------------------------
# Parâmetros
SF = 10
BW = 125000
Pnoise = 10 ** ((10 * math.log10(BW) - 168) / 10)

N_chips = 2 ** SF
k = np.arange(N_chips)
base_down_chirp = np.exp(-1j * 2 * np.pi * k * (k / N_chips))

SNR_dBs = np.arange(-30, 12, 2)
test_points = 200000
Ls = [1, 2, 3, 4, 5]

# -------------------------------------------------------------
# Estilos visuais
combining_colors = {
    "sc": "green",
    "egc": "blue",
    "mrc": "red"
}

line_styles = {
    1: "-",      # SISO
    2: "--",
    3: ":",
    4: "-o",
    5: "-x"
}

# -------------------------------------------------------------
# Simulação
results = {c: {L: [] for L in Ls} for c in ["sc", "egc", "mrc"]}

for combining in results:
    print(f"\n=== Técnica: {combining.upper()} ===")
    for L in Ls:
        for snr in SNR_dBs:
            ber = simulate_lora_diversity(
                snr, SF, N_chips, base_down_chirp,
                L, test_points, combining=combining
            )
            results[combining][L].append(ber)
            print(f"{combining.upper()} | L={L} | SNR={snr:>5.1f} dB → BER={ber:.4e}")

# -------------------------------------------------------------
# Plot
plt.figure(figsize=(10, 5))

for combining, color in combining_colors.items():
    for L in Ls:
        plt.semilogy(
            SNR_dBs,
            results[combining][L],
            line_styles[L],
            color=color,
            linewidth=2,
            label=f"{combining.upper()}, L={L}"
        )

plt.grid(True, which="both", linestyle="--", linewidth=0.6)
plt.xlabel("SNR (dB)")
plt.ylabel("BER")
plt.title(f"BER × SNR (SF={SF}) — Rayleigh + AWGN\nSC vs EGC vs MRC (SISO → SIMO 1×5)")
plt.ylim(1e-5, 1)
plt.xlim(SNR_dBs[0], SNR_dBs[-1])
plt.legend(ncol=3, fontsize=9)
plt.tight_layout()

plt.savefig("ber_sc_egc_mrc_simo.png", dpi=300, bbox_inches="tight")
plt.close()

print(f"\n✅ Gráfico salvo em: ber_sc_egc_mrc_simo.png")
