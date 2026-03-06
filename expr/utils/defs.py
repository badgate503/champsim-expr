
import os

LYQ_PATH = "/mnt/data/lyq/"
TRACE_PATH = os.path.join(LYQ_PATH, "champtraces")
CHAMPSIM_PATH = os.path.join(LYQ_PATH, "Kairos")
EXPR_PATH = os.path.join(CHAMPSIM_PATH, "expr")
LOG_PATH = os.path.join(LYQ_PATH, "exprlog")
RESULT_PATH = os.path.join(EXPR_PATH, "result")
HINT_PATH = os.path.join(EXPR_PATH, "hint")
RED = '\033[91m'
GREEN = '\033[92m'
YELLOW = '\033[93m'
BLUE = '\033[94m'
MAGENTA = '\033[95m'
CYAN = '\033[96m'
WHITE = '\033[97m'
BOLD = '\033[1m'
UNDERLINE = '\033[4m'
END = '\033[0m'
