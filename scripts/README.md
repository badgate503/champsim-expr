# Experiment Scripts

This directory contains scripts for reproducing the experiments presented in the **PRISM** paper.

## Reproducing Paper Results

We provide a top-level script `prism-expr.py` to automatically reproduce the figures reported in the evaluation section of the paper.

Please refer to the top-level repository README for detailed usage instructions of `prism-expr.py`.

## Evaluating Other Prefetchers

The provided scripts `compiler.py`, `runner.py`, `get_result.py` support evaluating other prefetchers and customizing the experimental workflow.  
The evaluation pipeline consists of three stages: **compilation**, **simulation**, and **result extraction**.
Each script provides a `-h` option to display the complete list of available arguments.

---

### 1. Compile Prefetchers

Compile a ChampSim executable with a specific prefetcher:

```bash
python3 compiler.py -p <prefetcher> 
```

For all available options:

```bash
python3 compiler.py -h
```

Key arguments:

| Argument           | Description                                                            |
| ------------------ | ---------------------------------------------------------------------- |
| `-p, --prefetcher` | Specify the prefetcher implementation in `PRISM/prefetcher` (required) |
| `-c, --config`     | Specify the ChampSim configuration file (e.g., `config_dram2400`)      |
| `-e, --exename`    | Specify the generated executable name (default: prefetcher name)       |
| `-f, --flag`       | Specify additional compilation flags (,i.e., -D[flag])                 |

Example: 

```bash
python3 compiler.py -p prism -c config_dram2400 -e dram2400.prism
```

### 2. Run Simulations

Run simulations with selected prefetchers and trace sets:

```bash
python3 runner.py -p <prefetcher> -l <trace_set>
```

For all available options:

```bash
python3 runner.py -h
```

Key arguments:

| Argument           | Description                                                                 |
| ------------------ | --------------------------------------------------------------------------- |
| `-p, --prefetcher` | Specify prefetcher(s) to evaluate (e.g., `baseline triangel prophet prism`) |
| `-t, --traces`     | Specify individual trace names to run                                       |
| `-l, --tracelist`  | Specify predefined trace suites (e.g., `ligra gap spec17 ml google`)        |
| `-o, --output`     | Specify the directory for simulation results                                |

Example:

```bash
python3 runner.py -p dram2400.prism -l ligra gap spec17 ml google
```

### 3. Extract Results

Extract and aggregate simulation results:

```bash
python3 get_result.py -p <prefetcher_list>
```

For all available options:

```bash
python3 get_result.py -h
```

Key arguments:

| Argument        | Description                                         |
| --------------- | --------------------------------------------------- |
| `-p, --pflist`  | Specify the list of prefetchers to analyze          |
| `-a, --alias`   | Specify aliases for prefetchers in output files     |
| `-m, --measure` | Specify metrics to extract                          |
| `-o, --output`  | Specify the output directory for aggregated results |

Example:

```bash
python3 get_result.py -p dram2400.prism -m IPC IPCI
```
