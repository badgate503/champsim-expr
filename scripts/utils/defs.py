
import os
from pathlib import Path

SCRIPTS_PATH = Path(__file__).resolve().parent.parent
LYQ_PATH = SCRIPTS_PATH.parent.parent

CHAMPSIM_PATH = SCRIPTS_PATH.parent
TRACE_PATH = LYQ_PATH / "trace"

EXPR_PATH = CHAMPSIM_PATH / "experiments"
# LOG_PATH = EXPR_PATH / "champsim_log"
LOG_PATH = LYQ_PATH / "PRISM_champsim_log"
RESULT_PATH = EXPR_PATH / "results"
FIGURE_PATH = EXPR_PATH / "figure_out"

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
