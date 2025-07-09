#!/bin/bash

# HTTP Server Benchmark Script
# Usage: ./scripts/benchmark.sh [options]
# Tools: wrk, ab, curl, custom tests

set -e  # Exit on any error

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
CYAN='\033[0;36m'
NC='\033[0m' # No Color

# Default values
SERVER_URL="http://localhost:8080"
BENCHMARK_TOOL=""
DURATION="30s"
CONNECTIONS="100"
THREADS="10"
REQUESTS="10000"
OUTPUT_FILE=""
VERBOSE=false
AUTO_START_SERVER=true
SERVER_PID=""
RESULTS_DIR="benchmark_results"

# Print colored output
print_status() {
    echo -e "${BLUE}[INFO]${NC} $1"
}

print_success() {
    echo -e "${GREEN}[SUCCESS]${NC} $1"
}

print_warning() {
    echo -e "${YELLOW}[WARNING]${NC} $1"
}

print_error() {
    echo -e "${RED}[ERROR]${NC} $1"
}

print_result() {
    echo -e "${CYAN}[RESULT]${NC} $1"
}

# Print usage information
show_usage() {
    echo "HTTP Server Benchmark Script"
    echo ""
    echo "Usage: $0 [options]"
    echo ""
    echo "Options:"
    echo "  --url URL          Server URL to benchmark (default: http://localhost:8080)"
    echo "  --tool TOOL        Benchmark tool: wrk, ab, curl, custom, all (default: auto-detect)"
    echo "  --duration TIME    Test duration (e.g., 30s, 5m) - for wrk"
    echo "  --connections NUM  Number of concurrent connections (default: 100)"
    echo "  --threads NUM      Number of threads (default: 10)"
    echo "  --requests NUM     Total number of requests (default: 10000) - for ab"
    echo "  --output FILE      Save results to file"
    echo "  --no-server        Don't auto-start server"
    echo "  --verbose          Enable verbose output"
    echo "  --help             Show this help message"
    echo ""
    echo "Tools:"
    echo "  wrk                Modern HTTP benchmarking tool"
    echo "  ab                 Apache Bench (traditional)"
    echo "  curl               Simple curl-based test"
    echo "  custom             Custom test scenarios"
    echo "  all                Run all available tools"
    echo ""
    echo "Examples:"
    echo "  $0                                    # Auto-detect and run benchmark"
    echo "  $0 --tool wrk --duration 60s         # Use wrk for 60 seconds"
    echo "  $0 --tool ab --requests 50000        # Use ab with 50k requests"
    echo "  $0 --url http://example.com:8080     # Benchmark remote server"
    echo "  $0 --tool all --output results.txt   # Run all tools, save results"
}

# Parse command line arguments
parse_arguments() {
    while [[ $# -gt 0 ]]; do
        case $1 in
            --url)
                SERVER_URL="$2"
                shift 2
                ;;
            --tool)
                BENCHMARK_TOOL="$2"
                shift 2
                ;;
            --duration)
                DURATION="$2"
                shift 2
                ;;
            --connections)
                CONNECTIONS="$2"
                shift 2
                ;;
            --threads)
                THREADS="$2"
                shift 2
                ;;
            --requests)
                REQUESTS="$2"
                shift 2
                ;;
            --output)
                OUTPUT_FILE="$2"
                shift 2
                ;;
            --no-server)
                AUTO_START_SERVER=false
                shift
                ;;
            --verbose)
                VERBOSE=true
                shift
                ;;
            --help|-h)
                show_usage
                exit 0
                ;;
            *)
                print_error "Unknown option: $1"
                show_usage
                exit 1
                ;;
        esac
    done
}

# Check if tools are available
check_tools() {
    local available_tools=()
    
    if command -v wrk &> /dev/null; then
        available_tools+=("wrk")
    fi
    
    if command -v ab &> /dev/null; then
        available_tools+=("ab")
    fi
    
    if command -v curl &> /dev/null; then
        available_tools+=("curl")
    fi
    
    if [ ${#available_tools[@]} -eq 0 ]; then
        print_error "No benchmark tools found!"
        echo ""
        echo "Please install one of the following:"
        echo "  - wrk: https://github.com/wg/wrk"
        echo "  - ab (Apache Bench): sudo apt-get install apache2-utils"
        echo "  - curl: sudo apt-get install curl"
        echo ""
        
        # Provide installation hints
        if command -v apt-get &> /dev/null; then
            echo "On Ubuntu/Debian:"
            echo "  sudo apt-get install apache2-utils curl"
            echo "  # For wrk, you may need to build from source"
        elif command -v yum &> /dev/null; then
            echo "On CentOS/RHEL:"
            echo "  sudo yum install httpd-tools curl"
        elif command -v brew &> /dev/null; then
            echo "On macOS:"
            echo "  brew install wrk curl"
        fi
        
        exit 1
    fi
    
    print_success "Available tools: ${available_tools[*]}"
    
    # Auto-select tool if not specified
    if [ -z "$BENCHMARK_TOOL" ]; then
        if [[ " ${available_tools[*]} " =~ " wrk " ]]; then
            BENCHMARK_TOOL="wrk"
            print_status "Auto-selected: wrk"
        elif [[ " ${available_tools[*]} " =~ " ab " ]]; then
            BENCHMARK_TOOL="ab"
            print_status "Auto-selected: ab"
        else
            BENCHMARK_TOOL="curl"
            print_status "Auto-selected: curl"
        fi
    fi
    
    # Validate selected tool
    if [ "$BENCHMARK_TOOL" != "all" ] && [ "$BENCHMARK_TOOL" != "custom" ]; then
        if ! command -v "$BENCHMARK_TOOL" &> /dev/null; then
            print_error "Selected tool '$BENCHMARK_TOOL' is not available"
            exit 1
        fi
    fi
}

# Start server if needed
start_server() {
    if [ "$AUTO_START_SERVER" = false ]; then
        return 0
    fi
    
    # Check if server is already running
    if curl -s --max-time 5 "$SERVER_URL" &> /dev/null; then
        print_status "Server is already running at $SERVER_URL"
        return 0
    fi
    
    print_status "Starting server for benchmarking..."
    
    # Check if build exists
    if [ ! -f "./build/http_server" ]; then
        print_error "Server binary not found. Building server..."
        if ! ./scripts/build.sh release; then
            print_error "Failed to build server"
            exit 1
        fi
    fi
    
    # Start server in background
    ./scripts/run.sh --daemon --pid-file benchmark_server.pid --log-file benchmark_server.log &
    
    # Wait for server to start
    local count=0
    while [ $count -lt 30 ]; do
        if curl -s --max-time 2 "$SERVER_URL" &> /dev/null; then
            print_success "Server started successfully"
            SERVER_PID=$(cat benchmark_server.pid 2>/dev/null || echo "")
            return 0
        fi
        sleep 1
        count=$((count + 1))
    done
    
    print_error "Failed to start server"
    exit 1
}

# Stop server if we started it
stop_server() {
    if [ -n "$SERVER_PID" ]; then
        print_status "Stopping benchmark server..."
        ./scripts/run.sh --stop || true
        rm -f benchmark_server.pid benchmark_server.log
    fi
}

# Setup trap to cleanup on exit
setup_cleanup() {
    trap 'stop_server' EXIT INT TERM
}

# Create results directory
setup_results() {
    if [ -n "$OUTPUT_FILE" ] || [ "$BENCHMARK_TOOL" = "all" ]; then
        mkdir -p "$RESULTS_DIR"
        
        local timestamp=$(date +"%Y%m%d_%H%M%S")
        if [ -z "$OUTPUT_FILE" ]; then
            OUTPUT_FILE="$RESULTS_DIR/benchmark_$timestamp.txt"
        fi
        
        print_status "Results will be saved to: $OUTPUT_FILE"
    fi
}

# Get server info
get_server_info() {
    print_status "Server Information:"
    echo "  URL: $SERVER_URL"
    
    # Try to get server stats
    if curl -s --max-time 5 "$SERVER_URL/api/status" &> /dev/null; then
        local stats=$(curl -s --max-time 5 "$SERVER_URL/api/status" 2>/dev/null || echo "{}")
        
        if command -v jq &> /dev/null; then
            local uptime=$(echo "$stats" | jq -r '.uptime_seconds // "unknown"' 2>/dev/null || echo "unknown")
            local total_conn=$(echo "$stats" | jq -r '.total_connections // "unknown"' 2>/dev/null || echo "unknown")
            
            echo "  Uptime: ${uptime}s"
            echo "  Total connections: $total_conn"
        fi
    fi
    
    # Get server version info with curl
    local server_header=$(curl -s -I --max-time 5 "$SERVER_URL" 2>/dev/null | grep -i "server:" || echo "Server: unknown")
    echo "  $server_header"
    
    echo ""
}

# Run wrk benchmark
run_wrk_benchmark() {
    print_status "Running wrk benchmark..."
    print_status "Duration: $DURATION, Connections: $CONNECTIONS, Threads: $THREADS"
    
    local wrk_args=("-t$THREADS" "-c$CONNECTIONS" "-d$DURATION")
    
    if [ "$VERBOSE" = true ]; then
        wrk_args+=("--latency")
    fi
    
    wrk_args+=("$SERVER_URL")
    
    local result_file=""
    if [ -n "$OUTPUT_FILE" ]; then
        result_file="/tmp/wrk_result_$$.txt"
        wrk "${wrk_args[@]}" > "$result_file" 2>&1
        
        echo "=== WRK Benchmark Results ===" >> "$OUTPUT_FILE"
        echo "URL: $SERVER_URL" >> "$OUTPUT_FILE"
        echo "Duration: $DURATION, Connections: $CONNECTIONS, Threads: $THREADS" >> "$OUTPUT_FILE"
        echo "Timestamp: $(date)" >> "$OUTPUT_FILE"
        echo "" >> "$OUTPUT_FILE"
        cat "$result_file" >> "$OUTPUT_FILE"
        echo "" >> "$OUTPUT_FILE"
        
        # Also display to console
        cat "$result_file"
        rm -f "$result_file"
    else
        wrk "${wrk_args[@]}"
    fi
}

# Run ab benchmark
run_ab_benchmark() {
    print_status "Running Apache Bench (ab)..."
    print_status "Requests: $REQUESTS, Concurrency: $CONNECTIONS"
    
    local ab_args=("-n$REQUESTS" "-c$CONNECTIONS")
    
    if [ "$VERBOSE" = true ]; then
        ab_args+=("-v" "2")
    fi
    
    ab_args+=("$SERVER_URL/")
    
    local result_file=""
    if [ -n "$OUTPUT_FILE" ]; then
        result_file="/tmp/ab_result_$$.txt"
        ab "${ab_args[@]}" > "$result_file" 2>&1
        
        echo "=== Apache Bench (ab) Results ===" >> "$OUTPUT_FILE"
        echo "URL: $SERVER_URL" >> "$OUTPUT_FILE"
        echo "Requests: $REQUESTS, Concurrency: $CONNECTIONS" >> "$OUTPUT_FILE"
        echo "Timestamp: $(date)" >> "$OUTPUT_FILE"
        echo "" >> "$OUTPUT_FILE"
        cat "$result_file" >> "$OUTPUT_FILE"
        echo "" >> "$OUTPUT_FILE"
        
        # Also display to console
        cat "$result_file"
        rm -f "$result_file"
    else
        ab "${ab_args[@]}"
    fi
}

# Run curl benchmark (simple)
run_curl_benchmark() {
    print_status "Running curl-based benchmark..."
    print_status "Requests: $REQUESTS (sequential)"
    
    local start_time=$(date +%s)
    local success_count=0
    local error_count=0
    local total_time=0
    
    for ((i=1; i<=REQUESTS; i++)); do
        local request_start=$(date +%s%3N)  # milliseconds
        
        if curl -s --max-time 10 "$SERVER_URL" &> /dev/null; then
            success_count=$((success_count + 1))
        else
            error_count=$((error_count + 1))
        fi
        
        local request_end=$(date +%s%3N)
        local request_time=$((request_end - request_start))
        total_time=$((total_time + request_time))
        
        # Show progress every 1000 requests
        if [ $((i % 1000)) -eq 0 ]; then
            echo "  Progress: $i/$REQUESTS requests completed"
        fi
    done
    
    local end_time=$(date +%s)
    local duration=$((end_time - start_time))
    local avg_time=$((total_time / REQUESTS))
    local rps=$(echo "scale=2; $success_count / $duration" | bc -l 2>/dev/null || echo "N/A")
    
    local results="CURL Benchmark Results:
  Total requests: $REQUESTS
  Successful requests: $success_count
  Failed requests: $error_count
  Total time: ${duration}s
  Average request time: ${avg_time}ms
  Requests per second: $rps"
    
    echo "$results"
    
    if [ -n "$OUTPUT_FILE" ]; then
        echo "=== CURL Benchmark Results ===" >> "$OUTPUT_FILE"
        echo "URL: $SERVER_URL" >> "$OUTPUT_FILE"
        echo "Timestamp: $(date)" >> "$OUTPUT_FILE"
        echo "" >> "$OUTPUT_FILE"
        echo "$results" >> "$OUTPUT_FILE"
        echo "" >> "$OUTPUT_FILE"
    fi
}

# Run custom benchmark scenarios
run_custom_benchmark() {
    print_status "Running custom benchmark scenarios..."
    
    # Test different endpoints
    local endpoints=("/" "/hello" "/api/status" "/dashboard" "/nonexistent")
    local endpoint_results=()
    
    for endpoint in "${endpoints[@]}"; do
        print_status "Testing endpoint: $endpoint"
        
        local url="$SERVER_URL$endpoint"
        local start_time=$(date +%s%3N)
        local status_code=""
        
        if [ "$endpoint" = "/nonexistent" ]; then
            # Expect 404 for this one
            status_code=$(curl -s -o /dev/null -w "%{http_code}" --max-time 5 "$url" || echo "000")
        else
            status_code=$(curl -s -o /dev/null -w "%{http_code}" --max-time 5 "$url" || echo "000")
        fi
        
        local end_time=$(date +%s%3N)
        local response_time=$((end_time - start_time))
        
        endpoint_results+=("$endpoint: ${status_code} (${response_time}ms)")
        echo "  Result: $endpoint -> $status_code (${response_time}ms)"
    done
    
    # Test concurrent requests
    print_status "Testing concurrent requests..."
    local concurrent_start=$(date +%s)
    
    for ((i=1; i<=10; i++)); do
        curl -s --max-time 5 "$SERVER_URL" &> /dev/null &
    done
    wait
    
    local concurrent_end=$(date +%s)
    local concurrent_time=$((concurrent_end - concurrent_start))
    
    print_result "10 concurrent requests completed in ${concurrent_time}s"
    
    # Save results
    if [ -n "$OUTPUT_FILE" ]; then
        echo "=== Custom Benchmark Results ===" >> "$OUTPUT_FILE"
        echo "URL: $SERVER_URL" >> "$OUTPUT_FILE"
        echo "Timestamp: $(date)" >> "$OUTPUT_FILE"
        echo "" >> "$OUTPUT_FILE"
        echo "Endpoint Tests:" >> "$OUTPUT_FILE"
        for result in "${endpoint_results[@]}"; do
            echo "  $result" >> "$OUTPUT_FILE"
        done
        echo "" >> "$OUTPUT_FILE"
        echo "Concurrent Test: 10 requests in ${concurrent_time}s" >> "$OUTPUT_FILE"
        echo "" >> "$OUTPUT_FILE"
    fi
}

# Run all benchmarks
run_all_benchmarks() {
    print_status "Running all available benchmarks..."
    
    if command -v wrk &> /dev/null; then
        echo ""
        print_status "=== Running WRK Benchmark ==="
        run_wrk_benchmark
    fi
    
    if command -v ab &> /dev/null; then
        echo ""
        print_status "=== Running Apache Bench ==="
        run_ab_benchmark
    fi
    
    if command -v curl &> /dev/null; then
        echo ""
        print_status "=== Running CURL Benchmark ==="
        run_curl_benchmark
    fi
    
    echo ""
    print_status "=== Running Custom Tests ==="
    run_custom_benchmark
}

# Main benchmark function
run_benchmark() {
    get_server_info
    
    case "$BENCHMARK_TOOL" in
        wrk)
            run_wrk_benchmark
            ;;
        ab)
            run_ab_benchmark
            ;;
        curl)
            run_curl_benchmark
            ;;
        custom)
            run_custom_benchmark
            ;;
        all)
            run_all_benchmarks
            ;;
        *)
            print_error "Unknown benchmark tool: $BENCHMARK_TOOL"
            exit 1
            ;;
    esac
}

# Display summary
show_summary() {
    echo ""
    print_success "Benchmark completed!"
    
    if [ -n "$OUTPUT_FILE" ] && [ -f "$OUTPUT_FILE" ]; then
        print_status "Results saved to: $OUTPUT_FILE"
        echo ""
        print_status "Summary:"
        echo "  Server URL: $SERVER_URL"
        echo "  Benchmark tool: $BENCHMARK_TOOL"
        echo "  Output file: $OUTPUT_FILE"
        
        # Show file size
        if command -v wc &> /dev/null; then
            local line_count=$(wc -l < "$OUTPUT_FILE")
            echo "  Report lines: $line_count"
        fi
    fi
}

# Main function
main() {
    echo "HTTP Server Benchmark Script"
    echo "============================="
    echo ""
    
    parse_arguments "$@"
    check_tools
    setup_cleanup
    setup_results
    start_server
    
    echo ""
    run_benchmark
    
    show_summary
}

# Run main function with all arguments
main "$@" 