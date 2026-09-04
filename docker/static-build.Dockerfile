# Build cir2c against the static LLVM already installed in the base image
# (/opt/cir) and export self-contained binaries. The base image
# (docker/llvm.Dockerfile) builds LLVM as static archives, so clang, cir-opt,
# clang++ and cir2c are all statically linked against LLVM out of the box —
# there is no LLVM rebuild here, which makes this image cheap to produce.
ARG LLVM_IMAGE=ghcr.io/ftsrg/cir2c-llvm:latest
FROM ${LLVM_IMAGE} AS builder

ARG DEBIAN_FRONTEND=noninteractive

WORKDIR /app

COPY CMakeLists.txt /app/CMakeLists.txt
COPY cmake /app/cmake
COPY src /app/src
# .git is only read by cmake/GenerateVersion.cmake, to stamp the binary with the
# commit it was built from.
COPY .git /app/.git

# cir2c links against the static LLVM/MLIR/CIR archives under /opt/cir.
# -static-libgcc/-static-libstdc++ removes the C++ runtime dynamic dependency, so
# the resulting binary only depends on libc.
RUN cmake -S /app -B /app/build \
        -DCMAKE_BUILD_TYPE=Release \
        -DCMAKE_C_COMPILER=clang \
        -DCMAKE_CXX_COMPILER=clang++ \
        -DCMAKE_EXE_LINKER_FLAGS="-static-libgcc -static-libstdc++" \
        -DCIR2C_LLVM_PREFIX=/opt/cir && \
    cmake --build /app/build --target cir2c -j$(nproc)

# Stage all exported artifacts under /export/ so the scratch stage can use stable
# COPY paths.  clang is shipped as the versioned binary (clang-NN) resolved to
# /export/clang; the Clang resource headers travel alongside it under include/.
# libc++/libc++abi/libunwind (built into the base image's /opt/cir alongside
# clang) travel under libcxx/, the same way: so a caller of these binaries who
# wants -stdlib=libc++ (e.g. to avoid the out-of-line-libstdc++ limitation
# documented for cir2c) has the headers+static libs available without needing
# to build libc++ themselves.
RUN mkdir -p /export && \
    CLANG_BIN="$(find /opt/cir/bin -maxdepth 1 -type f \( -name 'clang-[0-9]*' -o -name clang \) | sort -V | tail -n 1)" && \
    cp -L "$CLANG_BIN"          /export/clang && \
    cp -L /opt/cir/bin/cir-opt  /export/cir-opt && \
    cp -L /opt/cir/bin/clang++  /export/clang++ && \
    cp -rL /opt/cir/lib/clang   /export/clang-resource && \
    cp /app/build/cir2c         /export/cir2c && \
    cp /opt/cir/LICENSE.LLVM    /export/LICENSE.LLVM && \
    cp /opt/cir/llvm-version.txt /export/llvm-version.txt && \
    mkdir -p /export/libcxx/include /export/libcxx/lib && \
    cp -rL /opt/cir/include/c++         /export/libcxx/include/c++ && \
    cp -L /opt/cir/include/unwind.h     /export/libcxx/include/ && \
    cp -L /opt/cir/include/__libunwind_config.h /export/libcxx/include/ && \
    cp -L /opt/cir/lib/libc++.a         /export/libcxx/lib/ && \
    cp -L /opt/cir/lib/libc++abi.a      /export/libcxx/lib/ && \
    cp -L /opt/cir/lib/libunwind.a      /export/libcxx/lib/ && \
    strip /export/clang /export/cir2c

RUN echo "=== clang ===" && /export/clang --version && \
    printf "\n=== cir2c ===\n" && /export/cir2c --version

# Export only the (statically linked) binaries.
FROM scratch AS export
COPY --from=builder /export/clang            /clang
COPY --from=builder /export/cir-opt          /cir-opt
COPY --from=builder /export/clang++          /clang++
COPY --from=builder /export/clang-resource   /include
COPY --from=builder /export/libcxx           /libcxx
COPY --from=builder /export/cir2c            /cir2c
COPY --from=builder /export/LICENSE.LLVM     /LICENSE.LLVM
COPY --from=builder /export/llvm-version.txt /llvm-version.txt

# Runtime stage for a quick smoke-test.
FROM ${LLVM_IMAGE} AS runtime
COPY --from=builder /export/clang  /usr/local/bin/clang
COPY --from=builder /export/cir2c  /usr/local/bin/cir2c
RUN clang --version && cir2c --version
CMD ["/bin/bash"]
