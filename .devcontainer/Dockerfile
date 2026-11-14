FROM mcr.microsoft.com/devcontainers/base:ubuntu26.04

ENV DEBIAN_FRONTEND=noninteractive

RUN mkdir -p /home/vscode/.vscode-server/data/User /home/vscode/.copilot /workspaces/godot-vgmstream/build \
    && chown -R vscode:vscode /home/vscode/.vscode-server /home/vscode/.copilot /workspaces/godot-vgmstream/build

RUN apt-get update && apt-get install -y \
    build-essential \
    scons \
    pkg-config \
    libx11-dev \
    libxcursor-dev \
    libxinerama-dev \
    libgl1-mesa-dev \
    libglu1-mesa-dev \
    libasound2-dev \
    libpulse-dev \
    libudev-dev \
    libxi-dev \
    libxrandr-dev \
    libwayland-dev \
    clang-format \
    && apt-get clean \
    && rm -rf /var/lib/apt/lists/*
