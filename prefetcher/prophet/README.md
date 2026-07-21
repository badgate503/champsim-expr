# Prophet

Our implementation of **Prophet** is adapted from the official implementation:

> https://github.com/hkust-zhiyao/Prophet

The original Prophet is implemented in **gem5**. We reimplemented it in **ChampSim**.

## Profiling

Prophet is a **profile-guided** prefetcher. Before evaluating Prophet on a workload, profiling must be performed to generate workload-specific **hints**.

Compared with the original implementation, we make one simplification during hint generation:

> Instead of merging hints collected from multiple traces of the same application, we generate and use one hint file **per trace**.

This simplification avoids the hint-merging step and, in theory, provides hints that are more specialized for each individual trace, which may lead to slightly better performance on that trace.

## Building the Profiling Binary

To build the profiling version of Prophet, run:

```bash
cd scripts
python3 compiler.py -p prophet -e prophet_profile
```

This command generates a dedicated profiling binary:

```text
PRISM/bin/prophet_profile
```

## Collecting Hints

Run the generated `prophet_profile` binary on the target trace. During execution, Prophet will automatically collect profiling information and generate a hint file.

The hint file is written to a newly created `hint/` directory under the **parent directory of the target trace**. For example, if the trace is located at:

```text
PRISM/trace/traces-gap/bc-0.champsimtrace.gz
```

the generated hint will be placed at:

```text
PRISM/trace/hint/bc-0.txt
```

Each trace requires its own profiling run to generate the corresponding hint file.

## Pre-generated Hints

For the 87 traces used in the PRISM evaluation, we have already generated the corresponding hint files. They are available in the trace repository, so no additional profiling is required to reproduce our experimental results.