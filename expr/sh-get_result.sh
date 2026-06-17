########################################## 基础指标 IPCI，Accuracy, Timeliness, DRAM_Traffic
python3 get_result.py -p triangel prophet prism -o ./result/plot_data/basic -m IPCI L2C_Accuracy L2C_Timeliness DRAM_Traffic

########################################## Replacement 方案比较
python3 get_result.py -p baseline.4way lfutp shtp srtp drtp rndtp -a LRU LFU SHiP SRRiP DRRiP Random -o ./result/plot_data/rep -m IPCI MT_acc_find_rate

########################################## Resize 方案比较
python3 get_result.py -p no baseline.1way baseline.2way baseline.3way baseline.4way baseline.5way baseline.6way baseline.7way baseline.8way resize_tr resize_pr resize_kr resize_prism -o ./result/plot_data/resize -m IPCI

########################################## 敏感性实验
## 1. Cache 敏感性
python3 get_result.py -p cache0.5_2.baseline cache1_2.baseline cache1_4.baseline baseline.4way \
                         cache0.5_2.triangel cache1_2.triangel cache1_4.triangel triangel      \
                         cache0.5_2.prophet cache1_2.prophet  cache1_4.prophet  prophet       \
                         cache0.5_2.prism cache1_2.prism    cache1_4.prism    prism         \
                      -a cache0.5_2.baseline cache1_2.baseline cache1_4.baseline cache2_4.baseline \
                         cache0.5_2.triangel cache1_2.triangel cache1_4.triangel cache2_4.triangel \
                         cache0.5_2.prophet cache1_2.prophet  cache1_4.prophet  cache2_4.prophet  \
                         cache0.5_2.prism cache1_2.prism    cache1_4.prism    cache2_4.prism    -o ./result/plot_data/cache -m IPCI
## 2. DRAM 带宽敏感性
python3 get_result.py -p dram1200.baseline dram1200.triangel dram1200.prophet dram1200.prism \
                         dram2400.baseline dram2400.triangel dram2400.prophet dram2400.prism \
                         dram3600.baseline dram3600.triangel dram3600.prophet dram3600.prism \
                         baseline.4way     triangel          prophet          prism          \
                         dram6000.baseline dram6000.triangel dram6000.prophet dram6000.prism \
                      -a 1200.baseline 1200.triangel 1200.prophet 1200.prism \
                         2400.baseline 2400.triangel 2400.prophet 2400.prism \
                         3600.baseline 3600.triangel 3600.prophet 3600.prism \
                         4800.baseline 4800.triangel 4800.prophet 4800.prism \
                         6000.baseline 6000.triangel 6000.prophet 6000.prism -o ./result/plot_data/dram -m IPCI
## 3. L1 空间预取器敏感性
python3 get_result.py -p l1ipcp.baseline   l1ipcp.triangel   l1ipcp.prophet   l1ipcp.prism  \
                         l1berti.baseline  l1berti.triangel  l1berti.prophet  l1berti.prism \
                         baseline.4way     triangel          prophet          prism   \
                      -a IPCP.baseline   IPCP.triangel   IPCP.prophet   IPCP.prism  \
                         Berti.baseline  Berti.triangel  Berti.prophet  Berti.prism \
                         Stride.baseline Stride.triangel Stride.prophet Stride.prism  -o ./result/plot_data/l1spatial -m IPCI

######################################## Metadata 相关实验
python3 get_result.py -p baseline triangel prophet prism -a Baseline Triangel Prophet Prism -o ./result/plot_data/mdtraffic -m MT_lookups MT_inserts MT_hits L2C_USEFUL 

######################################## 消融实验
python3 get_result.py -p baseline.4way prism-ol-pctp prism-ol-tgp prism-ol-bmp prism-ol-pol prism prism-wo-pctp prism-wo-tgp prism-wo-bmp prism-wo-pol \
                      -a prism-none    prism-ol-pctp prism-ol-tgp prism-ol-bmp prism-ol-pol prism prism-wo-pctp prism-wo-tgp prism-wo-bmp prism-wo-pol -o ./result/plot_data/ablation -m IPCI

######################################## PATSIZE
python3 get_result.py -p pctpinf.a8 pctp12k pctp24k pctp48k pctp96k pctp144k pctp192k -o ./result/plot_data/pctp -m IPCI

python3 get_result.py -p inftable16 prism optimal -o ./result/optimal -m IPCI