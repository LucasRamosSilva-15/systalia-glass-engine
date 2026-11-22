#!/usr/bin/env bash
# Script de execução física assistida EXCLUSIVO para TTY3 (Etapa 03.2.4.6).
# NÃO executar dentro do KDE Plasma (TTY2) nem como root.
set -uo pipefail

HOMOLOG_DIR="${SYSTALIA_HOMOLOG_DIR:-$HOME/.local/systalia-homolog}"
LOG_DIR="${SYSTALIA_HOMOLOG_LOGS:-$HOME/systalia-homolog-logs}"
LOCK_FILE="${XDG_RUNTIME_DIR:-/tmp}/sge-homolog-${UID}.lock"
ENGINE_BIN="$HOMOLOG_DIR/bin/systalia_glass_engine"
QPA_DIR="$HOMOLOG_DIR/bin/platforms"

if [[ $EUID -eq 0 ]]; then
    echo "[ABORT] Execução proibida como root." >&2
    exit 1
fi

if [[ -n "${WAYLAND_DISPLAY:-}" || -n "${DISPLAY:-}" ]]; then
    echo "[ABORT] Sessão gráfica ativa detectada (WAYLAND_DISPLAY=${WAYLAND_DISPLAY:-}, DISPLAY=${DISPLAY:-})." >&2
    echo "Execute exclusivamente a partir de um console TTY puro (ex.: TTY3)." >&2
    exit 1
fi

if [[ ! -x "$ENGINE_BIN" || ! -f "$QPA_DIR/libsge_vulkan_offscreen.so" ]]; then
    echo "[ABORT] Instalação de homologação ausente em $HOMOLOG_DIR." >&2
    exit 1
fi

# Verifica sessão systemd-logind ativa em TTY (recusa TTY2 / sessão gráfica principal)
SESSION_ID="${XDG_SESSION_ID:-}"
if [[ -z "$SESSION_ID" ]]; then
    SESSION_ID="$(loginctl list-sessions --no-legend | awk -v u="$USER" '$3==u && $5=="user" && $6 ~ /^tty/ {print $1; exit}')"
fi
if [[ -z "$SESSION_ID" ]]; then
    echo "[ABORT] Nenhuma sessão systemd-logind TTY válida encontrada." >&2
    exit 1
fi

SESSION_TTY="$(loginctl show-session "$SESSION_ID" -p TTY --value 2>/dev/null || true)"
SESSION_TYPE="$(loginctl show-session "$SESSION_ID" -p Type --value 2>/dev/null || true)"
SESSION_STATE="$(loginctl show-session "$SESSION_ID" -p State --value 2>/dev/null || true)"

if [[ "$SESSION_TTY" == "tty2" || "${XDG_VTNR:-}" == "2" ]]; then
    echo "[ABORT] Execução bloqueada na TTY2 (sessão gráfica principal KDE Plasma)." >&2
    exit 1
fi
if [[ "$SESSION_TYPE" != "tty" || "$SESSION_STATE" != "active" ]]; then
    echo "[ABORT] Sessão logind ($SESSION_ID) inválida para bare-metal: Type=$SESSION_TYPE State=$SESSION_STATE TTY=$SESSION_TTY." >&2
    exit 1
fi

mkdir -p "$LOG_DIR"
exec 9>"$LOCK_FILE"
if ! flock -n 9; then
    echo "[ABORT] Já existe uma instância de homologação em execução ($LOCK_FILE)." >&2
    exit 1
fi

TIMESTAMP="$(date +%Y%m%d-%H%M%S)"
LOG_FILE="$LOG_DIR/tty3-homolog-${TIMESTAMP}.log"

echo "=== SYSTALIA Homologação Física TTY3 ===" | tee "$LOG_FILE"
echo "Data: $(date -Iseconds)" | tee -a "$LOG_FILE"
echo "Sessão: id=$SESSION_ID tty=$SESSION_TTY seat=$(loginctl show-session "$SESSION_ID" -p Seat --value 2>/dev/null || true)" | tee -a "$LOG_FILE"
echo "Binário: $ENGINE_BIN" | tee -a "$LOG_FILE"
echo "Log: $LOG_FILE" | tee -a "$LOG_FILE"

export SGE_USE_DRM_BACKEND=1
export QT_QPA_PLATFORM=sge_vulkan_offscreen
export QT_QPA_PLATFORM_PLUGIN_PATH="$QPA_DIR"
export QSG_RHI_BACKEND=vulkan
export QT_LOGGING_RULES="qt.qpa.*=true;sge.*=true"

"$ENGINE_BIN" >>"$LOG_FILE" 2>&1
RC=$?

echo "=== Encerrado com código de saída: $RC ($(date -Iseconds)) ===" | tee -a "$LOG_FILE"
exit "$RC"
