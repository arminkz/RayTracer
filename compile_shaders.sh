#!/bin/bash

# This script compiles GLSL shaders to SPIR-V using glslangValidator

# Colors
RED='\033[0;31m'
GREEN='\033[0;32m'
BLUE='\033[0;34m'
CYAN='\033[0;36m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

# Path to glslangValidator (make sure it's in your PATH or set full path here)
GLSLANG_PATH="glslangValidator"

# Path to shader source folder
SHADER_FOLDER="shaders"

# Output folder
OUTPUT_FOLDER="assets/spv"

# Create output folder if it doesn't exist
mkdir -p "$OUTPUT_FOLDER"

# Resolve full paths (macOS compatible)
if command -v realpath &> /dev/null; then
    SHADER_FOLDER_FULL=$(realpath "$SHADER_FOLDER")
    OUTPUT_FOLDER_FULL=$(realpath "$OUTPUT_FOLDER")
else
    # Fallback for macOS without realpath
    SHADER_FOLDER_FULL=$(cd "$SHADER_FOLDER" && pwd)
    OUTPUT_FOLDER_FULL=$(cd "$OUTPUT_FOLDER" && pwd)
fi

# Find all shader files
find "$SHADER_FOLDER_FULL" -type f \( -name "*.rchit" -o -name "*.rgen" -o -name "*.rmiss" -o -name "*.rahit" \) | while read -r INPUT_FILE; do
    # Get relative path
    RELATIVE_PATH="${INPUT_FILE#$SHADER_FOLDER_FULL/}"
    RELATIVE_DIR=$(dirname "$RELATIVE_PATH")
    TARGET_DIR="$OUTPUT_FOLDER_FULL/$RELATIVE_DIR"
    mkdir -p "$TARGET_DIR"

    # Get base filename
    BASENAME=$(basename "$INPUT_FILE")
    EXT="${BASENAME##*.}"
    NAME="${BASENAME%.*}"

    OUTPUT_FILE="$TARGET_DIR/${NAME}_${EXT}.spv"

    echo -e "${BLUE}Compiling ${YELLOW}$RELATIVE_PATH${NC} -> ${GREEN}${OUTPUT_FILE#$OUTPUT_FOLDER_FULL/}${NC}..."

    "$GLSLANG_PATH" -V "$INPUT_FILE" -o "$OUTPUT_FILE" --target-env vulkan1.2
    if [ $? -ne 0 ]; then
        echo -e "${RED}Failed to compile ${RELATIVE_PATH}${NC}" >&2
    else
        echo -e "${GREEN}Successfully compiled ${RELATIVE_PATH}${NC}"
    fi
done

echo -e "${CYAN}Done.${NC}"