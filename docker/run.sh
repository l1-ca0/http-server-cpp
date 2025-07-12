#!/bin/bash

# Docker management script for Modern C++ HTTP Server
# Usage: ./docker/run.sh [command] [options]

set -e

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

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

show_usage() {
    echo "Docker Management Script for C++ HTTP Server"
    echo ""
    echo "Usage: $0 [command] [options]"
    echo ""
    echo "Commands:"
    echo "  build          Build production Docker image"
    echo "  build-dev      Build development Docker image"
    echo "  run            Run production container"
    echo "  dev            Run development environment"
    echo "  test           Run tests in container"
    echo "  benchmark      Run benchmarks"
    echo "  logs           Show container logs"
    echo "  stop           Stop all containers"
    echo "  clean          Remove containers and images"
    echo "  shell          Open shell in running container"
    echo "  health         Check container health status"
    echo ""
    echo "Options:"
    echo "  --port PORT    Override default port (8080)"
    echo "  --detach       Run container in background"
    echo "  --help         Show this help message"
}

build_production() {
    print_status "Building production Docker image..."
    docker build -t cpp-http-server:latest .
    print_success "Production image built successfully"
}

build_development() {
    print_status "Building development Docker image..."
    docker build -f Dockerfile.dev -t cpp-http-server:dev .
    print_success "Development image built successfully"
}

run_production() {
    local port=${1:-8080}
    local detach_flag=""
    
    if [[ "$2" == "--detach" ]]; then
        detach_flag="-d"
    fi
    
    print_status "Starting production server on port $port..."
    docker run $detach_flag --rm \
        -p $port:8080 \
        -v $(pwd)/config:/opt/http-server/config:ro \
        -v $(pwd)/public:/opt/http-server/public:ro \
        -v $(pwd)/logs:/opt/http-server/logs \
        --name cpp-http-server \
        cpp-http-server:latest
}

run_development() {
    print_status "Starting development environment..."
    docker-compose -f docker-compose.dev.yml up --build
}

run_tests() {
    print_status "Running tests in container..."
    docker run --rm \
        -v $(pwd):/app \
        cpp-http-server:dev \
        ./scripts/build.sh debug --tests
}

run_benchmark() {
    print_status "Running benchmarks..."
    docker-compose -f docker-compose.dev.yml run --rm benchmark
}

show_logs() {
    local container_name=${1:-cpp-http-server}
    docker logs -f $container_name
}

stop_containers() {
    print_status "Stopping all containers..."
    docker-compose down 2>/dev/null || true
    docker-compose -f docker-compose.dev.yml down 2>/dev/null || true
    docker stop cpp-http-server 2>/dev/null || true
    print_success "All containers stopped"
}

clean_containers() {
    print_status "Cleaning up containers and images..."
    stop_containers
    docker system prune -f
    docker rmi cpp-http-server:latest cpp-http-server:dev 2>/dev/null || true
    print_success "Cleanup completed"
}

open_shell() {
    local container_name=${1:-cpp-http-server}
    print_status "Opening shell in container $container_name..."
    docker exec -it $container_name /bin/bash
}

check_health() {
    local container_name=${1:-cpp-http-server}
    print_status "Checking health status..."
    docker inspect --format='{{.State.Health.Status}}' $container_name 2>/dev/null || echo "Container not found or no health check configured"
}

# Parse command line arguments
COMMAND=""
PORT=8080
DETACH=""

while [[ $# -gt 0 ]]; do
    case $1 in
        build)
            COMMAND="build"
            shift
            ;;
        build-dev)
            COMMAND="build-dev"
            shift
            ;;
        run)
            COMMAND="run"
            shift
            ;;
        dev)
            COMMAND="dev"
            shift
            ;;
        test)
            COMMAND="test"
            shift
            ;;
        benchmark)
            COMMAND="benchmark"
            shift
            ;;
        logs)
            COMMAND="logs"
            shift
            ;;
        stop)
            COMMAND="stop"
            shift
            ;;
        clean)
            COMMAND="clean"
            shift
            ;;
        shell)
            COMMAND="shell"
            shift
            ;;
        health)
            COMMAND="health"
            shift
            ;;
        --port)
            PORT="$2"
            shift 2
            ;;
        --detach)
            DETACH="--detach"
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

# Execute command
case $COMMAND in
    build)
        build_production
        ;;
    build-dev)
        build_development
        ;;
    run)
        run_production $PORT $DETACH
        ;;
    dev)
        run_development
        ;;
    test)
        run_tests
        ;;
    benchmark)
        run_benchmark
        ;;
    logs)
        show_logs
        ;;
    stop)
        stop_containers
        ;;
    clean)
        clean_containers
        ;;
    shell)
        open_shell
        ;;
    health)
        check_health
        ;;
    "")
        print_error "No command specified"
        show_usage
        exit 1
        ;;
    *)
        print_error "Unknown command: $COMMAND"
        show_usage
        exit 1
        ;;
esac 