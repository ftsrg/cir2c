#!/usr/bin/env bash
#
# Copyright 2025 Budapest University of Technology and Economics
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

set -euo pipefail

# Build the self-contained cir2c/clang/clang++/cir-opt binaries with Docker
# Buildx (BuildKit) and export them to ./output/.
#
# LLVM is built statically once, in the toolchain base image
# (docker/llvm.Dockerfile, published as ghcr.io/ftsrg/cir2c-llvm); this script
# only compiles cir2c against it, so it takes minutes rather than hours.
#
# Usage: scripts/build-static-binaries.sh [--output DIR]
#
# Environment:
#   DOCKER      docker command to use (default: docker; set to "sudo docker"
#               if your user is not in the docker group)
#   LLVM_IMAGE  toolchain base image (default: the Dockerfile's own default)

REPO_ROOT="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)"
OUTPUT_DIR="$REPO_ROOT/output"
DOCKER=${DOCKER:-docker}

while [[ $# -gt 0 ]]; do
    case "$1" in
        --output) OUTPUT_DIR="$2"; shift 2 ;;
        -h|--help)
            cat <<'EOF'
Usage: scripts/build-static-binaries.sh [--output DIR]

Builds the self-contained cir2c/clang/clang++/cir-opt binaries with Docker
Buildx and exports them to ./output/ (or --output DIR).

Environment:
  DOCKER      docker command to use (default: docker; set to "sudo docker"
              if your user is not in the docker group)
  LLVM_IMAGE  toolchain base image (default: the Dockerfile's own default)
EOF
            exit 0 ;;
        *) echo "Unknown option: $1" >&2; exit 1 ;;
    esac
done

echo "Building static binaries with Docker Buildx…"
echo

if ! $DOCKER --version >/dev/null 2>&1; then
    echo "Error: '$DOCKER' is not usable. Install Docker, or set DOCKER='sudo docker'." >&2
    exit 1
fi

if ! $DOCKER buildx version >/dev/null 2>&1; then
    echo "Error: Docker Buildx is not installed/enabled." >&2
    echo "See https://docs.docker.com/build/buildx/install/" >&2
    exit 1
fi

# Buildx needs an active builder to use the local exporter.
if ! $DOCKER buildx inspect >/dev/null 2>&1; then
    echo "No active buildx builder found. Creating 'cir2c-builder'…"
    $DOCKER buildx create --use --name cir2c-builder >/dev/null
fi

BUILD_ARGS=()
[[ -n "${LLVM_IMAGE:-}" ]] && BUILD_ARGS+=(--build-arg "LLVM_IMAGE=$LLVM_IMAGE")

rm -rf "$OUTPUT_DIR"
$DOCKER buildx build \
    -f "$REPO_ROOT/docker/static-build.Dockerfile" \
    --target=export \
    ${BUILD_ARGS[@]+"${BUILD_ARGS[@]}"} \
    --output "type=local,dest=$OUTPUT_DIR" \
    "$REPO_ROOT"

# BuildKit writes the exported tree as root when docker runs privileged.
if [[ ! -w "$OUTPUT_DIR" ]] || [[ "$(stat -c %u "$OUTPUT_DIR")" != "$(id -u)" ]]; then
    $DOCKER run --rm -v "$OUTPUT_DIR:/out" busybox chown -R "$(id -u):$(id -g)" /out 2>/dev/null \
        || sudo chown -R "$(id -u):$(id -g)" "$OUTPUT_DIR"
fi

if [[ ! -f "$OUTPUT_DIR/clang" || ! -f "$OUTPUT_DIR/cir2c" ]]; then
    echo "Error: expected binaries not found in $OUTPUT_DIR." >&2
    ls -lah "$OUTPUT_DIR" >&2 || true
    exit 1
fi

echo
echo "✓ Build complete — binaries are in $OUTPUT_DIR/"
echo
file "$OUTPUT_DIR/clang" "$OUTPUT_DIR/cir2c" || true
echo
ls -lh "$OUTPUT_DIR/"
