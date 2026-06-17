./compiler.py -p prism -m ipc -e prism

./compiler.py -p prism -m ipc -c config_cache1_2 -f N_LLC_SET=2048 -e cache1_2.prism
./compiler.py -p prism -m ipc -c config_cache1_4 -f N_LLC_SET=4096 -e cache1_4.prism

./compiler.py -p prism -m ipc -c config_dram1200 -e dram1200.prism
./compiler.py -p prism -m ipc -c config_dram2400 -e dram2400.prism
./compiler.py -p prism -m ipc -c config_dram3600 -e dram3600.prism
./compiler.py -p prism -m ipc -c config_dram6000 -e dram6000.prism

./compiler.py -p prism -m ipc -c config_ipcp -e l1ipcp.prism
./compiler.py -p prism -m ipc -c config_berti -e l1berti.prism

./submitter.py -p prism \
 cache1_2.prism cache1_4.prism\
 dram1200.prism dram2400.prism dram3600.prism dram6000.prism\
 l1ipcp.prism l1berti.prism\
 -l gap ligra ml spec17 google

 ./submitter.py -p prism-none prism-ol-pctp prism-ol-tgp prism-ol-bmp prism-ol-pol\
  prism-wo-pctp prism-wo-tgp prism-wo-bmp prism-wo-pol\
  -l gap ligra ml spec17 google