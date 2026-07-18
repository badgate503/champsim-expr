<p align="center">
  <img src="logo.png" alt="PRISM Logo" width="400">
</p>

<h2 align="center">
  PRISM: A Miss-Attribution-Guided Temporal Prefetcher Beyond Metadata Management
</h2>

<!-- <p align="center">
    <a href="https://github.com/CMU-SAFARI/Athena/blob/master/LICENSE"><img alt="License: MIT" src="https://img.shields.io/badge/License-MIT-yellow.svg"></a>
    <a href="https://github.com/CMU-SAFARI/Athena/releases"><img alt="GitHub release" src="https://img.shields.io/github/release/CMU-SAFARI/Athena"></a>
    <a href="https://arxiv.org/abs/2601.17615"><img src="https://img.shields.io/badge/cs.AR-2601.17615-b31b1b?logo=arxiv&logoColor=red" alt="DOI"></a>
    <a href="https://doi.org/10.5281/zenodo.17854634"><img src="https://zenodo.org/badge/DOI/10.5281/zenodo.17854634.svg" alt="DOI"></a>
</p> -->

<details open="open">
  <summary>Table of Contents</summary>
  <ol>
    <li><a href="#what-is-PRISM">What is PRISM?</a></li>
    <li><a href="#set-up">Set Up</a></li>
    <li><a href="#preparing-traces">Preparing Traces</a></li>
    <li><a href="#running-experiments">Running Experiments</a></li>
    <li><a href="#understanding-results">Understanding Results</a></li>
    <li><a href="#brief-code-walkthrough">Brief Code Walkthrough</a></li>
    <li><a href="#citation">Citation</a></li>
    <li><a href="#license">License</a></li>
    <li><a href="#contact">Contact</a></li>
  </ol>
</details>


## What is PRISM?

**PRISM** is a high-performance temporal prefetcher. Motivated by a systematic miss attribution analysis, PRISM jointly addresses three major sources of non-compulsory cache misses: unseen correlations, late prefetches, and metadata inefficiency. It integrates **PC-Triggered Prefetching (PCTP)** to capture ⟨PC, Address⟩ correlations, **Timeliness-Guaranteed Prefetching (TGP)** to improve prefetch timeliness through static lookahead and adaptive degree control, and **Utility-Aware Management (UAM)** to maximize metadata efficiency via Bimodal Metadata Partitioning (BMP), selective insertion, and a fine-tuned SHiP replacement policy. Together, these techniques enable PRISM to consistently outperform state-of-the-art temporal prefetchers across diverse workloads. 

## Set Up

### 0. Prerequisite

This repository has been tested with the following system configuration:
- GNU Make 4.3
- GCC/G++ 11.4.0
- Python 3.10.12

### 1. Clone the repository
```bash
git clone 
cd PRISM
```

### 2. Set up the environment
```bash
bash setup.sh
```

## Preparing Traces

The trace dataset is hosted on **Hugging Face** and distributed separately from this repository due to its large size.

### Install the Hugging Face CLI (if needed)

```bash
pip install -U "huggingface_hub[cli]"
```

### Download the trace dataset

```bash
huggingface-cli download YiquanLin-ZJU/trace \
    --repo-type dataset \
    --local-dir trace
```

The complete evaluation uses **87 traces** from five benchmark suites:

* **Ligra:** 10 traces
* **GAP:** 9 traces
* **SPEC CPU 2017:** 11 traces
* **ML:** 15 traces
* **Google:** 42 traces

The expected directory structure is:

```text
PRISM/
├── trace/
│   ├── traces-gap/
│   ├── traces-google/
│   └── ...
├── scripts/
└── ...
```

## Running Experiments

PRISM provides a push-button script, `prism-expr.py`, to reproduce the major experimental results reported in the paper.

```bash
cd PRISM/scripts

python3 prism-expr.py -p <Phase> -f <FigureID>
```

### Phases

| Phase     | Description                                            |
| --------- | ------------------------------------------------------ |
| `Compile` | Build the required binaries.                           |
| `Run`     | Run the experiments for the selected figure.           |
| `Draw`    | Aggregate results and generate the figure.             |
| `All`     | Execute all of the above (`Compile` + `Run` + `Draw`). |

### Supported Figures

Each `FigureID` corresponds to a figure in the paper.

| FigureID | Description                                     |
| -------- | ----------------------------------------------- |
| `Fig10`  | IPC speedup on the single-core system           |
| `Fig11`  | Prefetch accuracy, timeliness, and DRAM traffic |
| `Fig12`  | Normalized energy consumption                   |
| `Fig13`  | Metadata analysis                               |
| `Fig16`  | Sensitivity analysis                            |
| `Fig18`  | Ablation study                                  |

### Example: Reproducing Figure 10

To reproduce **Figure 10** with a single command:

```bash
cd PRISM/scripts

python3 prism-expr.py -p All -f Fig10
```

Alternatively, execute each phase separately:

```bash
# Step 1: Build the binaries
python3 prism-expr.py -p Compile -f Fig10

# Step 2: Run the experiments
python3 prism-expr.py -p Run -f Fig10

# Wait for all jobs to finish (~3 hours for Fig10)

# Step 3: Aggregate results and generate the figure
python3 prism-expr.py -p Draw -f Fig10
```

## Understanding Results

### ChampSim Output

Simulation logs are stored in:

```text
experiments/champsim_log/<binary_name>/<trace_name>.log
```

### Aggregated Results

Processed results are available under:

```text
experiments/results/
├── basic/                    # Results for Fig. 10 and Fig. 11
├── energy/                   # Results for Fig. 12
├── metadata/                 # Results for Fig. 13
├── ablation/                 # Results for Fig. 18
└── ...
```

### Key Metrics

The primary evaluation metric is **IPC Speedup** over the baseline (IPCI):

```text
IPCI = IPC_experiment / IPC_baseline
```

Results are reported as **geometric means** and are summarized at two levels:

* **Per-suite:** Geometric mean across all traces within each benchmark suite (Ligra, GAP, SPEC, ML, and Google).
* **Overall:** Geometric mean across the benchmark suites.

## Brief Code Walkthrough

This repository is organized as follows:

```text
PRISM/
├── bin/                    # Compiled ChampSim binaries
├── branch/                 # Branch predictor implementations (ChampSim)
├── btb/                    # Branch target buffer implementations (ChampSim)
├── config/                 # ChampSim configuration files
├── docs/                   # Documentation
├── experiments/            # Experiment outputs
│   ├── champsim_log/       # Raw simulation logs
│   ├── figure_out/         # Generated figures
│   ├── results/            # Aggregated CSV results
│   └── ...
├── inc/                    # Header files
│   ├── prism_framework.h   # Core metadata framework used by PRISM
│   └── ...
├── prefetcher/             # Temporal prefetcher implementations
│   ├── baseline/           # Baseline temporal prefetcher
│   ├── prism/              # PRISM implementation
│   ├── triangel/           # Triangel prefetcher
│   ├── prophet/            # Prophet prefetcher
│   └── ...
├── replacement/            # Cache replacement policies (ChampSim)
├── scripts/                # Experiment automation
│   ├── champsim_config/    # ChampSim configuration files
│   ├── figure-scripts/     # Figure generation scripts
│   ├── prism-expr.py       # Main entry point for reproducing experiments
│   ├── compiler.py         # Binary compilation
│   ├── runner.py           # Experiment execution
│   ├── get_result.py       # Result collection and aggregation
│   └── ...
├── src/                    # ChampSim simulator source code
├── test/                   # Unit tests
├── trace/                  # Trace files (user must download separately)
├── Makefile                # Build configuration
└── setup.sh                # Environment setup script
```

## Citation

PRISM was accepted by MICRO 2026. If you find this repository useful, please cite the paper using:

```
@inproceedings{prism,
  title           = {{PRISM: A Miss-Attribution-Guided Temporal Prefetcher <br> Beyond Metadata Management}},
  author          = {Lin, Yiquan and Liu, Jianxiang and Chen, Yiquan and Lin, Wenhai and Wang, Zonghui and Chen, Wenzhi},
  booktitle       = {MICRO},
  year            = {2026}
}
```

## License

Distributed under the MIT License. See `LICENSE` for more information.

## Contact

Please contact [Yiquan Lin](linyiquan@zju.edu.cn) if you have any questions/suggestions.