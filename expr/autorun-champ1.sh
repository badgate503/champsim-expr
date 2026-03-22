./compiler.py -p latetp -m ipc -f DEFAULT_LOOKAHEAD=0 DEFAULT_DEGREE=2 -e ltp4wayl0d2
./compiler.py -p latetp -m ipc -f DEFAULT_LOOKAHEAD=0 DEFAULT_DEGREE=4 -e ltp4wayl0d4
# ./compiler.py -p latetp -m ipc -f DEFAULT_LOOKAHEAD=0 DEFAULT_DEGREE=5 -e ltp4wayl0d5
./compiler.py -p latetp -m ipc -f DEFAULT_LOOKAHEAD=0 DEFAULT_DEGREE=6 -e ltp4wayl0d6
./compiler.py -p latetp -m ipc -f DEFAULT_LOOKAHEAD=0 DEFAULT_DEGREE=8 -e ltp4wayl0d8
./compiler.py -p latetp -m ipc -f DEFAULT_LOOKAHEAD=1 DEFAULT_DEGREE=1 -e ltp4wayl1d1
./compiler.py -p latetp -m ipc -f DEFAULT_LOOKAHEAD=1 DEFAULT_DEGREE=2 -e ltp4wayl1d2
# # ./compiler.py -p latetp -m ipc -f DEFAULT_LOOKAHEAD=1 DEFAULT_DEGREE=3 -e ltp4wayl1d3
./compiler.py -p latetp -m ipc -f DEFAULT_LOOKAHEAD=1 DEFAULT_DEGREE=4 -e ltp4wayl1d4
# ./compiler.py -p latetp -m ipc -f DEFAULT_LOOKAHEAD=1 DEFAULT_DEGREE=5 -e ltp4wayl1d5
./compiler.py -p latetp -m ipc -f DEFAULT_LOOKAHEAD=1 DEFAULT_DEGREE=6 -e ltp4wayl1d6
./compiler.py -p latetp -m ipc -f DEFAULT_LOOKAHEAD=1 DEFAULT_DEGREE=8 -e ltp4wayl1d8
./compiler.py -p latetp -m ipc -f DEFAULT_LOOKAHEAD=2 DEFAULT_DEGREE=1 -e ltp4wayl2d1
./compiler.py -p latetp -m ipc -f DEFAULT_LOOKAHEAD=2 DEFAULT_DEGREE=2 -e ltp4wayl2d2
# # ./compiler.py -p latetp -m ipc -f DEFAULT_LOOKAHEAD=2 DEFAULT_DEGREE=3 -e ltp4wayl2d3
./compiler.py -p latetp -m ipc -f DEFAULT_LOOKAHEAD=2 DEFAULT_DEGREE=4 -e ltp4wayl2d4
# ./compiler.py -p latetp -m ipc -f DEFAULT_LOOKAHEAD=2 DEFAULT_DEGREE=5 -e ltp4wayl2d5
./compiler.py -p latetp -m ipc -f DEFAULT_LOOKAHEAD=2 DEFAULT_DEGREE=6 -e ltp4wayl2d6
./compiler.py -p latetp -m ipc -f DEFAULT_LOOKAHEAD=2 DEFAULT_DEGREE=8 -e ltp4wayl2d8
./compiler.py -p latetp -m ipc -f DEFAULT_LOOKAHEAD=3 DEFAULT_DEGREE=1 -e ltp4wayl3d1
./compiler.py -p latetp -m ipc -f DEFAULT_LOOKAHEAD=3 DEFAULT_DEGREE=2 -e ltp4wayl3d2
# # ./compiler.py -p latetp -m ipc -f DEFAULT_LOOKAHEAD=3 DEFAULT_DEGREE=3 -e ltp4wayl3d3
./compiler.py -p latetp -m ipc -f DEFAULT_LOOKAHEAD=3 DEFAULT_DEGREE=4 -e ltp4wayl3d4
# ./compiler.py -p latetp -m ipc -f DEFAULT_LOOKAHEAD=3 DEFAULT_DEGREE=5 -e ltp4wayl3d5
./compiler.py -p latetp -m ipc -f DEFAULT_LOOKAHEAD=3 DEFAULT_DEGREE=6 -e ltp4wayl3d6
./compiler.py -p latetp -m ipc -f DEFAULT_LOOKAHEAD=3 DEFAULT_DEGREE=8 -e ltp4wayl3d8
./compiler.py -p latetp -m ipc -f DEFAULT_LOOKAHEAD=4 DEFAULT_DEGREE=1 -e ltp4wayl4d1
./compiler.py -p latetp -m ipc -f DEFAULT_LOOKAHEAD=4 DEFAULT_DEGREE=2 -e ltp4wayl4d2
# # ./compiler.py -p latetp -m ipc -f DEFAULT_LOOKAHEAD=4 DEFAULT_DEGREE=3 -e ltp4wayl4d3
./compiler.py -p latetp -m ipc -f DEFAULT_LOOKAHEAD=4 DEFAULT_DEGREE=4 -e ltp4wayl4d4
# ./compiler.py -p latetp -m ipc -f DEFAULT_LOOKAHEAD=4 DEFAULT_DEGREE=5 -e ltp4wayl4d5
./compiler.py -p latetp -m ipc -f DEFAULT_LOOKAHEAD=4 DEFAULT_DEGREE=6 -e ltp4wayl4d6
./compiler.py -p latetp -m ipc -f DEFAULT_LOOKAHEAD=4 DEFAULT_DEGREE=8 -e ltp4wayl4d8
# # ./submitter.py -p ltp4wayl0d3 ltp4wayl0d4 ltp4wayl1d1 ltp4wayl1d2 ltp4wayl1d3 ltp4wayl1d4 ltp4wayl2d1 ltp4wayl2d2 ltp4wayl2d3 ltp4wayl2d4 ltp4wayl3d1 ltp4wayl3d2 ltp4wayl3d3 ltp4wayl3d4 ltp4wayl4d1 ltp4wayl4d2 ltp4wayl4d3 ltp4wayl4d4 -l gap google ligra ml spec06 spec17
# ./submitter.py -p ltp4wayl0d5 ltp4wayl1d5 ltp4wayl2d5 ltp4wayl3d5 ltp4wayl4d5 -l gap google ligra ml spec06 spec17
# ./submitter.py -p ltp4wayl0d6 ltp4wayl0d8 ltp4wayl1d6 ltp4wayl1d8 ltp4wayl2d6 ltp4wayl2d8 ltp4wayl3d6 ltp4wayl3d8 ltp4wayl4d6 ltp4wayl4d8 -l gap google ligra ml spec06 spec17
./submitter.py -p ltp4wayl0d2 ltp4wayl0d4 ltp4wayl0d6 ltp4wayl0d8 ltp4wayl1d1 ltp4wayl1d2 ltp4wayl1d4 ltp4wayl1d6 ltp4wayl1d8 ltp4wayl2d1 ltp4wayl2d2 ltp4wayl2d4 ltp4wayl2d6 ltp4wayl2d8 ltp4wayl3d1 ltp4wayl3d2 ltp4wayl3d4 ltp4wayl3d6 ltp4wayl3d8 ltp4wayl4d1 ltp4wayl4d2 ltp4wayl4d4 ltp4wayl4d6 ltp4wayl4d8 -l gap google ligra ml spec17

# ./compiler.py -p pctp -m ipc -f PCQ_SIZE=1 -e pctpinf.a1
# ./compiler.py -p pctp -m ipc -f PCQ_SIZE=2 -e pctpinf.a2
# ./compiler.py -p pctp -m ipc -f PCQ_SIZE=4 -e pctpinf.a4
# ./compiler.py -p pctp -m ipc -f PCQ_SIZE=8 -e pctpinf.a8
# ./compiler.py -p pctp -m ipc -f PCQ_SIZE=12 -e pctpinf.a12
# ./compiler.py -p pctp -m ipc -f PCQ_SIZE=16 -e pctpinf.a16
# ./submitter.py -p pctpinf.a1 pctpinf.a2 pctpinf.a4 pctpinf.a8 pctpinf.a12 pctpinf.a16 -l google

# ./compiler.py -p pctp -m ipc -f PC_META_TABLE_SIZE=6144 -e pctp6ksrp
# ./compiler.py -p pctp -m ipc -f PC_META_TABLE_SIZE=12288 -e pctp12ksrp
# ./compiler.py -p pctp -m ipc -f PC_META_TABLE_SIZE=24576 -e pctp24ksrp
# ./submitter.py -p pctp6ksrp pctp12ksrp pctp24ksrp -l google


# ./compiler.py -p conftp -m ipc -f INF_CONFLICT_TABLE -e conftp-inf
# ./compiler.py -p conftp -m ipc -f CONFLICT_TABLE_SIZE=49152 -e conftp1way
# ./compiler.py -p conftp -m ipc -f CONFLICT_TABLE_SIZE=98304 -e conftp2way
# ./compiler.py -p conftp -m ipc -f CONFLICT_TABLE_SIZE=147456 -e conftp3way
# ./compiler.py -p conftp -m ipc -f CONFLICT_TABLE_SIZE=196608 -e conftp4way

# ./compiler.py -p resize -m ipc -f INIT_WINDOW=10000 -e resize1w
# ./compiler.py -p resize -m ipc -f INIT_WINDOW=100000 -e resize10w
# ./compiler.py -p resize -m ipc -f INIT_WINDOW=1000000 -e resize1m
# ./compiler.py -p resize -m ipc -f INIT_WINDOW=10000000 -e resize10m
