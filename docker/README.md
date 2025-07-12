# Docker Setup

Docker configuration and utilities for the C++ HTTP Server.

## Files

- `run.sh` - Docker management script
- `nginx.conf` - Nginx reverse proxy configuration  
- `prometheus.yml` - Prometheus monitoring configuration

## Quick Start

### Production

```bash
# Start production stack
docker-compose up -d

# Test server
curl http://localhost/api/status
```

### Development

```bash
# Start development environment
./docker/run.sh dev

# Run tests
./docker/run.sh test
```

## Management Script

The `run.sh` script provides commands for Docker operations:

```bash
./docker/run.sh [command] [options]
```

**Commands:**
- `build` - Build production image
- `dev` - Start development environment
- `test` - Run tests in container
- `benchmark` - Run performance benchmarks
- `logs` - Show container logs
- `stop` - Stop all containers
- `clean` - Remove containers and images

**Options:**
- `--port PORT` - Override default port
- `--detach` - Run in background

## Architecture

**Production Stack:**
- `http-server` - Main C++ application
- `nginx` - Reverse proxy with rate limiting
- `prometheus` - Metrics collection

**Development Stack:**
- `http-server-dev` - Development server with debugging
- `static-analysis` - Code quality checks
- `test-runner` - Automated testing

## Configuration

### Environment Variables

| Variable | Default | Description |
|----------|---------|-------------|
| `HTTP_SERVER_HOST` | `0.0.0.0` | Bind address |
| `HTTP_SERVER_PORT` | `8080` | Server port |
| `HTTP_SERVER_THREADS` | `8` | Thread pool size |

### Custom Configuration

```bash
# Deploy with custom config
docker run -d \
  -p 8080:8080 \
  -v $(pwd)/config/custom.json:/opt/http-server/config/server_config.json:ro \
  cpp-http-server:latest
```

## Monitoring

- Prometheus UI: http://localhost:9090
- Server metrics: http://localhost:8080/api/status

---
