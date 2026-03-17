# ./compiler.py -p latetp -m ipc -f DEFAULT_LOOKAHEAD=0 DEFAULT_DEGREE=2 -e ltpl0d2
# ./compiler.py -p latetp -m ipc -f DEFAULT_LOOKAHEAD=0 DEFAULT_DEGREE=4 -e ltpl0d4
# # ./compiler.py -p latetp -m ipc -f DEFAULT_LOOKAHEAD=0 DEFAULT_DEGREE=5 -e ltpl0d5
# ./compiler.py -p latetp -m ipc -f DEFAULT_LOOKAHEAD=0 DEFAULT_DEGREE=6 -e ltpl0d6
# ./compiler.py -p latetp -m ipc -f DEFAULT_LOOKAHEAD=0 DEFAULT_DEGREE=8 -e ltpl0d8
# ./compiler.py -p latetp -m ipc -f DEFAULT_LOOKAHEAD=1 DEFAULT_DEGREE=1 -e ltpl1d1
# ./compiler.py -p latetp -m ipc -f DEFAULT_LOOKAHEAD=1 DEFAULT_DEGREE=2 -e ltpl1d2
# # # ./compiler.py -p latetp -m ipc -f DEFAULT_LOOKAHEAD=1 DEFAULT_DEGREE=3 -e ltpl1d3
# ./compiler.py -p latetp -m ipc -f DEFAULT_LOOKAHEAD=1 DEFAULT_DEGREE=4 -e ltpl1d4
# # ./compiler.py -p latetp -m ipc -f DEFAULT_LOOKAHEAD=1 DEFAULT_DEGREE=5 -e ltpl1d5
# ./compiler.py -p latetp -m ipc -f DEFAULT_LOOKAHEAD=1 DEFAULT_DEGREE=6 -e ltpl1d6
# ./compiler.py -p latetp -m ipc -f DEFAULT_LOOKAHEAD=1 DEFAULT_DEGREE=8 -e ltpl1d8
# ./compiler.py -p latetp -m ipc -f DEFAULT_LOOKAHEAD=2 DEFAULT_DEGREE=1 -e ltpl2d1
# ./compiler.py -p latetp -m ipc -f DEFAULT_LOOKAHEAD=2 DEFAULT_DEGREE=2 -e ltpl2d2
# # # ./compiler.py -p latetp -m ipc -f DEFAULT_LOOKAHEAD=2 DEFAULT_DEGREE=3 -e ltpl2d3
# ./compiler.py -p latetp -m ipc -f DEFAULT_LOOKAHEAD=2 DEFAULT_DEGREE=4 -e ltpl2d4
# # ./compiler.py -p latetp -m ipc -f DEFAULT_LOOKAHEAD=2 DEFAULT_DEGREE=5 -e ltpl2d5
# ./compiler.py -p latetp -m ipc -f DEFAULT_LOOKAHEAD=2 DEFAULT_DEGREE=6 -e ltpl2d6
# ./compiler.py -p latetp -m ipc -f DEFAULT_LOOKAHEAD=2 DEFAULT_DEGREE=8 -e ltpl2d8
# ./compiler.py -p latetp -m ipc -f DEFAULT_LOOKAHEAD=3 DEFAULT_DEGREE=1 -e ltpl3d1
# ./compiler.py -p latetp -m ipc -f DEFAULT_LOOKAHEAD=3 DEFAULT_DEGREE=2 -e ltpl3d2
# # # ./compiler.py -p latetp -m ipc -f DEFAULT_LOOKAHEAD=3 DEFAULT_DEGREE=3 -e ltpl3d3
# ./compiler.py -p latetp -m ipc -f DEFAULT_LOOKAHEAD=3 DEFAULT_DEGREE=4 -e ltpl3d4
# # ./compiler.py -p latetp -m ipc -f DEFAULT_LOOKAHEAD=3 DEFAULT_DEGREE=5 -e ltpl3d5
# ./compiler.py -p latetp -m ipc -f DEFAULT_LOOKAHEAD=3 DEFAULT_DEGREE=6 -e ltpl3d6
# ./compiler.py -p latetp -m ipc -f DEFAULT_LOOKAHEAD=3 DEFAULT_DEGREE=8 -e ltpl3d8
# ./compiler.py -p latetp -m ipc -f DEFAULT_LOOKAHEAD=4 DEFAULT_DEGREE=1 -e ltpl4d1
# ./compiler.py -p latetp -m ipc -f DEFAULT_LOOKAHEAD=4 DEFAULT_DEGREE=2 -e ltpl4d2
# # # ./compiler.py -p latetp -m ipc -f DEFAULT_LOOKAHEAD=4 DEFAULT_DEGREE=3 -e ltpl4d3
# ./compiler.py -p latetp -m ipc -f DEFAULT_LOOKAHEAD=4 DEFAULT_DEGREE=4 -e ltpl4d4
# # ./compiler.py -p latetp -m ipc -f DEFAULT_LOOKAHEAD=4 DEFAULT_DEGREE=5 -e ltpl4d5
# ./compiler.py -p latetp -m ipc -f DEFAULT_LOOKAHEAD=4 DEFAULT_DEGREE=6 -e ltpl4d6
# ./compiler.py -p latetp -m ipc -f DEFAULT_LOOKAHEAD=4 DEFAULT_DEGREE=8 -e ltpl4d8
# # ./submitter.py -p ltpl0d3 ltpl0d4 ltpl1d1 ltpl1d2 ltpl1d3 ltpl1d4 ltpl2d1 ltpl2d2 ltpl2d3 ltpl2d4 ltpl3d1 ltpl3d2 ltpl3d3 ltpl3d4 ltpl4d1 ltpl4d2 ltpl4d3 ltpl4d4 -l gap google ligra ml spec06 spec17
# ./submitter.py -p ltpl0d5 ltpl1d5 ltpl2d5 ltpl3d5 ltpl4d5 -l gap google ligra ml spec06 spec17
# ./submitter.py -p ltpl0d6 ltpl0d8 ltpl1d6 ltpl1d8 ltpl2d6 ltpl2d8 ltpl3d6 ltpl3d8 ltpl4d6 ltpl4d8 -l gap google ligra ml spec06 spec17
./submitter.py -p prismlog ltpl0d2 ltpl0d4 ltpl0d6 ltpl0d8 ltpl1d1 ltpl1d2 ltpl1d4 ltpl1d6 ltpl1d8 ltpl2d1 ltpl2d2 ltpl2d4 ltpl2d6 ltpl2d8 ltpl3d1 ltpl3d2 ltpl3d4 ltpl3d6 ltpl3d8 ltpl4d1 ltpl4d2 ltpl4d4 ltpl4d6 ltpl4d8 -l gap google ligra ml spec06 spec17

# ./compiler.py -p pctp -m ipc -f PCQ_SIZE=1 -e pctp.a1
# ./compiler.py -p pctp -m ipc -f PCQ_SIZE=2 -e pctp.a2
# ./compiler.py -p pctp -m ipc -f PCQ_SIZE=4 -e pctp.a4
# ./compiler.py -p pctp -m ipc -f PCQ_SIZE=8 -e pctp.a8
# ./compiler.py -p pctp -m ipc -f PCQ_SIZE=12 -e pctp.a12
# ./compiler.py -p pctp -m ipc -f PCQ_SIZE=16 -e pctp.a16
# ./submitter.py -p pctp.a1 pctp.a2 pctp.a4 pctp.a8 pctp.a12 pctp.a16 -l google

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
