#!/bin/bash
# Skred Release Script - Linux & Windows Only

VERSION="1.2.0"
TARGETS=("x86_64-linux" "x86_64-windows")

echo "--- Cleaning old builds ---"
rm -rf zig-out
rm -rf .zig-cache

for TARGET in "${TARGETS[@]}"; do
    echo "--- Building for $TARGET ---"
    
    if [[ "$TARGET" == "x86_64-linux" ]]; then
        # NATIVE BUILD: This is the ONLY way it finds your local libasound
        echo "Running native Linux build (finding system ALSA)..."
        zig build zip -Dversion=$VERSION
    else
        # WINDOWS CROSS BUILD: Zig handles this natively without extra SDKs
        echo "Cross-compiling for $TARGET..."
        zig build zip -Dtarget=$TARGET -Dversion=$VERSION
    fi
    
    if [ $? -eq 0 ]; then
        echo "Successfully created zip for $TARGET"
    else
        echo "Failed to build for $TARGET"
        exit 1
    fi
done

echo "--- All releases completed ---"
ls -lh zig-out/*.zip
