echo "任务 @ champ1: Triangel 跑所有 Trace，计算 IPC Speedup"
echo "================================================================"

./expr.py -m ipc -p triangel -l ligra spec17 ml google gap spec06

echo "Triangel Finished"

./analyze.py -p triangel

echo "Champsim Task Finished"