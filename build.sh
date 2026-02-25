#!/usr/bin/env bash
# ============================================================
#  libnexus - Build Script for Linux / macOS
# ============================================================
set -euo pipefail

# --- Default options ---
BUILD_TYPE="Release"
BUILD_DIR="build"
BUILD_TESTS="OFF"
VCPKG_ROOT="${VCPKG_ROOT:-}"
GENERATOR=""
PARALLEL_JOBS="$(nproc 2>/dev/null || sysctl -n hw.logicalcpu 2>/dev/null || echo 4)"

# --- Helpers ---
info()    { echo -e "\033[1;34m[INFO]\033[0m  $*"; }
success() { echo -e "\033[1;32m[SUCCESS]\033[0m $*"; }
warn()    { echo -e "\033[1;33m[WARNING]\033[0m $*"; }
error()   { echo -e "\033[1;31m[ERROR]\033[0m  $*" >&2; }

show_help() {
    cat <<EOF

 Usage: ./build.sh [OPTIONS]

 Options:
   --debug              Build in Debug mode (default: Release)
   --release            Build in Release mode
   --build-dir <dir>    Output build directory (default: build)
   --with-tests         Enable unit tests (BUILD_TESTS=ON)
   --vcpkg-root <path>  Path to vcpkg root (auto-detected if not set)
   --generator <name>   CMake generator (e.g. "Ninja", "Unix Makefiles")
   --jobs <n>           Parallel build jobs (default: auto)
   --help               Show this help message

 Environment variables:
   VCPKG_ROOT           Path to vcpkg installation (overridden by --vcpkg-root)

EOF
    exit 0
}

# --- Parse arguments ---
while [[ $# -gt 0 ]]; do
    case "$1" in
        --debug)            BUILD_TYPE="Debug" ;;
        --release)          BUILD_TYPE="Release" ;;
        --build-dir)        BUILD_DIR="$2"; shift ;;
        --with-tests)       BUILD_TESTS="ON" ;;
        --vcpkg-root)       VCPKG_ROOT="$2"; shift ;;
        --generator)        GENERATOR="$2"; shift ;;
        --jobs)             PARALLEL_JOBS="$2"; shift ;;
        --help|-h)          show_help ;;
        *) warn "Unknown argument: $1" ;;
    esac
    shift
done

echo ""
echo "============================================================"
echo " libnexus Build Script (Linux/macOS)"
echo " Configuration : $BUILD_TYPE"
echo " Build dir     : $BUILD_DIR"
echo " Tests         : $BUILD_TESTS"
echo " Jobs          : $PARALLEL_JOBS"
echo "============================================================"
echo ""

# --- Detect vcpkg ---
if [[ -z "$VCPKG_ROOT" ]]; then
    for candidate in \
        "$HOME/vcpkg" \
        "/opt/vcpkg" \
        "/usr/local/vcpkg" \
        "$(pwd)/vcpkg"
    do
        if [[ -x "$candidate/vcpkg" ]]; then
            VCPKG_ROOT="$candidate"
            break
        fi
    done
fi

if [[ -n "$VCPKG_ROOT" ]]; then
    VCPKG_TOOLCHAIN="-DCMAKE_TOOLCHAIN_FILE=${VCPKG_ROOT}/scripts/buildsystems/vcpkg.cmake"
    info "Using vcpkg at: $VCPKG_ROOT"
else
    VCPKG_TOOLCHAIN=""
    warn "vcpkg not found. Set --vcpkg-root or VCPKG_ROOT. Proceeding without vcpkg toolchain."
fi

# --- Generator flag ---
GENERATOR_FLAG=()
if [[ -n "$GENERATOR" ]]; then
    GENERATOR_FLAG=(-G "$GENERATOR")
fi

# --- Configure ---
info "Running CMake configure..."
cmake -B "$BUILD_DIR" \
    "${GENERATOR_FLAG[@]}" \
    ${VCPKG_TOOLCHAIN} \
    -DCMAKE_BUILD_TYPE="$BUILD_TYPE" \
    -DBUILD_TESTS="$BUILD_TESTS"

# --- Build ---
echo ""
info "Building with $PARALLEL_JOBS parallel jobs..."
cmake --build "$BUILD_DIR" --config "$BUILD_TYPE" --parallel "$PARALLEL_JOBS"

# --- Tests ---
if [[ "$BUILD_TESTS" == "ON" ]]; then
    echo ""
    info "Running tests..."
    ctest --test-dir "$BUILD_DIR" -C "$BUILD_TYPE" --output-on-failure --parallel "$PARALLEL_JOBS"
fi

echo ""
success "Build completed successfully."
echo "          Binaries are in: $BUILD_DIR/"
echo ""
