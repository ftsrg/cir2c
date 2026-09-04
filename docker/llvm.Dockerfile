# Toolchain base image: LLVM+Clang+MLIR with ClangIR, plus libc++/libc++abi/
# libunwind, all built as static archives and installed into /opt/cir.
#
# The LLVM revision is the released tag pinned in llvm-version.txt — see
# docs/llvm-toolchain.md. Published as ghcr.io/ftsrg/cir2c-llvm, rebuilt only
# when that pin moves, so day-to-day cir2c builds never rebuild LLVM.
FROM debian:trixie-slim

ARG DEBIAN_FRONTEND=noninteractive
ENV LC_ALL=C.UTF-8

RUN apt-get update \
 && apt-get install -y --no-install-recommends \
    ca-certificates \
    git \
    cmake \
    ninja-build \
    build-essential \
    pkg-config \
    python3 \
    clang \
    lld \
    wget \
    curl \
    zlib1g-dev \
    libstdc++-13-dev \
 && rm -rf /var/lib/apt/lists/*

WORKDIR /build

# The actual build (LLVM+Clang+MLIR with ClangIR, then libc++/libc++abi/libunwind,
# all as static archives into /opt/cir) lives in build-llvm.sh so it can also be
# run directly on a host machine without Docker — see docs/building.md.
# --clean removes the (multi-GB) source checkout and build directories within
# this same RUN/layer, so they never end up baked into the image.
COPY docker/build-llvm.sh /build/docker/build-llvm.sh
COPY llvm-version.txt /build/llvm-version.txt
RUN chmod +x /build/docker/build-llvm.sh && \
    /build/docker/build-llvm.sh --src /build/llvm-project --prefix /opt/cir --clean && \
    cp /build/llvm-version.txt /opt/cir/llvm-version.txt && \
    rm -rf /build/llvm-project /build/docker /build/llvm-version.txt

ENV PATH="/opt/cir/bin:${PATH}"

LABEL org.opencontainers.image.title="cir2c-llvm" \
      org.opencontainers.image.description="Base image with LLVM+Clang (CLANG_ENABLE_CIR) and libc++/libc++abi/libunwind, all built as static archives and installed to /opt/cir" \
      org.opencontainers.image.source="https://github.com/ftsrg/cir2c"

CMD ["/bin/sh","-c","echo 'This image only provides static LLVM in /opt/cir' && /bin/sh"]
