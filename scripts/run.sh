#!/bin/bash

# HTTP Server Run Script
# Usage: ./scripts/run.sh [options]
# Options: --config, --port, --host, --threads, --verbose, --help

set -e  # Exit on any error

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

# Default values
SERVER_BINARY="./build/http_server"
CONFIG_FILE=""
HOST=""
PORT=""
THREADS=""
DOCUMENT_ROOT=""
VERBOSE=false
DAEMON=false
PID_FILE=""
LOG_FILE=""

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

# Print usage information
show_usage() {
    echo "HTTP Server Run Script"
    echo ""
    echo "Usage: $0 [options]"
    echo ""
    echo "Options:"
    echo "  --config FILE      Use specific configuration file"
    echo "  --host HOST        Override host address (e.g., 0.0.0.0, localhost)"
    echo "  --port PORT        Override port number (e.g., 8080, 3000)"
    echo "  --threads NUM      Override thread pool size"
    echo "  --root DIR         Override document root directory"
    echo "  --daemon           Run server as daemon in background"
    echo "  --pid-file FILE    Write process ID to file (daemon mode)"
    echo "  --log-file FILE    Write logs to file instead of stdout"
    echo "  --verbose          Enable verbose logging"
    echo "  --stop             Stop running daemon server"
    echo "  --status           Show status of running server"
    echo "  --help             Show this help message"
    echo ""
    echo "Examples:"
    echo "  $0                                    # Run with default settings"
    echo "  $0 --config config/prod.json         # Use production config"
    echo "  $0 --port 3000 --host localhost      # Override host and port"
    echo "  $0 --daemon --pid-file server.pid    # Run as daemon"
    echo "  $0 --stop                            # Stop daemon server"
    echo "  $0 --status                          # Check server status"
}

# Parse command line arguments
parse_arguments() {
    while [[ $# -gt 0 ]]; do
        case $1 in
            --config)
                CONFIG_FILE="$2"
                shift 2
                ;;
            --host)
                HOST="$2"
                shift 2
                ;;
            --port)
                PORT="$2"
                shift 2
                ;;
            --threads)
                THREADS="$2"
                shift 2
                ;;
            --root)
                DOCUMENT_ROOT="$2"
                shift 2
                ;;
            --daemon)
                DAEMON=true
                shift
                ;;
            --pid-file)
                PID_FILE="$2"
                shift 2
                ;;
            --log-file)
                LOG_FILE="$2"
                shift 2
                ;;
            --verbose)
                VERBOSE=true
                shift
                ;;
            --stop)
                stop_server
                exit 0
                ;;
            --status)
                show_status
                exit 0
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

# Check if server binary exists
check_binary() {
    if [ ! -f "$SERVER_BINARY" ]; then
        print_error "Server binary not found: $SERVER_BINARY"
        print_status "Please build the server first:"
        echo "  ./scripts/build.sh"
        exit 1
    fi
    
    if [ ! -x "$SERVER_BINARY" ]; then
        print_error "Server binary is not executable: $SERVER_BINARY"
        chmod +x "$SERVER_BINARY"
        print_success "Made server binary executable"
    fi
}

# Create temporary config file with overrides
create_temp_config() {
    if [ -n "$HOST" ] || [ -n "$PORT" ] || [ -n "$THREADS" ] || [ -n "$DOCUMENT_ROOT" ]; then
        local base_config
        
        if [ -n "$CONFIG_FILE" ] && [ -f "$CONFIG_FILE" ]; then
            base_config=$(cat "$CONFIG_FILE")
        else
            # Use default configuration
            base_config=$(cat << 'EOF'
{
  "host": "0.0.0.0",
  "port": 8080,
  "thread_pool_size": 8,
  "document_root": "./public",
  "max_connections": 1000,
  "keep_alive_timeout": 30,
  "max_request_size": 1048576,
  "enable_logging": true,
  "log_file": "",
  "serve_static_files": true,
  "index_files": ["index.html", "index.htm"],
  "mime_types": {
    "html": "text/html; charset=utf-8",
    "css": "text/css",
    "js": "application/javascript",
    "json": "application/json"
  }
}
EOF
)
        fi
        
        # Create temporary config file
        local temp_config="/tmp/http_server_config_$$.json"
        echo "$base_config" > "$temp_config"
        
        # Apply overrides using jq if available, otherwise use sed
        if command -v jq &> /dev/null; then
            local updated_config="$base_config"
            
            if [ -n "$HOST" ]; then
                updated_config=$(echo "$updated_config" | jq --arg host "$HOST" '.host = $host')
            fi
            
            if [ -n "$PORT" ]; then
                updated_config=$(echo "$updated_config" | jq --arg port "$PORT" '.port = ($port | tonumber)')
            fi
            
            if [ -n "$THREADS" ]; then
                updated_config=$(echo "$updated_config" | jq --arg threads "$THREADS" '.thread_pool_size = ($threads | tonumber)')
            fi
            
            if [ -n "$DOCUMENT_ROOT" ]; then
                updated_config=$(echo "$updated_config" | jq --arg root "$DOCUMENT_ROOT" '.document_root = $root')
            fi
            
            echo "$updated_config" > "$temp_config"
        else
            # Fallback to sed-based replacement
            if [ -n "$HOST" ]; then
                sed -i.bak "s/\"host\": \"[^\"]*\"/\"host\": \"$HOST\"/" "$temp_config"
            fi
            
            if [ -n "$PORT" ]; then
                sed -i.bak "s/\"port\": [0-9]*/\"port\": $PORT/" "$temp_config"
            fi
            
            if [ -n "$THREADS" ]; then
                sed -i.bak "s/\"thread_pool_size\": [0-9]*/\"thread_pool_size\": $THREADS/" "$temp_config"
            fi
            
            if [ -n "$DOCUMENT_ROOT" ]; then
                sed -i.bak "s|\"document_root\": \"[^\"]*\"|\"document_root\": \"$DOCUMENT_ROOT\"|" "$temp_config"
            fi
            
            # Clean up backup files
            rm -f "$temp_config.bak"
        fi
        
        CONFIG_FILE="$temp_config"
        
        # Schedule cleanup
        trap "rm -f '$temp_config'" EXIT
    fi
}

# Check if port is available
check_port() {
    local port_to_check="$PORT"
    
    # Extract port from config file if not specified
    if [ -z "$port_to_check" ] && [ -n "$CONFIG_FILE" ]; then
        if command -v jq &> /dev/null; then
            port_to_check=$(jq -r '.port' "$CONFIG_FILE" 2>/dev/null || echo "8080")
        else
            port_to_check=$(grep -o '"port":[[:space:]]*[0-9]*' "$CONFIG_FILE" | grep -o '[0-9]*' || echo "8080")
        fi
    fi
    
    if [ -z "$port_to_check" ]; then
        port_to_check="8080"
    fi
    
    if command -v lsof &> /dev/null; then
        if lsof -i ":$port_to_check" &> /dev/null; then
            print_warning "Port $port_to_check appears to be in use"
            print_status "Checking if it's our server..."
            
            local pid=$(lsof -t -i ":$port_to_check" 2>/dev/null | head -1)
            if [ -n "$pid" ]; then
                local process_name=$(ps -p "$pid" -o comm= 2>/dev/null || echo "unknown")
                print_status "Process using port $port_to_check: $process_name (PID: $pid)"
                
                if [[ "$process_name" == *"http_server"* ]]; then
                    print_warning "Another instance of the HTTP server is already running"
                    print_status "Use --stop to stop the existing server first"
                    return 1
                fi
            fi
        fi
    elif command -v netstat &> /dev/null; then
        if netstat -ln | grep ":$port_to_check " &> /dev/null; then
            print_warning "Port $port_to_check appears to be in use"
        fi
    fi
    
    return 0
}

# Create necessary directories
setup_directories() {
    local doc_root="$DOCUMENT_ROOT"
    
    # Extract document root from config if not specified
    if [ -z "$doc_root" ] && [ -n "$CONFIG_FILE" ]; then
        if command -v jq &> /dev/null; then
            doc_root=$(jq -r '.document_root' "$CONFIG_FILE" 2>/dev/null || echo "./public")
        else
            doc_root=$(grep -o '"document_root":[[:space:]]*"[^"]*"' "$CONFIG_FILE" | sed 's/.*"\([^"]*\)".*/\1/' || echo "./public")
        fi
    fi
    
    if [ -z "$doc_root" ]; then
        doc_root="./public"
    fi
    
    if [ ! -d "$doc_root" ]; then
        print_status "Creating document root directory: $doc_root"
        mkdir -p "$doc_root"
        
        # Create a simple index.html if it doesn't exist
        if [ ! -f "$doc_root/index.html" ]; then
            cat > "$doc_root/index.html" << 'EOF'
<!DOCTYPE html>
<html>
<head>
    <title>C++ HTTP Server</title>
    <style>
        body { font-family: Arial, sans-serif; text-align: center; margin-top: 100px; }
        h1 { color: #333; }
        .status { color: #28a745; font-weight: bold; }
    </style>
</head>
<body>
    <h1>Welcome to C++ HTTP Server!</h1>
    <p class="status">Server is running successfully</p>
    <p>This is the default index page.</p>
</body>
</html>
EOF
            print_success "Created default index.html"
        fi
    fi
}

# Stop running server
stop_server() {
    local pid_to_stop=""
    
    # Try to find PID from file
    if [ -n "$PID_FILE" ] && [ -f "$PID_FILE" ]; then
        pid_to_stop=$(cat "$PID_FILE")
    else
        # Try default PID file
        if [ -f "server.pid" ]; then
            pid_to_stop=$(cat "server.pid")
        fi
    fi
    
    # If no PID file, try to find by process name
    if [ -z "$pid_to_stop" ]; then
        if command -v pgrep &> /dev/null; then
            pid_to_stop=$(pgrep -f "http_server" | head -1)
        fi
    fi
    
    if [ -n "$pid_to_stop" ] && [ "$pid_to_stop" != "0" ]; then
        if kill -0 "$pid_to_stop" 2>/dev/null; then
            print_status "Stopping server (PID: $pid_to_stop)..."
            
            # Try graceful shutdown first
            kill -TERM "$pid_to_stop" 2>/dev/null || true
            
            # Wait a bit for graceful shutdown
            local count=0
            while [ $count -lt 10 ] && kill -0 "$pid_to_stop" 2>/dev/null; do
                sleep 1
                count=$((count + 1))
            done
            
            # Force kill if still running
            if kill -0 "$pid_to_stop" 2>/dev/null; then
                print_warning "Graceful shutdown failed, forcing termination..."
                kill -KILL "$pid_to_stop" 2>/dev/null || true
            fi
            
            print_success "Server stopped"
        else
            print_warning "Server process not found (PID: $pid_to_stop)"
        fi
        
        # Clean up PID file
        if [ -n "$PID_FILE" ] && [ -f "$PID_FILE" ]; then
            rm -f "$PID_FILE"
        fi
        if [ -f "server.pid" ]; then
            rm -f "server.pid"
        fi
    else
        print_warning "No running server found"
    fi
}

# Show server status
show_status() {
    local pid_to_check=""
    
    # Try to find PID from file
    if [ -n "$PID_FILE" ] && [ -f "$PID_FILE" ]; then
        pid_to_check=$(cat "$PID_FILE")
    else
        # Try default PID file
        if [ -f "server.pid" ]; then
            pid_to_check=$(cat "server.pid")
        fi
    fi
    
    if [ -n "$pid_to_check" ] && [ "$pid_to_check" != "0" ]; then
        if kill -0 "$pid_to_check" 2>/dev/null; then
            print_success "Server is running (PID: $pid_to_check)"
            
            # Show additional info if available
            if command -v ps &> /dev/null; then
                local process_info=$(ps -p "$pid_to_check" -o pid,ppid,etime,cmd --no-headers 2>/dev/null || echo "")
                if [ -n "$process_info" ]; then
                    print_status "Process details:"
                    echo "  $process_info"
                fi
            fi
            
            # Try to determine server URL
            local port="8080"  # default
            if [ -f "config/server_config.json" ]; then
                if command -v jq &> /dev/null; then
                    port=$(jq -r '.port' "config/server_config.json" 2>/dev/null || echo "8080")
                fi
            fi
            
            print_status "Server URL: http://localhost:$port"
            
            return 0
        else
            print_warning "PID file exists but process not running (stale PID: $pid_to_check)"
            # Clean up stale PID file
            if [ -n "$PID_FILE" ] && [ -f "$PID_FILE" ]; then
                rm -f "$PID_FILE"
            fi
            if [ -f "server.pid" ]; then
                rm -f "server.pid"
            fi
        fi
    fi
    
    # Try to find by process name
    if command -v pgrep &> /dev/null; then
        local running_pids=$(pgrep -f "http_server" || true)
        if [ -n "$running_pids" ]; then
            print_warning "Found HTTP server processes but no PID file:"
            echo "$running_pids" | while read -r pid; do
                if [ -n "$pid" ]; then
                    local cmd=$(ps -p "$pid" -o cmd --no-headers 2>/dev/null || echo "unknown")
                    echo "  PID $pid: $cmd"
                fi
            done
            return 0
        fi
    fi
    
    print_status "Server is not running"
    return 1
}

# Start the server
start_server() {
    print_status "Starting HTTP server..."
    
    local server_args=()
    
    # Add config file if specified
    if [ -n "$CONFIG_FILE" ]; then
        server_args+=("$CONFIG_FILE")
        print_status "Using configuration file: $CONFIG_FILE"
    fi
    
    # Show configuration summary
    local host_display="0.0.0.0"
    local port_display="8080"
    
    if [ -n "$CONFIG_FILE" ] && [ -f "$CONFIG_FILE" ]; then
        if command -v jq &> /dev/null; then
            host_display=$(jq -r '.host' "$CONFIG_FILE" 2>/dev/null || echo "0.0.0.0")
            port_display=$(jq -r '.port' "$CONFIG_FILE" 2>/dev/null || echo "8080")
        fi
    fi
    
    print_status "Server will start on: http://$host_display:$port_display"
    
    if [ "$DAEMON" = true ]; then
        # Run as daemon
        local log_output="/dev/null"
        if [ -n "$LOG_FILE" ]; then
            log_output="$LOG_FILE"
            # Create log directory if needed
            mkdir -p "$(dirname "$LOG_FILE")"
        fi
        
        local pid_output="server.pid"
        if [ -n "$PID_FILE" ]; then
            pid_output="$PID_FILE"
            # Create PID directory if needed
            mkdir -p "$(dirname "$PID_FILE")"
        fi
        
        # Start server in background
        nohup "$SERVER_BINARY" "${server_args[@]}" > "$log_output" 2>&1 &
        local server_pid=$!
        
        # Write PID to file
        echo "$server_pid" > "$pid_output"
        
        # Give server a moment to start
        sleep 2
        
        # Check if server is still running
        if kill -0 "$server_pid" 2>/dev/null; then
            print_success "Server started as daemon (PID: $server_pid)"
            print_status "PID file: $pid_output"
            if [ -n "$LOG_FILE" ]; then
                print_status "Log file: $LOG_FILE"
            fi
            print_status "Use '$0 --stop' to stop the server"
        else
            print_error "Server failed to start"
            rm -f "$pid_output"
            exit 1
        fi
    else
        # Run in foreground
        print_status "Starting server in foreground mode..."
        print_status "Press Ctrl+C to stop the server"
        echo ""
        
        # Set up signal handler for graceful shutdown
        trap 'echo ""; print_status "Shutting down server..."; exit 0' INT TERM
        
        exec "$SERVER_BINARY" "${server_args[@]}"
    fi
}

# Main function
main() {
    parse_arguments "$@"
    check_binary
    
    if ! check_port; then
        exit 1
    fi
    
    create_temp_config
    setup_directories
    start_server
}

# Run main function with all arguments
main "$@" 