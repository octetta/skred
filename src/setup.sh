#!/bin/bash

# Setup script for miniaudio + Emscripten project

echo "Setting up miniaudio Emscripten project..."

# Download miniaudio header if it doesn't exist
if [ ! -f "miniaudio.h" ]; then
    echo "Downloading miniaudio.h..."
    curl -o miniaudio.h https://raw.githubusercontent.com/mackron/miniaudio/master/miniaudio.h
    if [ $? -eq 0 ]; then
        echo "✓ miniaudio.h downloaded successfully"
    else
        echo "✗ Failed to download miniaudio.h"
        echo "Please download it manually from https://github.com/mackron/miniaudio"
        exit 1
    fi
else
    echo "✓ miniaudio.h already exists"
fi

# Check if Emscripten is installed
if command -v emcc >/dev/null 2>&1; then
    echo "✓ Emscripten found: $(emcc --version | head -n1)"
else
    echo "✗ Emscripten not found!"
    echo "Please install Emscripten from https://emscripten.org/docs/getting_started/downloads.html"
    exit 1
fi

# Build the project
echo "Building project..."
make -f Makefile.emaud clean
make -f Makefile.emaud

if [ $? -eq 0 ]; then
    echo "✓ Build successful!"
    echo ""
    echo "To run the example:"
    echo "  1. Start a local server: make serve"
    echo "  2. Open http://localhost:8000 in your browser"
    echo "  3. Click 'Start Audio' to begin audio generation"
    echo ""
    echo "Note: Most browsers require user interaction before playing audio,"
    echo "so make sure to click the start button after the page loads."
else
    echo "✗ Build failed!"
    exit 1
fi
