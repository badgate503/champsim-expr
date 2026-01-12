echo "任务 @ champ4: 跑 PTP 的所有 Trace，计算 IPC Speedup;"
echo "================================================================"

# ./expr.py -m missclass -p baseline -l ligra spec17 ml google gap spec06
./expr.py -m ipc -p ptp -l ligra spec17 ml google gap spec06

