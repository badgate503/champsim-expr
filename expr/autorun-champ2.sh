echo "任务 @ champ2: 跑 Baseline 的所有 Trace，计算 IPC Speedup; 跑 Prophet 的所有 Trace，计算 IPC Speedup; 计算 Miss Class"
echo "================================================================"

# ./expr.py -m missclass -p baseline -l ligra spec17 ml google gap spec06


echo "next up: prophet" 
sleep 3
./expr.py -m missclass -p prophet -l ligra spec17 ml google gap spec06 -a