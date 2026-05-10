#!/bin/bash
# Usage: ./scripts/build-sdl3.sh <platform> [--docker]
# Platforms: linux, wasm-web
#
# Builds SDL3 as a static library for the given platform.
# --docker: build inside container (Ubuntu 20.04 for linux, emsdk for wasm-web)
#
# Prerequisites:
#   Native: cmake, git, build toolchain
#   Docker: podman/docker

set -euo pipefail
PLATFORM=$1
USE_DOCKER=0
if [ "${2:-}" = "--docker" ]; then
  USE_DOCKER=1
fi

SDL_TAG="release-3.2.x"
SDL_DIR="/tmp/SDL-$PLATFORM"
OUT_DIR="ext-sdlinput/lib/$PLATFORM"
SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"

CMAKE_ARGS=(
  -DBUILD_SHARED_LIBS=OFF
  -DSDL_STATIC=ON
  -DSDL_STATIC_PIC=ON
  -DSDL_UNIX_CONSOLE_BUILD=ON

  # Enabled subsystems
  -DSDL_JOYSTICK=ON
  -DSDL_HIDAPI=ON
  -DSDL_SENSOR=ON
  -DSDL_HAPTIC=ON
  -DSDL_POWER=ON

  # Disabled subsystems
  -DSDL_VIDEO=OFF
  -DSDL_AUDIO=OFF
  -DSDL_RENDER=OFF
  -DSDL_GPU=OFF
  -DSDL_CAMERA=OFF
  -DSDL_DIALOG=OFF
  -DSDL_TRAY=OFF
  -DSDL_PROCESS=OFF
  -DSDL_STORAGE=OFF

  # Video platform backends
  -DSDL_X11=OFF
  -DSDL_WAYLAND=OFF
  -DSDL_OPENGL=OFF
  -DSDL_VULKAN=OFF
  -DSDL_METAL=OFF
  -DSDL_COCOA=OFF
  -DSDL_DIRECTX=OFF

  # Audio backends
  -DSDL_ALSA=OFF
  -DSDL_PULSEAUDIO=OFF
  -DSDL_PIPEWIRE=OFF
  -DSDL_JACK=OFF
  -DSDL_SNDIO=OFF
  -DSDL_DISKAUDIO=OFF
  -DSDL_DUMMYAUDIO=OFF

  # Other features
  -DSDL_IBUS=OFF
  -DSDL_FCITX=OFF
  -DSDL_LIBURING=OFF
  -DSDL_VIRTUAL_JOYSTICK=OFF
  -DSDL_LIBUDEV=OFF
  -DSDL_DBUS=OFF
  -DSDL_HIDAPI_LIBUSB=OFF
)

docker_build() {
  echo "Building SDL3 inside Ubuntu 20.04 container..."
  docker build -t sdl3-builder -f "$SCRIPT_DIR/Dockerfile.sdl3-build" "$SCRIPT_DIR"
  local cid
  cid=$(docker create sdl3-builder:latest) || { echo "docker create failed"; exit 1; }
  mkdir -p "$OUT_DIR"
  docker cp "$cid:/SDL/build/libSDL3.a" "$OUT_DIR/"
  docker rm "$cid"
  echo "Built $OUT_DIR/libSDL3.a"
}

clone_sdl() {
  if [ ! -d "$SDL_DIR" ]; then
    git clone --depth 1 --branch "$SDL_TAG" https://github.com/libsdl-org/SDL "$SDL_DIR"
  fi
}

case $PLATFORM in
  linux)
    if [ "$USE_DOCKER" -eq 1 ]; then
      docker_build
      exit 0
    fi

    clone_sdl
    cd "$SDL_DIR"

    cmake -B build "${CMAKE_ARGS[@]}"
    cmake --build build -j"$(nproc)"
    mkdir -p "$OUT_DIR"
    cp build/libSDL3.a "$OUT_DIR/"
    echo "Built $OUT_DIR/libSDL3.a"
    ;;

  win32)
    echo "Cross-compilation for win32 not yet implemented"
    exit 1
    ;;

  osx|x86_64-osx)
    echo "Cross-compilation for osx not yet implemented"
    exit 1
    ;;

  arm64-osx)
    echo "Cross-compilation for arm64-osx not yet implemented"
    exit 1
    ;;

  wasm-web|js-web)
    if [ "$USE_DOCKER" -ne 1 ]; then
      echo "wasm-web build requires --docker flag"
      exit 1
    fi
    echo "Building SDL3 for WASM inside emscripten/emsdk container..."
    docker build -t sdl3-wasm-builder -f "$SCRIPT_DIR/Dockerfile.sdl3-wasm-build" "$SCRIPT_DIR"
    cid=$(docker create sdl3-wasm-builder:latest) || { echo "docker create failed"; exit 1; }
    mkdir -p "$OUT_DIR"
    docker cp "$cid:/SDL/build/libSDL3.a" "$OUT_DIR/"
    docker rm "$cid"
    echo "Built $OUT_DIR/libSDL3.a"
    ;;

  *)
    echo "Unknown platform: $PLATFORM"
    echo "Usage: $0 {linux|wasm-web} [--docker]"
    exit 1
    ;;
esac
