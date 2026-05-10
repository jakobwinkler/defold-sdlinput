#!/bin/bash
# Usage: ./scripts/build-sdl3.sh <platform>
# Platforms: linux, win32, wasm-web
#
# Builds SDL3 as a static library for the given platform inside a Docker container.
# Prerequisites: podman/docker

set -euo pipefail
PLATFORM=$1
SDL_TAG="release-3.4.x"
SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"

# Shared build + extract helper
# Args: image_tag dockerfile_path container_build_path output_path
docker_build_extract() {
  local image_tag=$1 dockerfile=$2 container_path=$3 output_path=$4
  docker build --build-arg SDL_TAG="$SDL_TAG" -t "$image_tag" -f "$dockerfile" "$SCRIPT_DIR"
  local cid
  cid=$(docker create "$image_tag":latest) || { echo "docker create failed"; exit 1; }
  mkdir -p "$(dirname "$output_path")"
  docker cp "$cid:$container_path" "$output_path"
  docker rm "$cid"
  echo "Built $output_path"
}

case $PLATFORM in
  linux)
    OUT_DIR="ext-sdlinput/lib/x86_64-linux"
    docker_build_extract sdl3-builder \
      "$SCRIPT_DIR/Dockerfile.sdl3-build" \
      "/SDL/build/libSDL3.a" \
      "$OUT_DIR/libSDL3.a"
    ;;

  win32)
    OUT_DIR="ext-sdlinput/lib/x86_64-win32"
    docker_build_extract sdl3-win32-builder \
      "$SCRIPT_DIR/Dockerfile.sdl3-win32-build" \
      "/SDL/build/libSDL3.a" \
      "$OUT_DIR/SDL3.lib"
    ;;

  wasm-web|js-web)
    OUT_DIR="ext-sdlinput/lib/wasm-web"
    docker_build_extract sdl3-wasm-builder \
      "$SCRIPT_DIR/Dockerfile.sdl3-wasm-build" \
      "/SDL/build/libSDL3.a" \
      "$OUT_DIR/libSDL3.a"
    ;;

  osx|x86_64-osx)
    echo "Cross-compilation for osx not yet implemented"
    exit 1
    ;;

  arm64-osx)
    echo "Cross-compilation for arm64-osx not yet implemented"
    exit 1
    ;;

  *)
    echo "Unknown platform: $PLATFORM"
    echo "Usage: $0 {linux|win32|wasm-web}"
    exit 1
    ;;
esac
