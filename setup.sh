#!/usr/bin/env bash

# ChampSim uses [vcpkg](https://vcpkg.io) to manage its dependencies. 
# In this repository, vcpkg is included as a submodule. 
# This script is to download and install the dependencies.

set -e

echo "==> Updating Git Submodule..."
git submodule update --init

echo "==> Bootstrap vcpkg..."
./vcpkg/bootstrap-vcpkg.sh

echo "==> Installing dependencies..."
./vcpkg/vcpkg install

echo "Completion!"