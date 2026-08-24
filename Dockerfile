# ==============================================================================
# Multi-Stage Dockerfile for Safe Semantic Planner (Render & Cloud Deployment)
# ==============================================================================

# Stage 1: Build Stage with full C++17 Toolchain
FROM alpine:3.19 AS builder

RUN apk add --no-cache g++ clang make cmake git libstdc++

WORKDIR /app
COPY . .

# Compile high-performance server binary with -O3 optimization
RUN make bin/ssp_server

# Stage 2: Ultra-lightweight Production Runtime
FROM alpine:3.19

RUN apk add --no-cache libstdc++ libgcc

WORKDIR /app

# Copy compiled binary and required web/data assets
COPY --from=builder /app/bin/ssp_server ./bin/ssp_server
COPY --from=builder /app/web ./web
COPY --from=builder /app/data ./data
COPY --from=builder /app/config.json ./config.json

# Default environment variables for Render
ENV PORT=8080
ENV HOST=0.0.0.0

EXPOSE 8080

CMD ["./bin/ssp_server"]
