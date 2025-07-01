# Documentation Build Guide

## Quick Setup

The CMake setup handles Doxygen automatically:

```bash
# Install Doxygen (one-time setup)
# Ubuntu/Debian:
sudo apt-get install doxygen

# macOS:
brew install doxygen

# Windows: Download from https://www.doxygen.nl/download.html
# Or with Chocolatey: choco install doxygen.install

# Configure with documentation enabled
cmake -B build -DBUILD_DOCUMENTATION=ON

# Build documentation
cmake --build build --target docs

# Open documentation
build/docs/html/index.html
```