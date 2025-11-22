# ==========================================
# Stage 1: Build
# ==========================================
FROM debian:bookworm-slim AS builder

# Install build dependencies
# libssl-dev is required by libdatachannel
# nlohmann-json3-dev provides the JSON headers and CMake config
RUN apt-get update && apt-get install -y \
    build-essential \
    cmake \
    git \
    libssl-dev \
    nlohmann-json3-dev \
    pkg-config

WORKDIR /app

# Copy the source code
# Note: We assume libdatachannel is populated (see instructions below)
COPY . .

# Create build directory
WORKDIR /app/build

# Configure and Build
# Release mode is highly recommended for an SFU (performance)
RUN cmake -DCMAKE_BUILD_TYPE=Release ..  -DBUILD_SHARED_LIBS=OFF && \
    make -j$(nproc)

# ==========================================
# Stage 2: Runtime
# ==========================================
FROM debian:bookworm-slim AS runner

# Install runtime dependencies
# We only need the runtime SSL libraries, not the dev headers
RUN apt-get update && apt-get install -y \
    ca-certificates \
    libssl3 \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /app

# Copy the binary from the builder stage
COPY --from=builder /app/build/sfu_server .

# (Optional) Expose ports
# Replace 8000 with your signaling port
# WebRTC (UDP) ports usually need a range, e.g., 50000-60000
EXPOSE 8000

# Run the server
ENTRYPOINT ["./sfu_server"]