# LoRa SIMO Diversity for ns-3

## Técnicas implementadas

- SC (Selection Combining)
- EGC (Equal Gain Combining)
- MRC (Maximal Ratio Combining)

## Dependências

- ns-3.43
- module lorawan

## Compilar

```bash
./ns3 configure
./ns3 build
```

## Executar

```bash
./ns3 run scratch/lora-simo/lora-network-test
```

## Gerar gráficos

```bash
python3 analysis/plot_snir.py

```
