./ns3 run "scratch/lora-simo/lora-network-test \
    --nDevices=600 \
    --radius=2500 \
    --simTime=3600 \
    --appPeriod=50 \
    --technique=ALL \
    --nAntennas=0 \
    --realisticChannel=true"

./ns3 run "scratch/lora-simo/lora-network-test \
    --nDevices=600 \
    --radius=5000 \
    --simTime=3600 \
    --appPeriod=50 \
    --technique=ALL \
    --nAntennas=0 \
    --realisticChannel=true"
