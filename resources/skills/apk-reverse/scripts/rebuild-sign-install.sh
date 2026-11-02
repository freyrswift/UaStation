#!/usr/bin/env bash
# rebuild-sign-install.sh — APK repack + sign + install
# Equivalent to the Windows version rebuild-sign-install.ps1
#
# Usage:
#   bash rebuild-sign-install.sh <project_dir> [--out <dir>] [--name <base>]
#                                [--keystore <path>] [--install] [--reinstall]
#                                [--device <serial>] [--clean]

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
KALI_BOOTSTRAP="$(cd "$SCRIPT_DIR/../../../kali/scripts" 2>/dev/null && pwd)/bootstrap-reverse.sh"
DEFAULT_KEYSTORE="$HOME/.android/debug.keystore"

# ─── Arguments ──────────────────────────────────────────────────────────────────

PROJECT_DIR=""
OUT_DIR=""
BASE_NAME=""
KEYSTORE="$DEFAULT_KEYSTORE"
KEY_ALIAS="androiddebugkey"
STORE_PASS="android"
KEY_PASS="android"
DEVICE_SERIAL=""
DO_INSTALL=false
REINSTALL=false
CLEAN=false

while [[ $# -gt 0 ]]; do
    case "$1" in
        --out) OUT_DIR="$2"; shift 2 ;;
        --name) BASE_NAME="$2"; shift 2 ;;
        --keystore) KEYSTORE="$2"; shift 2 ;;
        --alias) KEY_ALIAS="$2"; shift 2 ;;
        --store-pass) STORE_PASS="$2"; shift 2 ;;
        --key-pass) KEY_PASS="$2"; shift 2 ;;
        --device) DEVICE_SERIAL="$2"; shift 2 ;;
        --install) DO_INSTALL=true; shift ;;
        --reinstall) REINSTALL=true; DO_INSTALL=true; shift ;;
        --clean) CLEAN=true; shift ;;
        -*) echo "Unknown option: $1"; exit 1 ;;
        *) PROJECT_DIR="$1"; shift ;;
    esac
done

if [[ -z "$PROJECT_DIR" || ! -d "$PROJECT_DIR" ]]; then
    echo "Usage: $0 <apktool_project_dir> [options]"
    echo "  --out <dir>        Output directory (default: the project's parent directory)"
    echo "  --name <base>      Output filename prefix"
    echo "  --keystore <path>  Signing keystore (default: ~/.android/debug.keystore)"
    echo "  --install          Install to device after signing"
    echo "  --reinstall        Reinstall, overwriting"
    echo "  --device <serial>  Specify a device"
    echo "  --clean            Clean old artifacts"
    exit 1
fi

# ─── Tool detection ──────────────────────────────────────────────────────────────

ensure_tool() {
    local name="$1"
    if command -v "$name" &>/dev/null; then
        return 0
    fi
    echo "INFO: $name not found, attempting auto-install..."
    if [[ -x "$KALI_BOOTSTRAP" ]]; then
        bash "$KALI_BOOTSTRAP" "$name" --skip-refresh 2>/dev/null || true
    fi
    if ! command -v "$name" &>/dev/null; then
        echo "ERR: $name is unavailable."
        case "$name" in
            zipalign|apksigner) echo "  Install: sudo apt install android-sdk-build-tools or sdkmanager 'build-tools;35.0.0'" ;;
            *) echo "  Please install $name manually" ;;
        esac
        exit 1
    fi
}

ensure_tool "apktool"
ensure_tool "zipalign"
ensure_tool "apksigner"
ensure_tool "keytool"
[[ "$DO_INSTALL" == "true" ]] && ensure_tool "adb"

# ─── Generate a debug keystore (if it does not exist) ─────────────────────────────

if [[ ! -f "$KEYSTORE" ]]; then
    echo "INFO: generating debug keystore: $KEYSTORE"
    keytool -genkeypair -v \
        -keystore "$KEYSTORE" \
        -storepass "$STORE_PASS" \
        -keypass "$KEY_PASS" \
        -alias "$KEY_ALIAS" \
        -keyalg RSA -keysize 2048 -validity 10000 \
        -dname "CN=Android Debug,O=ReverseSkill,C=CN"
fi

# ─── Path computation ────────────────────────────────────────────────────────────

OUT_DIR="${OUT_DIR:-$(dirname "$PROJECT_DIR")}"
BASE_NAME="${BASE_NAME:-$(basename "$PROJECT_DIR")}"
mkdir -p "$OUT_DIR"

UNSIGNED_APK="$OUT_DIR/${BASE_NAME}-unsigned.apk"
ALIGNED_APK="$OUT_DIR/${BASE_NAME}-aligned.apk"
SIGNED_APK="$OUT_DIR/${BASE_NAME}-signed.apk"

if [[ "$CLEAN" == "true" ]]; then
    rm -f "$UNSIGNED_APK" "$ALIGNED_APK" "$SIGNED_APK"
fi

# ─── Repack ───────────────────────────────────────────────────────────────────────

echo "=== apktool repack ==="
apktool b "$PROJECT_DIR" -o "$UNSIGNED_APK"

# ─── Alignment ───────────────────────────────────────────────────────────────────

echo "=== zipalign alignment ==="
zipalign -f -p 4 "$UNSIGNED_APK" "$ALIGNED_APK"

# ─── Signing ─────────────────────────────────────────────────────────────────────

echo "=== apksigner signing ==="
apksigner sign \
    --ks "$KEYSTORE" \
    --ks-key-alias "$KEY_ALIAS" \
    --ks-pass "pass:$STORE_PASS" \
    --key-pass "pass:$KEY_PASS" \
    --out "$SIGNED_APK" \
    "$ALIGNED_APK"

# ─── Verification ─────────────────────────────────────────────────────────────────

echo "=== Verify signature ==="
apksigner verify --print-certs "$SIGNED_APK"

echo ""
echo "═══════════════════════════════════════════"
echo "  APK repack complete"
echo "═══════════════════════════════════════════"
echo "  unsigned_apk=$UNSIGNED_APK"
echo "  aligned_apk=$ALIGNED_APK"
echo "  signed_apk=$SIGNED_APK"
echo "  keystore=$KEYSTORE"

# ─── Installation ─────────────────────────────────────────────────────────────────

if [[ "$DO_INSTALL" == "true" ]]; then
    echo "=== adb install ==="
    ADB_ARGS=()
    [[ -n "$DEVICE_SERIAL" ]] && ADB_ARGS+=("-s" "$DEVICE_SERIAL")
    ADB_ARGS+=("install")
    [[ "$REINSTALL" == "true" ]] && ADB_ARGS+=("-r")
    ADB_ARGS+=("$SIGNED_APK")

    adb "${ADB_ARGS[@]}"
    echo "  install_device=${DEVICE_SERIAL:-default}"
fi
