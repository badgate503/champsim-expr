echo "任务 @ champ2"
echo "================================================================"

# ./expr.py -p mjtp -m ipc -j inf-lyq -l gap google ligra ml spec06 spec17
# ./expr.py -p mjtp -m ipc -j inf-tri-tar -l gap google ligra ml spec06 spec17 -a
# ./expr.py -p mjtp -m ipc -j inf-tri -l gap google ligra ml spec06 spec17 -a
# ./expr.py -p mjtp -m ipc -j inf-tar -l gap google ligra ml spec06 spec17 -a

# ./runner.py -p mjset.inf-tri mjset.inf-tar mjset.inf-tt mjset.inf-yq mjtp.inf-tri mjtp.inf-tar mjtp.inf-tt mjtp.inf-yq -l gap google ligra ml spec06 spec17
# ./runner.py -p stride.no stride.baseline -l gap google ligra ml spec06 spec17
# ./runner.py -p nostore notouch storetouch -l gap google ligra ml spec06 spec17
# ./runner.py -p ctp2.0.mc -l gap google ligra ml spec06 spec17
# ./runner.py -p look1d1 look1d2 look1d4 look1d6 look1d8 degree2 degree4 degree6 degree8 -l ml
# ./runner.py -p look1d1 look1d2 look1d4 look1d6 look1d8 degree2 degree4 degree6 degree8 look2d1 look3d1 look4d1 -l google
# ./runner.py -p look2d2 look2d4 look2d6 look2d8 look3d2 look3d4 look3d6 look3d8 look4d2 look4d4 look4d6 look4d8 mjyq -l google
# ./runner.py -p srtp.tt srtp.tri retp ptp.l1 ptp.l2 -l gap google ligra ml spec06 spec17
# ./runner.py -p ptp.m1 ptp.m2 ptp.m4 ptp.m8 ptp.m12 ptp.m16 ptp.a1 ptp.a2 ptp.a4 ptp.a8 ptp.a12 ptp.a16 -l google
# ./runner.py -p ptpa8.lru4k ptpa8.lru8k ptpa8.lru16k ptpa8.lru32k ptpa8.lru64k ptpa8.lru128k ptpa8.srp4k ptpa8.srp8k ptpa8.srp16k ptpa8.srp32k ptpa8.srp64k ptpa8.srp128k -l google
# ./runner.py -p ptpa8.srp1way ptpa8.srp2way ptpa8.srp3way ptpa8.srp4way -l google

./compiler.py -p ftp -m ipc -f HWND=32 HIT_THRESHOLD=0.9 -e champsim.ftp.inf.9p
./compiler.py -p ftp -m ipc -f HWND=32 HIT_THRESHOLD=0.8 -e champsim.ftp.inf.8p
./compiler.py -p ftp -m ipc -f HWND=32 HIT_THRESHOLD=0.7 -e champsim.ftp.inf.7p
./compiler.py -p ftp -m ipc -f HWND=32 HIT_THRESHOLD=0.6 -e champsim.ftp.inf.6p
./compiler.py -p ftp -m ipc -f HWND=1 HIT_THRESHOLD=0.9 -e champsim.ftp.look1

# ./runner.py -p ftp.inf.9p ftp.inf.8p ftp.inf.7p ftp.inf.6p ftp.look1 -l gap google ligra ml spec06 spec17
