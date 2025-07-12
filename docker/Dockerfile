# Multi-stage Dockerfile for C++ HTTP Server
# Stage 1: Build environment
FROM ubuntu:22.04 as builder

# Avoid interactive prompts during package installation
ENV DEBIAN_FRONTEND=noninteractive

# Install build dependencies
RUN apt-get update && apt-get install -y \
    build-essential \
    cmake \
    git \
    libboost-system-dev \
    libboost-filesystem-dev \
    libboost-thread-dev \
    libboost-chrono-dev \
    libboost-date-time-dev \
    libboost-atomic-dev \
    libgtest-dev \
    pkg-config \
    wget \
    curl \
    && rm -rf /var/lib/apt/lists/*

# Set work directory
WORKDIR /app

# Copy project files
COPY . .

# Make build script executable and build the project
RUN chmod +x scripts/build.sh && \
    ./scripts/build.sh release --clean

# Stage 2: Runtime environment
FROM ubuntu:22.04 as runtime

# Install runtime dependencies only
RUN apt-get update && apt-get install -y \
    libboost-system1.74.0 \
    libboost-filesystem1.74.0 \
    libboost-thread1.74.0 \
    libboost-chrono1.74.0 \
    libboost-date-time1.74.0 \
    libboost-atomic1.74.0 \
    && rm -rf /var/lib/apt/lists/*

# Create non-root user for security
RUN groupadd -r httpserver && useradd -r -g httpserver httpserver

# Create application directory
WORKDIR /opt/http-server

# Copy built binary and configuration
COPY --from=builder /app/build/http_server ./
COPY --from=builder /app/config/server_config.json ./config/
COPY --from=builder /app/public ./public

# Create logs directory
RUN mkdir -p logs && chown -R httpserver:httpserver /opt/http-server

# Switch to non-root user
USER httpserver

# Expose HTTP port
EXPOSE 8080

# Health check
HEALTHCHECK --interval=30s --timeout=3s --start-period=5s --retries=3 \
    CMD curl -f http://localhost:8080/api/status || exit 1

# Default command
CMD ["./http_server", "config/server_config.json"] 