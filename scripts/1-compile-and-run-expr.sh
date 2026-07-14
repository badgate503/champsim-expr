#!/bin/bash

# Install vcpkg and install the dependencies
# cd ..
# git submodule update --init
# vcpkg/bootstrap-vcpkg.sh
# vcpkg/vcpkg install
# cd scripts

# 1. Define the expected binary path for each prefetcher.
# (Please adjust the "champsim-...-1core" part to match your actual binary names)
declare -A PREFETCHERS=(
    ["baseline"]="../bin/baseline"
    ["triangel"]="../bin/triangel"
    ["prophet"]="../bin/prophet"
    ["prism"]="../bin/prism"
)

# 2. Loop through and check/compile
for pref in "${!PREFETCHERS[@]}"; do
    TARGET_BIN="${PREFETCHERS[$pref]}"
    
    echo "========================================"
    echo "Checking Prefetcher: $pref ..."
    
    # Check if the file exists and is a regular file
    if [ -f "$TARGET_BIN" ]; then
        echo " [✓] Binary found: $TARGET_BIN"
        echo "     Skipping compilation."
    else
        echo " [✗] Binary NOT found. Starting compilation..."
        
        # Run the compilation command
        ./compiler.py -p "$pref"
        
        # Check if the compilation succeeded
        if [ $? -eq 0 ]; then
            echo " [✓] $pref compiled successfully!"
        else
            echo " [💥] $pref compilation FAILED. Checking error logs."
            exit 1
        fi
    fi
done

echo "========================================"
echo "All checks and compilations completed."

# Executable files will be generated under {champsim_root}/bin
# Use runner.py to start a runner service

# submit the tasks
./runner.py -p no baseline triangel prophet prism -l ligra gap spec17 ml google -s

echo "========================================"
echo "All Champsim tasks finished"

