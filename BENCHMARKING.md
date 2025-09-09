# gSlapper Benchmarking Guide

This guide explains how to use the included benchmarking script to compare gSlapper performance against [mpvpaper](https://github.com/GhostNaN/mpvpaper) on your system.

## Quick Start

```bash
# Run a 5-minute benchmark (recommended for quick testing)
./benchmark.sh -t 300

# Run a comprehensive 30-minute benchmark
./benchmark.sh -t 1800
```

## Prerequisites

### Required Software
- Both `gslapper` and `mpvpaper` installed and accessible in PATH
- NVIDIA GPU with proprietary drivers (for GPU metrics)
- A test video file (script includes fallback if none provided)

### Recommended System State
- Close other GPU-intensive applications
- Ensure stable system load (no background downloads, compiling, etc.)
- Run on AC power (laptops) to avoid throttling
- Allow at least 15-30 minutes for meaningful results

## Script Options

### Basic Usage
```bash
./benchmark.sh [OPTIONS]

Options:
  -t, --time SECONDS     Test duration in seconds (default: 600)
  -v, --video FILE       Path to test video file
  -o, --output DIR       Output directory for results (default: ./benchmark_results)
  -h, --help            Show help message
```

### Example Commands
```bash
# Quick 2-minute test
./benchmark.sh -t 120

# Test with specific video file
./benchmark.sh -t 600 -v /path/to/your/video.mp4

# Save results to custom directory
./benchmark.sh -t 300 -o ~/my_benchmark_results
```

## What the Script Measures

### Performance Metrics
- **Frame Rate (FPS)**: Average frames per second during playback
- **Frame Drops**: Number of dropped/skipped frames
- **CPU Usage**: Average CPU utilization percentage
- **GPU Usage**: GPU core utilization percentage (NVIDIA only)
- **GPU Memory**: Video memory usage in MB
- **GPU Decoder**: Hardware decoder utilization percentage
- **System Memory**: RAM usage in MB

### Efficiency Metrics
- **Memory Growth**: Change in GPU memory from start to finish
- **CPU Efficiency**: CPU usage relative to frame rate delivered
- **GPU Efficiency**: GPU usage relative to video decoding workload
- **Resource Stability**: Consistency of resource usage over time

## Understanding Results

### CSV Output Files
The script generates detailed CSV files for each application tested:
- `gslapper_metrics_[timestamp].csv`: gSlapper performance data
- `mpvpaper_metrics_[timestamp].csv`: mpvpaper performance data
- `benchmark_summary_[timestamp].txt`: Comparative summary

### Key Performance Indicators

**Frame Rate Consistency**
- Higher average FPS = smoother playback
- Lower FPS variance = more consistent performance
- Zero frame drops = perfect playback quality

**Resource Efficiency**
- Lower CPU/GPU usage for same frame rate = better efficiency
- Stable memory usage = no memory leaks
- Hardware decoder usage = proper acceleration

**System Impact**
- Lower overall system resource usage
- Stable performance over time
- Minimal impact on other applications

## Interpreting Your Results

### Optimal gSlapper Performance
You should expect to see:
- Consistent 30/60 FPS (depending on video and monitor refresh rate)
- 0 frame drops during stable playback
- 4-8% GPU decoder usage (hardware acceleration active)
- Stable GPU memory usage (no significant growth over time)
- Lower CPU usage compared to mpvpaper

### Potential Issues

**Low Frame Rate**
- Check video file format and codec compatibility
- Verify GStreamer plugins are installed
- Test with different video files

**High CPU Usage**
- May indicate software decoding instead of hardware
- Check NVIDIA driver installation
- Verify GStreamer NVDEC plugin availability

**Memory Growth**
- Should be minimal for gSlapper
- Significant growth may indicate driver issues
- Compare against mpvpaper baseline

## System-Specific Considerations

### NVIDIA Systems
- Ensure proprietary drivers are installed
- Check `nvidia-smi` functionality before benchmarking
- GStreamer NVDEC/NVENC plugins should be available

### AMD/Intel Systems
- GPU metrics may not be available
- Focus on CPU usage and frame rate metrics
- Hardware acceleration support varies by driver

### Different Video Formats
- H.264: Best compatibility and hardware support
- H.265/HEVC: May require newer hardware for acceleration
- AV1: Limited hardware support, may fall back to software

## Troubleshooting

### Script Errors
```bash
# Verify both applications are installed
which gslapper mpvpaper

# Check NVIDIA driver status
nvidia-smi

# Test video file manually
gslapper -v /path/to/video.mp4 DP-1
```

### Performance Issues
```bash
# Enable debug output
GST_DEBUG=2 ./benchmark.sh -t 60

# Check GStreamer plugins
gst-inspect-1.0 | grep -i nvdec
```

### Unexpected Results
- Verify system is idle during testing
- Check for background processes affecting GPU/CPU
- Try different video files
- Compare with manual testing

## Contributing Benchmark Data

If you'd like to contribute your benchmark results to help improve gSlapper:

1. Run the standard 30-minute benchmark
2. Include system information (GPU model, driver version, distribution)
3. Note any specific configuration or issues encountered
4. Share results through GitHub issues or discussions

Example system info:
```bash
# Include this information with benchmark results
lscpu | grep "Model name"
nvidia-smi --query-gpu=name,driver_version --format=csv
uname -a
```

## Performance Tips

### For Best Results
- Use hardware-accelerated video formats (H.264/H.265)
- Ensure adequate system cooling
- Test on AC power (laptops)
- Close unnecessary applications
- Use SSD storage for video files

### Optimization
- Try different GStreamer pipeline options via `-o` flag
- Experiment with different video resolutions/bitrates
- Consider monitor refresh rate alignment
- Test both single and multi-monitor configurations

Remember: Performance varies significantly based on hardware, drivers, video content, and system configuration. The goal is to establish relative performance between gSlapper and mpvpaper on your specific setup.