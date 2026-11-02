#!/bin/bash
# Clean build verification of public GitHub source. Never flashes the device.
set -euo pipefail

NCS="/opt/nordic/ncs/v3.0.2"
TOOLCHAIN="/opt/nordic/ncs/toolchains/ef4fc6722e"
REPO="XIAO-nRF54L15-Matter-Vibration-Sensor"
ROOT="$HOME/Documents/XIAO/$REPO-clean-$(date +%Y%m%d-%H%M%S)"
WEST="$TOOLCHAIN/bin/west"

for file in "$WEST" "$TOOLCHAIN/opt/zephyr-sdk/arm-zephyr-eabi/bin/arm-zephyr-eabi-gcc"; do
    if [[ ! -x "$file" ]]; then
        echo "Missing required executable: $file" >&2
        exit 1
    fi
done
if [[ ! -d "$NCS/zephyr" || -e "$ROOT" ]]; then
    echo "Missing NCS checkout or verification directory already exists" >&2
    exit 1
fi

mkdir -p "$ROOT"
echo "Retrieving a fresh public snapshot; no GitHub login required..."
curl --fail --location --silent --show-error \
  "https://github.com/blb4github/$REPO/archive/refs/heads/main.zip" \
  --output "$ROOT/source.zip"
ditto -x -k "$ROOT/source.zip" "$ROOT"

APP="$ROOT/$REPO-main"
BUILD="$ROOT/build"
LOG="$ROOT/build.log"
for file in CMakeLists.txt Kconfig.sysbuild sysbuild_internal.conf prj.conf pm_static.yml; do
    [[ -f "$APP/$file" ]] || { echo "Missing $APP/$file" >&2; exit 1; }
done
[[ -f "$APP/board-root/boards/seeed/xiao_nrf54l15/board.yml" ]] || {
    echo "Custom board definition missing" >&2
    exit 1
}

export PATH="$TOOLCHAIN/bin:$PATH"
export ZEPHYR_TOOLCHAIN_VARIANT=zephyr
export ZEPHYR_SDK_INSTALL_DIR="$TOOLCHAIN/opt/zephyr-sdk"
unset GNUARMEMB_TOOLCHAIN_PATH || true

{
    echo "=== Independent clean build of GitHub source ==="
    date
    echo "NCS: $NCS"
    echo "Source: $APP"
    echo "Board: xiao_nrf54l15/nrf54l15/cpuapp"
    echo "Build profile: internal"
    "$ZEPHYR_SDK_INSTALL_DIR/arm-zephyr-eabi/bin/arm-zephyr-eabi-gcc" --version | head -1
} | tee "$LOG"

cd "$NCS"
"$WEST" build \
    -b xiao_nrf54l15/nrf54l15/cpuapp \
    --sysbuild \
    --pristine=always \
    --build-dir "$BUILD" \
    "$APP" \
    -- \
    -DBOARD_ROOT="$APP/board-root" \
    -DFILE_SUFFIX=internal 2>&1 | tee -a "$LOG"

if [[ ! -f "$BUILD/merged.hex" ]]; then
    echo "Build did not produce merged.hex" | tee -a "$LOG"
    exit 1
fi

{
    echo "=== CLEAN BUILD SUCCESS ==="
    echo "Generated: $BUILD/merged.hex"
    shasum -a 256 "$BUILD/merged.hex"
    echo "Original prototype SHA256 (a different hash is not automatically an error):"
    echo "17a7dae001e7a008205ec06e5f712771bbc5107c8a71ca221ceb4092e89c05f9"
    echo "=== Kconfig values ==="
    find "$BUILD" -path '*/zephyr/.config' -type f -print | while IFS= read -r cfg; do
        echo "-- $cfg"
        grep -E '^(CONFIG_CHIP_ENABLE_ICD_SUPPORT|CONFIG_OPENTHREAD_MTD_SED|CONFIG_OPENTHREAD_MTD|CONFIG_CHIP_ICD_SLOW_POLL_INTERVAL|CONFIG_CHIP_FACTORY_DATA|CONFIG_CHIP_FACTORY_DATA_BUILD|CONFIG_PM_PARTITION_SIZE_SETTINGS_STORAGE)=' "$cfg" || true
    done
    echo "DO NOT publish or flash this HEX: it may include device-specific Matter credentials."
    echo "No device was modified."
    echo "Build log: $LOG"
} | tee -a "$LOG"
