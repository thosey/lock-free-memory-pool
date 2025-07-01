# LockFreeMemoryPool Benchmarks

This directory contains performance benchmarks for the LockFreeMemoryPool library using Google Benchmark.

### Prerequisites
Install Google Benchmark:
```bash
# Ubuntu/Debian
sudo apt-get install libbenchmark-dev

# macOS
brew install google-benchmark

### Running Benchmarks
```bash
# From the main project directory
cmake -B build -DBUILD_BENCHMARKS=ON
cmake --build build

# Run benchmarks
make -C build run_google_benchmark

# Generate JSON report for analysis
make -C build benchmark_report
```

### Custom Benchmark Runs
```bash
# Run specific benchmarks
./google_benchmark --benchmark_filter=".*Pool.*"

# Control benchmark parameters
./google_benchmark --benchmark_min_time=2.0 --benchmark_repetitions=10

# Output formats
./google_benchmark --benchmark_format=json --benchmark_out=results.json
./google_benchmark --benchmark_format=csv --benchmark_out=results.csv
```

## Learning Resources

- [Google Benchmark User Guide](https://github.com/google/benchmark/blob/main/docs/user_guide.md)
- [Performance Testing Best Practices](https://github.com/google/benchmark/blob/main/docs/random_interleaving.md)

## Integration with Main Project

The benchmarks are automatically integrated with the main build system:
- Enable with `-DBUILD_BENCHMARKS=ON` 
- Disabled by default to avoid Google Benchmark dependency
