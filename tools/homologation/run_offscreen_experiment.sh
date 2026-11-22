#!/usr/bin/env bash
# Executor experimental controlado de cenário único (Etapa 03.2.4.6.5).
# Executa APENAS UM cenário autorizado explicitamente por invocação.
# NÃO executar cenários C..H sem autorização humana explícita.
set -uo pipefail

if [[ $EUID -eq 0 ]]; then
    echo "[ABORT] Execução proibida como root." >&2
    exit 1
fi

SCENARIO=""
FRAMES="1"
AUTHORIZED="0"
BUILD_DIR="${SYSTALIA_HARNESS_BUILD:-$HOME/.cache/sge-build-homolog}"

while [[ $# -gt 0 ]]; do
    case "$1" in
        --scenario)
            SCENARIO="${2:-}"
            shift 2
            ;;
        --frames)
            FRAMES="${2:-1}"
            shift 2
            ;;
        --authorize)
            AUTHORIZED="1"
            shift
            ;;
        *)
            echo "[ABORT] Argumento desconhecido: $1" >&2
            exit 2
            ;;
    esac
done

SCENARIO="$(echo "$SCENARIO" | tr '[:lower:]' '[:upper:]')"
case "$SCENARIO" in
    A|B|C|D|E|F|G|H) ;;
    *)
        echo "[ABORT] Uso: $0 --scenario <A..H> [--frames N] [--authorize]" >&2
        exit 2
        ;;
esac

HARNESS_BIN="$BUILD_DIR/sge_vulkan_dmabuf_harness"
QPA_DIR="$BUILD_DIR/platforms"
if [[ ! -x "$HARNESS_BIN" || ! -f "$QPA_DIR/libsge_vulkan_offscreen.so" ]]; then
    echo "[ABORT] Harness não encontrado em $BUILD_DIR." >&2
    exit 1
fi

if [[ "$SCENARIO" =~ ^[C-H]$ && "$AUTHORIZED" != "1" ]]; then
    echo "[BLOCKED] Cenário $SCENARIO requer --authorize explícito após aprovação humana." >&2
    exit 3
fi

LOG_DIR="${SYSTALIA_HOMOLOG_LOGS:-$HOME/systalia-homolog-logs}"
mkdir -p "$LOG_DIR"
STAMP="$(date +%Y%m%d-%H%M%S)"
LOG_FILE="$LOG_DIR/offscreen-exp-${SCENARIO}-${STAMP}.log"
COMMIT_SHA="$(git rev-parse --short HEAD 2>/dev/null || echo unknown)"

echo "=== SYSTALIA Experimento Offscreen Controlado ===" | tee "$LOG_FILE"
echo "Data: $(date -Iseconds)" | tee -a "$LOG_FILE"
echo "Commit: $COMMIT_SHA | Cenário: $SCENARIO | Frames: $FRAMES" | tee -a "$LOG_FILE"

export QT_QPA_PLATFORM=sge_vulkan_offscreen
export QT_QPA_PLATFORM_PLUGIN_PATH="$QPA_DIR"
export SGE_GRAPHICS_DIAGNOSTICS=1

EXTRA_ARGS=()
if [[ "$SCENARIO" =~ ^[C-H]$ ]]; then
    export SGE_ALLOW_HIGH_RISK_GPU_SCENARIO=1
    EXTRA_ARGS+=(--allow-gpu-risk)
fi

"$HARNESS_BIN" --scenario "$SCENARIO" --frames "$FRAMES" "${EXTRA_ARGS[@]}" >>"$LOG_FILE" 2>&1
RC=$?

echo "=== Fim do experimento (rc=$RC, $(date -Iseconds)) ===" | tee -a "$LOG_FILE"
echo "Log salvo em: $LOG_FILE"
exit "$RC"
