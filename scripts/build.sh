#!/bin/bash

# HTTP Server Build Script
# Usage: ./scripts/build.sh [build_type] [options]
# Build types: debug, release, relwithdebinfo
# Options: --clean, --tests, --install, --help

set -e  # Exit on any error

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

# Default values
BUILD_TYPE="Release"
BUILD_DIR="build"
CLEAN_BUILD=false
BUILD_TESTS=false
INSTALL_BUILD=false
VERBOSE=false

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
    echo "HTTP Server Build Script"
    echo ""
    echo "Usage: $0 [build_type] [options]"
    echo ""
    echo "Build Types:"
    echo "  debug          Build with debug information and optimizations disabled"
    echo "  release        Build optimized for performance (default)"
    echo "  relwithdebinfo Build optimized with debug information"
    echo ""
    echo "Options:"
    echo "  --clean        Clean build directory before building"
    echo "  --tests        Build and run unit tests"
    echo "  --install      Install the built executable"
    echo "  --verbose      Enable verbose output"
    echo "  --help         Show this help message"
    echo ""
    echo "Examples:"
    echo "  $0 debug --clean --tests"
    echo "  $0 release --install"
    echo "  $0 --tests"
}

# Check if required tools are available
check_dependencies() {
    print_status "Checking build dependencies..."
    
    local missing_deps=()
    
    # Check for cmake
    if ! command -v cmake &> /dev/null; then
        missing_deps+=("cmake")
    fi
    
    # Check for C++ compiler
    if ! command -v g++ &> /dev/null && ! command -v clang++ &> /dev/null; then
        missing_deps+=("g++ or clang++")
    fi
    
    # Check for make or ninja
    if ! command -v make &> /dev/null && ! command -v ninja &> /dev/null; then
        missing_deps+=("make or ninja")
    fi
    
    if [ ${#missing_deps[@]} -ne 0 ]; then
        print_error "Missing required dependencies:"
        for dep in "${missing_deps[@]}"; do
            echo "  - $dep"
        done
        echo ""
        echo "Please install the missing dependencies and try again."
        
        # Provide installation hints for common systems
        if command -v apt-get &> /dev/null; then
            echo "On Ubuntu/Debian, try:"
            echo "  sudo apt-get update"
            echo "  sudo apt-get install build-essential cmake libboost-all-dev"
        elif command -v yum &> /dev/null; then
            echo "On CentOS/RHEL, try:"
            echo "  sudo yum groupinstall 'Development Tools'"
            echo "  sudo yum install cmake boost-devel"
        elif command -v brew &> /dev/null; then
            echo "On macOS with Homebrew, try:"
            echo "  brew install cmake boost"
        fi
        
        exit 1
    fi
    
    print_success "All required dependencies found"
}

# Check for Boost libraries
check_boost() {
    print_status "Checking for Boost libraries..."
    
    # Try to find Boost in common locations
    local boost_found=false
    
    if command -v pkg-config &> /dev/null; then
        if pkg-config --exists boost-system 2>/dev/null; then
            boost_found=true
            local boost_version=$(pkg-config --modversion boost-system 2>/dev/null || echo "unknown")
            print_success "Found Boost libraries (version: $boost_version)"
        fi
    fi
    
    if [ "$boost_found" = false ]; then
        # Check common directories
        local boost_dirs=("/usr/include/boost" "/usr/local/include/boost" "/opt/local/include/boost")
        for dir in "${boost_dirs[@]}"; do
            if [ -d "$dir" ]; then
                boost_found=true
                print_success "Found Boost headers in $dir"
                break
            fi
        done
    fi
    
    if [ "$boost_found" = false ]; then
        print_warning "Boost libraries not found in standard locations"
        print_warning "CMake will attempt to download dependencies automatically"
    fi
}

# Parse command line arguments
parse_arguments() {
    while [[ $# -gt 0 ]]; do
        case $1 in
            debug)
                BUILD_TYPE="Debug"
                shift
                ;;
            release)
                BUILD_TYPE="Release"
                shift
                ;;
            relwithdebinfo)
                BUILD_TYPE="RelWithDebInfo"
                shift
                ;;
            --clean)
                CLEAN_BUILD=true
                shift
                ;;
            --tests)
                BUILD_TESTS=true
                shift
                ;;
            --install)
                INSTALL_BUILD=true
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

# Clean build directory
clean_build() {
    if [ "$CLEAN_BUILD" = true ]; then
        print_status "Cleaning build directory..."
        if [ -d "$BUILD_DIR" ]; then
            rm -rf "$BUILD_DIR"
            print_success "Build directory cleaned"
        else
            print_status "Build directory doesn't exist, nothing to clean"
        fi
    fi
}

# Configure the build
configure_build() {
    print_status "Configuring build (type: $BUILD_TYPE)..."
    
    mkdir -p "$BUILD_DIR"
    cd "$BUILD_DIR"
    
    local cmake_args=()
    cmake_args+=("-DCMAKE_BUILD_TYPE=$BUILD_TYPE")
    
    # Enable testing if requested
    if [ "$BUILD_TESTS" = true ]; then
        cmake_args+=("-DBUILD_TESTING=ON")
    fi
    
    # Add verbose flag if requested
    if [ "$VERBOSE" = true ]; then
        cmake_args+=("-DCMAKE_VERBOSE_MAKEFILE=ON")
    fi
    
    # Prefer Ninja if available
    if command -v ninja &> /dev/null; then
        cmake_args+=("-GNinja")
    fi
    
    # Run cmake configuration
    if cmake "${cmake_args[@]}" ..; then
        print_success "Configuration completed"
    else
        print_error "Configuration failed"
        exit 1
    fi
    
    cd ..
}

# Build the project
build_project() {
    print_status "Building project..."
    
    cd "$BUILD_DIR"
    
    local build_args=()
    
    # Determine number of parallel jobs
    local num_jobs
    if command -v nproc &> /dev/null; then
        num_jobs=$(nproc)
    elif [ -r /proc/cpuinfo ]; then
        num_jobs=$(grep -c ^processor /proc/cpuinfo)
    else
        num_jobs=4  # Default fallback
    fi
    
    if command -v ninja &> /dev/null && [ -f "build.ninja" ]; then
        print_status "Using Ninja build system with $num_jobs parallel jobs"
        if ninja; then
            print_success "Build completed successfully"
        else
            print_error "Build failed"
            exit 1
        fi
    else
        print_status "Using Make build system with $num_jobs parallel jobs"
        build_args+=("-j$num_jobs")
        
        if [ "$VERBOSE" = true ]; then
            build_args+=("VERBOSE=1")
        fi
        
        if make "${build_args[@]}"; then
            print_success "Build completed successfully"
        else
            print_error "Build failed"
            exit 1
        fi
    fi
    
    cd ..
}

# Run tests if requested
run_tests() {
    if [ "$BUILD_TESTS" = true ]; then
        print_status "Running unit tests..."
        
        cd "$BUILD_DIR"
        
        if [ -f "test_runner" ]; then
            if ./test_runner; then
                print_success "All tests passed"
            else
                print_error "Some tests failed"
                exit 1
            fi
        else
            print_warning "Test executable not found, skipping tests"
        fi
        
        cd ..
    fi
}

# Install the binary if requested
install_binary() {
    if [ "$INSTALL_BUILD" = true ]; then
        print_status "Installing binary..."
        
        cd "$BUILD_DIR"
        
        if command -v sudo &> /dev/null; then
            sudo make install
        else
            make install
        fi
        
        print_success "Installation completed"
        cd ..
    fi
}

# Display build information
show_build_info() {
    print_status "Build Summary:"
    echo "  Build Type: $BUILD_TYPE"
    echo "  Build Directory: $BUILD_DIR"
    echo "  Tests: $([ "$BUILD_TESTS" = true ] && echo "enabled" || echo "disabled")"
    echo "  Clean Build: $([ "$CLEAN_BUILD" = true ] && echo "yes" || echo "no")"
    
    if [ -f "$BUILD_DIR/http_server" ]; then
        local binary_size=$(du -h "$BUILD_DIR/http_server" | cut -f1)
        echo "  Binary Size: $binary_size"
        
        if command -v file &> /dev/null; then
            local binary_info=$(file "$BUILD_DIR/http_server")
            echo "  Binary Info: $binary_info"
        fi
    fi
}

# Main function
main() {
    echo "HTTP Server Build Script"
    echo "========================"
    echo ""
    
    parse_arguments "$@"
    check_dependencies
    check_boost
    clean_build
    configure_build
    build_project
    run_tests
    install_binary
    
    echo ""
    show_build_info
    echo ""
    print_success "Build process completed successfully!"
    
    if [ -f "$BUILD_DIR/http_server" ]; then
        echo ""
        print_status "To run the server:"
        echo "  ./$BUILD_DIR/http_server"
        echo ""
        print_status "To run with custom configuration:"
        echo "  ./$BUILD_DIR/http_server config/server_config.json"
    fi
}

# Run main function with all arguments
main "$@" 