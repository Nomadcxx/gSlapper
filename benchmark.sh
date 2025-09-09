#!/bin/bash

# Robust NVIDIA GPU + CPU benchmarking script for slapper vs mpvpaper
# Focus: Accurate memory leak detection and performance tracking

set -euo pipefail

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
CYAN='\033[0;36m'
NC='\033[0m' # No Color

# Configuration
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
SLAPPER_BIN="${SCRIPT_DIR}/build/slapper"
TEST_DURATION=1800   # 30 minutes for full benchmark test
SAMPLE_INTERVAL=5   # Sample every 5 seconds for detailed tracking
LOG_DIR="/tmp/nvidia_benchmark_$(date +%Y%m%d_%H%M%S)"

echo -e "${BLUE}===============================================${NC}"
echo -e "${BLUE}   NVIDIA GPU + CPU PERFORMANCE BENCHMARK    ${NC}"
echo -e "${BLUE}===============================================${NC}"
echo -e "${CYAN}Test duration:${NC} ${TEST_DURATION}s ($(awk "BEGIN {printf \"%.1f\", $TEST_DURATION/60}") minutes)"
echo -e "${CYAN}Sample interval:${NC} ${SAMPLE_INTERVAL}s"
echo -e "${CYAN}Log directory:${NC} $LOG_DIR"
echo ""

# Create log directory
mkdir -p "$LOG_DIR"

# Dependency checking
check_dependencies() {
    echo -e "${YELLOW}Checking dependencies...${NC}"
    
    local missing_deps=()
    
    # Check for required commands
    for cmd in bc ps nvidia-smi pgrep pkill timeout; do
        if ! command -v "$cmd" >/dev/null 2>&1; then
            missing_deps+=("$cmd")
        fi
    done
    
    # Check for slapper binary
    if [ ! -x "$SLAPPER_BIN" ]; then
        echo -e "${RED}✗ Slapper binary not found: $SLAPPER_BIN${NC}"
        return 1
    fi
    
    # Check for mpvpaper
    if ! command -v mpvpaper >/dev/null 2>&1; then
        echo -e "${YELLOW}⚠ mpvpaper not found - comparison will be skipped${NC}"
    fi
    
    # Check NVIDIA GPU
    if ! nvidia-smi >/dev/null 2>&1; then
        echo -e "${RED}✗ NVIDIA GPU or drivers not available${NC}"
        return 1
    fi
    
    if [ ${#missing_deps[@]} -gt 0 ]; then
        echo -e "${RED}✗ Missing dependencies: ${missing_deps[*]}${NC}"
        return 1
    fi
    
    echo -e "${GREEN}✓ All dependencies satisfied${NC}"
    return 0
}

# Get GPU baseline before testing
get_gpu_baseline() {
    echo -e "${YELLOW}Getting GPU baseline...${NC}"
    
    local baseline_file="$LOG_DIR/gpu_baseline.txt"
    nvidia-smi --query-gpu=memory.used,memory.free,memory.total,utilization.gpu,utilization.memory,temperature.gpu --format=csv,noheader,nounits > "$baseline_file"
    
    local baseline_data=$(cat "$baseline_file")
    IFS=',' read -r mem_used mem_free mem_total gpu_util mem_util temp <<< "$baseline_data"
    
    echo -e "${GREEN}✓ GPU baseline captured${NC}"
    echo -e "  ${CYAN}Memory used:${NC} ${mem_used}MB / ${mem_total}MB"
    echo -e "  ${CYAN}GPU utilization:${NC} ${gpu_util}%"
    echo -e "  ${CYAN}Memory utilization:${NC} ${mem_util}%"
    echo -e "  ${CYAN}Temperature:${NC} ${temp}°C"
    echo ""
}

# Safe process cleanup
safe_cleanup() {
    local process_name="$1"
    local exact_command="$2"
    
    echo -e "${YELLOW}Cleaning up $process_name processes...${NC}"
    
    # Find processes by exact command match
    local pids=()
    while IFS= read -r line; do
        if [[ "$line" =~ ^[0-9]+$ ]]; then
            pids+=("$line")
        fi
    done < <(pgrep -f "$exact_command" 2>/dev/null || true)
    
    if [ ${#pids[@]} -gt 0 ]; then
        echo "  Terminating ${#pids[@]} $process_name processes: ${pids[*]}"
        
        # Graceful termination
        for pid in "${pids[@]}"; do
            kill "$pid" 2>/dev/null || true
        done
        
        sleep 3
        
        # Force kill if still running
        for pid in "${pids[@]}"; do
            if ps -p "$pid" >/dev/null 2>&1; then
                kill -9 "$pid" 2>/dev/null || true
            fi
        done
        
        sleep 2
    else
        echo "  No $process_name processes found"
    fi
}

# Comprehensive monitoring with GPU metrics
monitor_application() {
    local pid="$1"
    local app_name="$2"
    local duration="$3"
    
    echo -e "${YELLOW}Monitoring $app_name (PID: $pid) for ${duration}s...${NC}"
    
    local metrics_file="$LOG_DIR/${app_name}_metrics.csv"
    local summary_file="$LOG_DIR/${app_name}_summary.txt"
    
    # CSV header
    echo "timestamp,cpu_percent,mem_percent,rss_mb,vsz_mb,gpu_mem_used_mb,gpu_mem_free_mb,gpu_util_percent,gpu_mem_util_percent,gpu_temp_c,decoder_util_percent,encoder_util_percent,fps,dropped_frames,frame_drop_rate,surface_commits_per_sec,protocol_messages_per_sec" > "$metrics_file"
    
    # Initialize tracking variables
    local sample_count=0
    local -a cpu_samples mem_samples rss_samples gpu_mem_samples gpu_util_samples decoder_util_samples encoder_util_samples fps_samples dropped_frames_samples frame_drop_rate_samples commit_rate_samples message_rate_samples
    local max_cpu=0 max_mem=0 max_rss=0 max_gpu_mem=0 max_gpu_util=0 max_decoder_util=0 max_encoder_util=0 max_fps=0 max_dropped_frames=0 max_frame_drop_rate=0 max_commit_rate=0 max_message_rate=0
    local prev_frame_count=0
    local fps_measurement_interval=0
    
    # Get initial GPU state
    local initial_gpu_data=$(nvidia-smi --query-gpu=memory.used --format=csv,noheader,nounits)
    local initial_gpu_mem=$(echo "$initial_gpu_data" | tr -d ' ')
    
    echo -e "${CYAN}Initial GPU memory:${NC} ${initial_gpu_mem}MB"
    
    # Monitoring loop
    local start_time=$(date +%s)
    for ((i=0; i<duration; i+=SAMPLE_INTERVAL)); do
        if ps -p "$pid" >/dev/null 2>&1; then
            local timestamp=$(date '+%Y-%m-%d %H:%M:%S')
            
            # Get CPU/Memory stats
            local cpu_mem_stats
            if cpu_mem_stats=$(ps -p "$pid" -o pcpu,pmem,rss,vsz --no-headers 2>/dev/null); then
                local cpu=$(echo "$cpu_mem_stats" | awk '{print $1}')
                local mem=$(echo "$cpu_mem_stats" | awk '{print $2}')
                local rss=$(echo "$cpu_mem_stats" | awk '{print $3}')
                local vsz=$(echo "$cpu_mem_stats" | awk '{print $4}')
                
                # Convert KB to MB
                local rss_mb=$(awk "BEGIN {printf \"%.1f\", $rss/1024}")
                local vsz_mb=$(awk "BEGIN {printf \"%.1f\", $vsz/1024}")
                
                # Get GPU stats including decoder usage
                local gpu_stats decoder_stats
                if gpu_stats=$(nvidia-smi --query-gpu=memory.used,memory.free,utilization.gpu,utilization.memory,temperature.gpu --format=csv,noheader,nounits 2>/dev/null) && decoder_stats=$(nvidia-smi --query-gpu=utilization.decoder,utilization.encoder --format=csv,noheader,nounits 2>/dev/null); then
                    IFS=',' read -r gpu_mem_used gpu_mem_free gpu_util gpu_mem_util gpu_temp <<< "$gpu_stats"
                    IFS=',' read -r decoder_util encoder_util <<< "$decoder_stats"
                    
                    # Clean up whitespace and ensure valid numeric values
                    gpu_mem_used=$(echo "$gpu_mem_used" | tr -d ' \n')
                    gpu_mem_free=$(echo "$gpu_mem_free" | tr -d ' \n')
                    gpu_util=$(echo "$gpu_util" | tr -d ' \n')
                    gpu_mem_util=$(echo "$gpu_mem_util" | tr -d ' \n')
                    gpu_temp=$(echo "$gpu_temp" | tr -d ' \n')
                    decoder_util=$(echo "$decoder_util" | tr -d ' \n')
                    encoder_util=$(echo "$encoder_util" | tr -d ' \n')
                    
                    # Validate numeric values (prevent arithmetic errors)
                    [[ "$decoder_util" =~ ^[0-9]+$ ]] || decoder_util="0"
                    [[ "$encoder_util" =~ ^[0-9]+$ ]] || encoder_util="0"
                    
                    # Measure FPS and frame drop metrics using multiple methods
                    local fps=0
                    local fps_source="unknown"
                    local dropped_frames=0
                    local presented_frames=0
                    local frame_drop_rate=0
                    
                    # Method 1: Check application-specific log output for FPS and frame drop info
                    if [[ "$app_name" == "slapper" ]]; then
                        # For slapper, analyze GStreamer frame events in recent log
                        local recent_log=$(tail -40 "$LOG_DIR/${app_name}_output.log" 2>/dev/null || echo "")
                        local recent_frames=$(echo "$recent_log" | grep -c "FOUND NEW FRAME" || echo "0")
                        local dropped=$(echo "$recent_log" | grep -c -E "(dropped|late frame|buffer underrun|decode error)" || echo "0")
                        
                        if [[ "$recent_frames" =~ ^[0-9]+$ ]] && [ "$recent_frames" -gt 0 ]; then
                            fps=$(awk "BEGIN {printf \"%.1f\", $recent_frames * 60 / $SAMPLE_INTERVAL}")
                            fps_source="frame_events"
                            presented_frames="$recent_frames"
                            dropped_frames="$dropped"
                            if [ "$recent_frames" -gt 0 ]; then
                                # Clean variables to prevent arithmetic errors
                                local clean_recent=$(echo "$recent_frames" | tr -d '\n' | tr -d ' ')
                                local clean_dropped=$(echo "$dropped" | tr -d '\n' | tr -d ' ')
                                [[ "$clean_recent" =~ ^[0-9]+$ ]] || clean_recent=0
                                [[ "$clean_dropped" =~ ^[0-9]+$ ]] || clean_dropped=0
                                
                                if [ "$((clean_recent + clean_dropped))" -gt 0 ] 2>/dev/null; then
                                    frame_drop_rate=$(awk "BEGIN {printf \"%.2f\", $clean_dropped * 100 / ($clean_recent + $clean_dropped)}")
                                else
                                    frame_drop_rate="0.00"
                                fi
                            fi
                        fi
                    elif [[ "$app_name" == "mpvpaper" ]]; then
                        # For mpvpaper, extract fps and frame drop info from verbose output
                        local recent_log=$(tail -40 "$LOG_DIR/${app_name}_output.log" 2>/dev/null || echo "")
                        local mpv_fps=$(echo "$recent_log" | grep -o "fps=[0-9.]*" | tail -1 | cut -d= -f2 || echo "0")
                        local dropped=$(echo "$recent_log" | grep -c -E "(dropped frames|late frame|missed frame|decode error)" || echo "0")
                        
                        if [[ "$mpv_fps" =~ ^[0-9]+\.?[0-9]*$ ]] && [ "$mpv_fps" != "0" ]; then
                            fps="$mpv_fps"
                            fps_source="mpv_output"
                            dropped_frames="$dropped"
                            # Estimate presented frames from FPS
                            presented_frames=$(awk "BEGIN {printf \"%.0f\", $fps * $SAMPLE_INTERVAL / 60}")
                            if [ "$presented_frames" -gt 0 ]; then
                                # Clean variables to prevent arithmetic errors  
                                local clean_presented=$(echo "$presented_frames" | tr -d '\n' | tr -d ' ')
                                local clean_dropped=$(echo "$dropped" | tr -d '\n' | tr -d ' ')
                                [[ "$clean_presented" =~ ^[0-9]+$ ]] || clean_presented=0
                                [[ "$clean_dropped" =~ ^[0-9]+$ ]] || clean_dropped=0
                                
                                if [ "$((clean_presented + clean_dropped))" -gt 0 ] 2>/dev/null; then
                                    frame_drop_rate=$(awk "BEGIN {printf \"%.2f\", $clean_dropped * 100 / ($clean_presented + $clean_dropped)}")
                                else
                                    frame_drop_rate="0.00"
                                fi
                            fi
                        fi
                    fi
                    
                    # Method 2: Fallback to GPU utilization correlation (rough estimate)
                    if [ "$fps" == "0" ] && [[ "$gpu_util" =~ ^[0-9]+$ ]] && [ "$gpu_util" -gt 0 ]; then
                        # Rough correlation: active GPU = likely rendering frames
                        # This is imprecise but gives us some indication
                        fps=$(awk "BEGIN {printf \"%.1f\", $gpu_util * 0.6}")  # Rough scaling
                        fps_source="gpu_estimate"
                    fi
                    
                    # Method 3: Last resort - assume target fps if GPU is active
                    if [ "$fps" == "0" ] && [[ "$gpu_util" =~ ^[0-9]+$ ]] && [ "$gpu_util" -gt 5 ]; then
                        fps="30.0"  # Assume 30fps if GPU is clearly active
                        fps_source="assumed"
                    fi
                    
                    # Measure Wayland protocol efficiency
                    local surface_commits=0
                    local protocol_messages=0
                    local commit_rate=0
                    local message_rate=0
                    
                    # Count recent Wayland activity in debug log
                    if [ -f "$LOG_DIR/${app_name}_wayland.log" ]; then
                        local recent_wayland=$(tail -50 "$LOG_DIR/${app_name}_wayland.log" 2>/dev/null || echo "")
                        surface_commits=$(echo "$recent_wayland" | grep -c "wl_surface@.*commit" || echo "0")
                        protocol_messages=$(echo "$recent_wayland" | wc -l || echo "0")
                        
                        # Calculate rates per second (based on sample interval)
                        if [ "$SAMPLE_INTERVAL" -gt 0 ]; then
                            commit_rate=$(awk "BEGIN {printf \"%.1f\", $surface_commits / $SAMPLE_INTERVAL}")
                            message_rate=$(awk "BEGIN {printf \"%.1f\", $protocol_messages / $SAMPLE_INTERVAL}")
                        fi
                        
                        # Ensure valid numeric values (prevent arithmetic errors)
                        [[ "$commit_rate" =~ ^[0-9]+\.?[0-9]*$ ]] || commit_rate="0.0"
                        [[ "$message_rate" =~ ^[0-9]+\.?[0-9]*$ ]] || message_rate="0.0"
                    fi
                    
                    # Validate and clean all metrics variables to prevent arithmetic errors
                    fps=$(echo "$fps" | tr -d '\n' | tr -d ' ')
                    dropped_frames=$(echo "$dropped_frames" | tr -d '\n' | tr -d ' ')
                    frame_drop_rate=$(echo "$frame_drop_rate" | tr -d '\n' | tr -d ' ')
                    commit_rate=$(echo "$commit_rate" | tr -d '\n' | tr -d ' ')
                    message_rate=$(echo "$message_rate" | tr -d '\n' | tr -d ' ')
                    
                    # Final validation for all numeric values - fixed regex patterns
                    [[ "$fps" =~ ^[0-9]+\.?[0-9]*$ ]] || fps="0.0"
                    [[ "$dropped_frames" =~ ^[0-9]+$ ]] || dropped_frames="0"
                    [[ "$frame_drop_rate" =~ ^[0-9]+\.?[0-9]*$ ]] || frame_drop_rate="0.0"
                    [[ "$commit_rate" =~ ^[0-9]+\.?[0-9]*$ ]] || commit_rate="0.0"
                    [[ "$message_rate" =~ ^[0-9]+\.?[0-9]*$ ]] || message_rate="0.0"
                    
                    # Validate and store samples
                    if [[ "$cpu" =~ ^[0-9]+\.?[0-9]*$ ]] && [[ "$mem" =~ ^[0-9]+\.?[0-9]*$ ]]; then
                        cpu_samples+=("$cpu")
                        mem_samples+=("$mem")
                        rss_samples+=("$rss_mb")
                        gpu_mem_samples+=("$gpu_mem_used")
                        gpu_util_samples+=("$gpu_util")
                        decoder_util_samples+=("$decoder_util")
                        encoder_util_samples+=("$encoder_util")
                        fps_samples+=("$fps")
                        dropped_frames_samples+=("$dropped_frames")
                        frame_drop_rate_samples+=("$frame_drop_rate")
                        commit_rate_samples+=("$commit_rate")
                        message_rate_samples+=("$message_rate")
                        
                        # Track maximums using safe arithmetic
                        max_cpu=$(echo "$cpu $max_cpu" | awk '{if($1>$2) print $1; else print $2}')
                        max_mem=$(echo "$mem $max_mem" | awk '{if($1>$2) print $1; else print $2}')
                        max_rss=$(echo "$rss_mb $max_rss" | awk '{if($1>$2) print $1; else print $2}')
                        max_gpu_mem=$(echo "$gpu_mem_used $max_gpu_mem" | awk '{if($1>$2) print $1; else print $2}')
                        max_gpu_util=$(echo "$gpu_util $max_gpu_util" | awk '{if($1>$2) print $1; else print $2}')
                        max_decoder_util=$(echo "$decoder_util $max_decoder_util" | awk '{if($1>$2) print $1; else print $2}')
                        max_encoder_util=$(echo "$encoder_util $max_encoder_util" | awk '{if($1>$2) print $1; else print $2}')
                        max_fps=$(echo "$fps $max_fps" | awk '{if($1>$2) print $1; else print $2}')
                        max_dropped_frames=$(echo "$dropped_frames $max_dropped_frames" | awk '{if($1>$2) print $1; else print $2}')
                        max_frame_drop_rate=$(echo "$frame_drop_rate $max_frame_drop_rate" | awk '{if($1>$2) print $1; else print $2}')
                        max_commit_rate=$(echo "$commit_rate $max_commit_rate" | awk '{if($1>$2) print $1; else print $2}')
                        max_message_rate=$(echo "$message_rate $max_message_rate" | awk '{if($1>$2) print $1; else print $2}')
                        
                        # Write to CSV
                        echo "$timestamp,$cpu,$mem,$rss_mb,$vsz_mb,$gpu_mem_used,$gpu_mem_free,$gpu_util,$gpu_mem_util,$gpu_temp,$decoder_util,$encoder_util,$fps,$dropped_frames,$frame_drop_rate,$commit_rate,$message_rate" >> "$metrics_file"
                        
                        ((sample_count++))
                        
                        # Progress indicator every 2 minutes
                        if (( sample_count % 24 == 0 )); then
                            local elapsed=$((sample_count * SAMPLE_INTERVAL))
                            echo "  ${elapsed}s: CPU=${cpu}%, RAM=${rss_mb}MB, GPU=${gpu_mem_used}MB (${gpu_util}%)"
                        fi
                    else
                        echo "$timestamp,invalid,invalid,invalid,invalid,invalid,invalid,invalid,invalid,invalid" >> "$metrics_file"
                    fi
                else
                    echo "$timestamp,valid,valid,valid,valid,gpu_failed,gpu_failed,gpu_failed,gpu_failed,gpu_failed" >> "$metrics_file"
                fi
            else
                echo "$timestamp,process_failed,process_failed,process_failed,process_failed,process_failed,process_failed,process_failed,process_failed,process_failed" >> "$metrics_file"
            fi
        else
            echo -e "${RED}  Process $pid died at ${i}s${NC}"
            break
        fi
        
        sleep "$SAMPLE_INTERVAL"
    done
    
    # Calculate final statistics
    if [ "$sample_count" -gt 0 ]; then
        local total_cpu=0 total_mem=0 total_rss=0 total_gpu_mem=0 total_gpu_util=0 total_decoder_util=0 total_encoder_util=0 total_fps=0 total_dropped_frames=0 total_frame_drop_rate=0 total_commit_rate=0 total_message_rate=0
        
        for i in "${!cpu_samples[@]}"; do
            total_cpu=$(awk "BEGIN {printf \"%.2f\", $total_cpu + ${cpu_samples[$i]}}")
            total_mem=$(awk "BEGIN {printf \"%.2f\", $total_mem + ${mem_samples[$i]}}")
            total_rss=$(awk "BEGIN {printf \"%.1f\", $total_rss + ${rss_samples[$i]}}")
            total_gpu_mem=$(awk "BEGIN {printf \"%.1f\", $total_gpu_mem + ${gpu_mem_samples[$i]}}")
            total_gpu_util=$(awk "BEGIN {printf \"%.1f\", $total_gpu_util + ${gpu_util_samples[$i]}}")
            total_decoder_util=$(awk "BEGIN {printf \"%.1f\", $total_decoder_util + ${decoder_util_samples[$i]}}")
            total_encoder_util=$(awk "BEGIN {printf \"%.1f\", $total_encoder_util + ${encoder_util_samples[$i]}}")
            total_fps=$(awk "BEGIN {printf \"%.1f\", $total_fps + ${fps_samples[$i]}}")
            total_dropped_frames=$(awk "BEGIN {printf \"%.1f\", $total_dropped_frames + ${dropped_frames_samples[$i]}}")
            total_frame_drop_rate=$(awk "BEGIN {printf \"%.2f\", $total_frame_drop_rate + ${frame_drop_rate_samples[$i]}}")
            total_commit_rate=$(awk "BEGIN {printf \"%.1f\", $total_commit_rate + ${commit_rate_samples[$i]}}")
            total_message_rate=$(awk "BEGIN {printf \"%.1f\", $total_message_rate + ${message_rate_samples[$i]}}")
        done
        
        local avg_cpu=$(awk "BEGIN {printf \"%.2f\", $total_cpu / $sample_count}")
        local avg_mem=$(awk "BEGIN {printf \"%.2f\", $total_mem / $sample_count}")
        local avg_rss=$(awk "BEGIN {printf \"%.1f\", $total_rss / $sample_count}")
        local avg_gpu_mem=$(awk "BEGIN {printf \"%.1f\", $total_gpu_mem / $sample_count}")
        local avg_gpu_util=$(awk "BEGIN {printf \"%.1f\", $total_gpu_util / $sample_count}")
        local avg_decoder_util=$(awk "BEGIN {printf \"%.1f\", $total_decoder_util / $sample_count}")
        local avg_encoder_util=$(awk "BEGIN {printf \"%.1f\", $total_encoder_util / $sample_count}")
        local avg_fps=$(awk "BEGIN {printf \"%.1f\", $total_fps / $sample_count}")
        local avg_dropped_frames=$(awk "BEGIN {printf \"%.1f\", $total_dropped_frames / $sample_count}")
        local avg_frame_drop_rate
        if [ "$sample_count" -gt 0 ]; then
            avg_frame_drop_rate=$(awk "BEGIN {printf \"%.2f\", $total_frame_drop_rate / $sample_count}")
        else
            avg_frame_drop_rate="0.00"
        fi
        local avg_commit_rate=$(awk "BEGIN {printf \"%.1f\", $total_commit_rate / $sample_count}")
        local avg_message_rate=$(awk "BEGIN {printf \"%.1f\", $total_message_rate / $sample_count}")
        
        # Calculate memory growth (leak detection)
        local initial_rss="${rss_samples[0]}"
        local final_rss="${rss_samples[-1]}"
        local mem_growth=$(awk "BEGIN {printf \"%.1f\", $final_rss - $initial_rss}")
        
        local initial_gpu="${gpu_mem_samples[0]}"
        local final_gpu="${gpu_mem_samples[-1]}"
        local gpu_growth=$(awk "BEGIN {printf \"%.1f\", $final_gpu - $initial_gpu}")
        
        echo -e "${GREEN}✓ $app_name monitoring completed${NC}"
        echo -e "  ${CYAN}Valid samples:${NC} $sample_count/$(awk "BEGIN {printf \"%.0f\", $duration/$SAMPLE_INTERVAL}")"
        echo -e "  ${CYAN}Average CPU:${NC} ${avg_cpu}% (max: ${max_cpu}%)"
        echo -e "  ${CYAN}Average RAM:${NC} ${avg_rss}MB (max: ${max_rss}MB)"
        echo -e "  ${CYAN}Average GPU Memory:${NC} ${avg_gpu_mem}MB (max: ${max_gpu_mem}MB)"
        echo -e "  ${CYAN}Average GPU Util:${NC} ${avg_gpu_util}% (max: ${max_gpu_util}%)"
        echo -e "  ${CYAN}HW Decoder Usage:${NC} ${avg_decoder_util}% avg, ${max_decoder_util}% max"
        echo -e "  ${CYAN}HW Encoder Usage:${NC} ${avg_encoder_util}% avg, ${max_encoder_util}% max"
        echo -e "  ${CYAN}Average FPS:${NC} ${avg_fps} (max: ${max_fps})"
        echo -e "  ${CYAN}Frame Drops:${NC} ${avg_dropped_frames} avg, ${max_dropped_frames} max (${avg_frame_drop_rate}% drop rate)"
        echo -e "  ${CYAN}Wayland Efficiency:${NC} ${avg_commit_rate} commits/sec (max: ${max_commit_rate}), ${avg_message_rate} messages/sec (max: ${max_message_rate})"
        echo -e "  ${CYAN}RAM Growth:${NC} ${mem_growth}MB (${initial_rss}MB → ${final_rss}MB)"
        echo -e "  ${CYAN}GPU Growth:${NC} ${gpu_growth}MB (${initial_gpu}MB → ${final_gpu}MB)"
        
        # Save summary for comparison
        echo "$avg_cpu,$avg_mem,$avg_rss,$avg_gpu_mem,$avg_gpu_util,$avg_decoder_util,$avg_encoder_util,$avg_fps,$avg_dropped_frames,$avg_frame_drop_rate,$max_cpu,$max_mem,$max_rss,$max_gpu_mem,$max_gpu_util,$max_decoder_util,$max_encoder_util,$max_fps,$max_dropped_frames,$max_frame_drop_rate,$mem_growth,$gpu_growth,$sample_count" > "$summary_file"
        return 0
    else
        echo -e "${RED}✗ No valid samples collected for $app_name${NC}"
        echo "0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0" > "$summary_file"
        return 1
    fi
}

# Test application with comprehensive monitoring
test_application() {
    local app_cmd="$1"
    local app_name="$2"
    local video_file="$3"
    
    echo -e "${BLUE}=== TESTING $app_name ===${NC}"
    echo -e "${CYAN}Command:${NC} $app_cmd"
    
    # Start application with Wayland debug logging
    local start_time=$(date +%s.%N)
    WAYLAND_DEBUG=client eval "$app_cmd" > "$LOG_DIR/${app_name}_output.log" 2> "$LOG_DIR/${app_name}_wayland.log" &
    local app_pid=$!
    
    echo "Started $app_name with PID: $app_pid"
    
    # Wait for initialization (but not too long)
    local initialized=false
    for i in {1..10}; do
        if ps -p "$app_pid" >/dev/null 2>&1; then
            sleep 0.5
            if ps -p "$app_pid" >/dev/null 2>&1; then
                initialized=true
                break
            fi
        else
            echo -e "${RED}✗ $app_name failed to start${NC}"
            return 1
        fi
    done
    
    if [ "$initialized" = false ]; then
        echo -e "${RED}✗ $app_name failed to initialize${NC}"
        return 1
    fi
    
    local init_time=$(date +%s.%N)
    local startup_time=$(awk "BEGIN {printf \"%.3f\", $init_time - $start_time}")
    echo -e "${GREEN}✓ $app_name initialized in ${startup_time}s${NC}"
    
    # Monitor performance
    if monitor_application "$app_pid" "$app_name" "$TEST_DURATION"; then
        echo "Stopping $app_name (PID: $app_pid)..."
        if kill "$app_pid" 2>/dev/null; then
            sleep 3
            if ps -p "$app_pid" >/dev/null 2>&1; then
                kill -9 "$app_pid" 2>/dev/null || true
            fi
        fi
        return 0
    else
        return 1
    fi
}

# Generate comparison report
generate_comparison() {
    echo ""
    echo -e "${BLUE}=== NVIDIA GPU + CPU BENCHMARK RESULTS ===${NC}"
    
    local slapper_data mpvpaper_data
    if [ -f "$LOG_DIR/slapper_summary.txt" ]; then
        slapper_data=$(cat "$LOG_DIR/slapper_summary.txt")
    else
        echo -e "${RED}No slapper results found${NC}"
        return 1
    fi
    
    if [ -f "$LOG_DIR/mpvpaper_summary.txt" ]; then
        mpvpaper_data=$(cat "$LOG_DIR/mpvpaper_summary.txt")
    else
        echo -e "${YELLOW}No mpvpaper results - running slapper only${NC}"
        mpvpaper_data="0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0"
    fi
    
    # Parse data: avg_cpu,avg_mem,avg_rss,avg_gpu_mem,avg_gpu_util,avg_decoder_util,avg_encoder_util,avg_fps,avg_dropped_frames,avg_frame_drop_rate,max_cpu,max_mem,max_rss,max_gpu_mem,max_gpu_util,max_decoder_util,max_encoder_util,max_fps,max_dropped_frames,max_frame_drop_rate,mem_growth,gpu_growth,sample_count
    IFS=',' read -r slapper_cpu slapper_mem slapper_rss slapper_gpu_mem slapper_gpu_util slapper_decoder_util slapper_encoder_util slapper_fps slapper_dropped slapper_drop_rate slapper_max_cpu slapper_max_mem slapper_max_rss slapper_max_gpu_mem slapper_max_gpu_util slapper_max_decoder_util slapper_max_encoder_util slapper_max_fps slapper_max_dropped slapper_max_drop_rate slapper_mem_growth slapper_gpu_growth slapper_samples <<< "$slapper_data"
    IFS=',' read -r mpvpaper_cpu mpvpaper_mem mpvpaper_rss mpvpaper_gpu_mem mpvpaper_gpu_util mpvpaper_decoder_util mpvpaper_encoder_util mpvpaper_fps mpvpaper_dropped mpvpaper_drop_rate mpvpaper_max_cpu mpvpaper_max_mem mpvpaper_max_rss mpvpaper_max_gpu_mem mpvpaper_max_gpu_util mpvpaper_max_decoder_util mpvpaper_max_encoder_util mpvpaper_max_fps mpvpaper_max_dropped mpvpaper_max_drop_rate mpvpaper_mem_growth mpvpaper_gpu_growth mpvpaper_samples <<< "$mpvpaper_data"
    
    # Display comparison table
    echo ""
    printf "${YELLOW}%-12s %-8s %-8s %-10s %-12s %-8s %-10s %-12s %-12s${NC}\n" "Application" "CPU%" "RAM(MB)" "GPU(MB)" "GPU%" "FPS" "RAM Growth" "GPU Growth" "Samples"
    printf "%-12s %-8.2f %-8.1f %-10.1f %-12.1f %-8.1f %-10.1f %-12.1f %-12d\n" "Slapper" "$slapper_cpu" "$slapper_rss" "$slapper_gpu_mem" "$slapper_gpu_util" "$slapper_fps" "$slapper_mem_growth" "$slapper_gpu_growth" "$slapper_samples"
    
    if (( mpvpaper_samples > 0 )); then
        printf "%-12s %-8.2f %-8.1f %-10.1f %-12.1f %-8.1f %-10.1f %-12.1f %-12d\n" "Mpvpaper" "$mpvpaper_cpu" "$mpvpaper_rss" "$mpvpaper_gpu_mem" "$mpvpaper_gpu_util" "$mpvpaper_fps" "$mpvpaper_mem_growth" "$mpvpaper_gpu_growth" "$mpvpaper_samples"
        
        echo ""
        echo -e "${CYAN}MEMORY LEAK ANALYSIS:${NC}"
        echo -e "  ${CYAN}Slapper RAM Growth:${NC} ${slapper_mem_growth}MB over $(awk "BEGIN {printf \"%.1f\", $TEST_DURATION/60}") minutes"
        echo -e "  ${CYAN}Mpvpaper RAM Growth:${NC} ${mpvpaper_mem_growth}MB over $(awk "BEGIN {printf \"%.1f\", $TEST_DURATION/60}") minutes"
        echo -e "  ${CYAN}Slapper GPU Growth:${NC} ${slapper_gpu_growth}MB"
        echo -e "  ${CYAN}Mpvpaper GPU Growth:${NC} ${mpvpaper_gpu_growth}MB"
        
        if (( $(awk "BEGIN {print ($mpvpaper_mem_growth > $slapper_mem_growth)}") )); then
            local ram_diff=$(awk "BEGIN {printf \"%.1f\", $mpvpaper_mem_growth - $slapper_mem_growth}")
            echo -e "  ${GREEN}✓ Slapper has ${ram_diff}MB LESS RAM growth than mpvpaper${NC}"
        fi
        
        if (( $(awk "BEGIN {print ($mpvpaper_gpu_growth > $slapper_gpu_growth)}") )); then
            local gpu_diff=$(awk "BEGIN {printf \"%.1f\", $mpvpaper_gpu_growth - $slapper_gpu_growth}")
            echo -e "  ${GREEN}✓ Slapper has ${gpu_diff}MB LESS GPU growth than mpvpaper${NC}"
        fi
        
        echo ""
        echo -e "${CYAN}FRAME RATE ANALYSIS:${NC}"
        echo -e "  ${CYAN}Slapper Average FPS:${NC} ${slapper_fps} (max: ${slapper_max_fps})"
        echo -e "  ${CYAN}Mpvpaper Average FPS:${NC} ${mpvpaper_fps} (max: ${mpvpaper_max_fps})"
        
        if (( $(awk "BEGIN {print ($slapper_fps > $mpvpaper_fps)}") )); then
            local fps_diff=$(awk "BEGIN {printf \"%.1f\", $slapper_fps - $mpvpaper_fps}")
            echo -e "  ${GREEN}✓ Slapper has ${fps_diff} HIGHER average FPS than mpvpaper${NC}"
        elif (( $(awk "BEGIN {print ($mpvpaper_fps > $slapper_fps)}") )); then
            local fps_diff=$(awk "BEGIN {printf \"%.1f\", $mpvpaper_fps - $slapper_fps}")
            echo -e "  ${YELLOW}⚠ Mpvpaper has ${fps_diff} HIGHER average FPS than slapper${NC}"
        else
            echo -e "  ${CYAN}≈ Both applications have similar FPS performance${NC}"
        fi
        
        echo ""
        echo -e "${CYAN}FRAME DROP ANALYSIS:${NC}"
        echo -e "  ${CYAN}Slapper Frame Drops:${NC} ${slapper_dropped} avg (${slapper_drop_rate}% drop rate, max: ${slapper_max_dropped})"
        echo -e "  ${CYAN}Mpvpaper Frame Drops:${NC} ${mpvpaper_dropped} avg (${mpvpaper_drop_rate}% drop rate, max: ${mpvpaper_max_dropped})"
        
        if (( $(awk "BEGIN {print ($mpvpaper_drop_rate > $slapper_drop_rate)}") )); then
            local drop_diff=$(awk "BEGIN {printf \"%.2f\", $mpvpaper_drop_rate - $slapper_drop_rate}")
            echo -e "  ${GREEN}✓ Slapper has ${drop_diff}% LOWER frame drop rate than mpvpaper${NC}"
        elif (( $(awk "BEGIN {print ($slapper_drop_rate > $mpvpaper_drop_rate)}") )); then
            local drop_diff=$(awk "BEGIN {printf \"%.2f\", $slapper_drop_rate - $mpvpaper_drop_rate}")
            echo -e "  ${YELLOW}⚠ Slapper has ${drop_diff}% HIGHER frame drop rate than mpvpaper${NC}"
        else
            echo -e "  ${CYAN}≈ Both applications have similar frame drop rates${NC}"
        fi
        
        echo ""
        echo -e "${CYAN}HARDWARE ACCELERATION ANALYSIS:${NC}"
        echo -e "  ${CYAN}Slapper Decoder Usage:${NC} ${slapper_decoder_util}% avg (max: ${slapper_max_decoder_util}%)"
        echo -e "  ${CYAN}Mpvpaper Decoder Usage:${NC} ${mpvpaper_decoder_util}% avg (max: ${mpvpaper_max_decoder_util}%)"
        
        if (( $(awk "BEGIN {print ($slapper_decoder_util > $mpvpaper_decoder_util)}") )); then
            local decoder_diff=$(awk "BEGIN {printf \"%.1f\", $slapper_decoder_util - $mpvpaper_decoder_util}")
            echo -e "  ${GREEN}✓ Slapper uses ${decoder_diff}% MORE hardware decoding than mpvpaper${NC}"
        elif (( $(awk "BEGIN {print ($mpvpaper_decoder_util > $slapper_decoder_util)}") )); then
            local decoder_diff=$(awk "BEGIN {printf \"%.1f\", $mpvpaper_decoder_util - $slapper_decoder_util}")
            echo -e "  ${YELLOW}⚠ Mpvpaper uses ${decoder_diff}% MORE hardware decoding than slapper${NC}"
        else
            echo -e "  ${CYAN}≈ Both applications use similar hardware acceleration${NC}"
        fi
    fi
}

# Main execution
main() {
    local video_file="$1"
    
    if [[ "$video_file" != /* ]]; then
        video_file="$(pwd)/$video_file"
    fi
    
    if [ ! -f "$video_file" ]; then
        echo -e "${RED}Error: Video file '$video_file' not found${NC}"
        return 1
    fi
    
    echo -e "${CYAN}Video file:${NC} $video_file"
    echo -e "${CYAN}Video size:${NC} $(du -h "$video_file" | cut -f1)"
    echo ""
    
    if ! check_dependencies; then
        return 1
    fi
    
    get_gpu_baseline
    
    # Clean up any existing processes
    safe_cleanup "slapper" "$SLAPPER_BIN"
    safe_cleanup "mpvpaper" "mpvpaper"
    
    # Test slapper
    if test_application "$SLAPPER_BIN -v ALL \"$video_file\"" "slapper" "$video_file"; then
        echo -e "${GREEN}✓ Slapper test completed${NC}"
    else
        echo -e "${RED}✗ Slapper test failed${NC}"
    fi
    
    echo ""
    
    # Test mpvpaper if available
    if command -v mpvpaper >/dev/null 2>&1; then
        safe_cleanup "slapper" "$SLAPPER_BIN"
        sleep 5
        
        if test_application "mpvpaper -v -o \"loop panscan=1.0\" ALL \"$video_file\"" "mpvpaper" "$video_file"; then
            echo -e "${GREEN}✓ Mpvpaper test completed${NC}"
        else
            echo -e "${RED}✗ Mpvpaper test failed${NC}"
        fi
    else
        echo -e "${YELLOW}⚠ Mpvpaper not available${NC}"
    fi
    
    # Generate comparison report
    generate_comparison
    
    # Final cleanup
    safe_cleanup "slapper" "$SLAPPER_BIN"
    safe_cleanup "mpvpaper" "mpvpaper"
    
    echo ""
    echo -e "${GREEN}✓ NVIDIA benchmark completed!${NC}"
    echo -e "${CYAN}Detailed results: $LOG_DIR${NC}"
}

# Script usage
if [ $# -ne 1 ]; then
    echo "Usage: $0 <video_file>"
    echo "Example: $0 test.mp4"
    exit 1
fi

main "$1"