#!/usr/bin/env bash
# Script de execução física assistida EXCLUSIVO para TTY3 (Etapa 03.2.4.6.18.1).
# NÃO executar dentro do KDE Plasma (TTY2), via SSH, em outro TTY nem como root.
set -uo pipefail

HOMOLOG_DIR="${SYSTALIA_HOMOLOG_DIR:-$HOME/.local/systalia-homolog}"
LOG_DIR="${SYSTALIA_HOMOLOG_LOGS:-$HOME/systalia-homolog-logs}"
LOCK_FILE="${XDG_RUNTIME_DIR:-/tmp}/sge-homolog-${UID}.lock"
ENGINE_BIN="$HOMOLOG_DIR/bin/systalia_glass_engine"
QPA_DIR="$HOMOLOG_DIR/bin/platforms"
IDENTITY_FILE="$HOMOLOG_DIR/share/systalia/BUILD_IDENTITY.txt"

if [[ $EUID -eq 0 ]]; then
    echo "[ABORT] Execução proibida como root." >&2
    exit 1
fi

if [[ -n "${SSH_CONNECTION:-}" || -n "${SSH_CLIENT:-}" || -n "${SSH_TTY:-}" ]]; then
    echo "[ABORT] Execução via SSH proibida." >&2
    exit 1
fi

if [[ -n "${WAYLAND_DISPLAY:-}" || -n "${DISPLAY:-}" || -n "${MIR_SOCKET:-}" ]]; then
    echo "[ABORT] Sessão gráfica ativa detectada (WAYLAND_DISPLAY=${WAYLAND_DISPLAY:-}, DISPLAY=${DISPLAY:-})." >&2
    echo "Execute exclusivamente a partir do console TTY3 puro." >&2
    exit 1
fi

CTRL_TTY="$(tty 2>/dev/null || true)"
if [[ "$CTRL_TTY" != "/dev/tty3" ]]; then
    echo "[ABORT] Terminal controlador inválido: '${CTRL_TTY:-none}' (esperado /dev/tty3)." >&2
    exit 1
fi

if [[ "${XDG_VTNR:-}" != "3" ]]; then
    echo "[ABORT] XDG_VTNR inválido: '${XDG_VTNR:-unset}' (esperado 3)." >&2
    exit 1
fi

SESSION_ID="${XDG_SESSION_ID:-}"
if [[ -z "$SESSION_ID" ]]; then
    echo "[ABORT] XDG_SESSION_ID ausente; seleção automática via list-sessions é proibida." >&2
    exit 1
fi

SESSION_TTY="$(loginctl show-session "$SESSION_ID" -p TTY --value 2>/dev/null || true)"
SESSION_TYPE="$(loginctl show-session "$SESSION_ID" -p Type --value 2>/dev/null || true)"
SESSION_STATE="$(loginctl show-session "$SESSION_ID" -p State --value 2>/dev/null || true)"
SESSION_ACTIVE="$(loginctl show-session "$SESSION_ID" -p Active --value 2>/dev/null || true)"
SESSION_VTNR="$(loginctl show-session "$SESSION_ID" -p VTNr --value 2>/dev/null || true)"

if [[ "$SESSION_TTY" != "tty3" || "$SESSION_TYPE" != "tty" || "$SESSION_STATE" != "active" || "$SESSION_ACTIVE" != "yes" || "$SESSION_VTNR" != "3" ]]; then
    echo "[ABORT] Sessão logind ($SESSION_ID) inconsistente ou inativa para TTY3: TTY='$SESSION_TTY' Type='$SESSION_TYPE' State='$SESSION_STATE' Active='$SESSION_ACTIVE' VTNr='$SESSION_VTNR'." >&2
    exit 1
fi

if [[ ! -x "$ENGINE_BIN" || ! -f "$QPA_DIR/libsge_vulkan_offscreen.so" ]]; then
    echo "[ABORT] Instalação de homologação ausente ou incompleta em $HOMOLOG_DIR." >&2
    exit 1
fi

if [[ ! -s "$IDENTITY_FILE" ]]; then
    echo "[ABORT] Identidade do build ausente ou vazia: $IDENTITY_FILE." >&2
    exit 1
fi

if ! grep -q '^branch=valo-graphics-2\.0$' "$IDENTITY_FILE" \
    || ! grep -q '^build_type=Release$' "$IDENTITY_FILE" \
    || ! grep -Eq '^base_commit=[0-9a-f]{7,}$' "$IDENTITY_FILE"; then
    echo "[ABORT] Identidade do build inválida ou incompatível em $IDENTITY_FILE." >&2
    exit 1
fi

if ! grep -aq "SGE_HOMOLOG_MAX_FRAMES" "$ENGINE_BIN"; then
    echo "[ABORT] Mecanismo SGE_HOMOLOG_MAX_FRAMES ausente no binário instalado ($ENGINE_BIN)." >&2
    exit 1
fi

if ! mkdir -p "$LOG_DIR"; then
    echo "[ABORT] Falha ao criar diretório de logs: $LOG_DIR." >&2
    exit 1
fi

exec 9>"$LOCK_FILE"
if ! flock -n 9; then
    echo "[ABORT] Já existe uma instância de homologação em execução ($LOCK_FILE)." >&2
    exit 1
fi

TIMESTAMP="$(date +%Y%m%d-%H%M%S)"
LOG_FILE="$LOG_DIR/tty3-homolog-${TIMESTAMP}.log"
KERNEL_LOG_FILE="$LOG_DIR/tty3-kernel-${TIMESTAMP}.log"
KERNEL_WATCH_PID=""

cleanup_monitors() {
    if [[ -n "$KERNEL_WATCH_PID" ]]; then
        kill "$KERNEL_WATCH_PID" 2>/dev/null || true
        wait "$KERNEL_WATCH_PID" 2>/dev/null || true
    fi
    sync "$LOG_FILE" "$KERNEL_LOG_FILE" 2>/dev/null || true
}
trap cleanup_monitors EXIT

{
    echo "=== SYSTALIA Homologação Física TTY3 ==="
    echo "Data: $(date -Iseconds)"
    echo "Sessão: id=$SESSION_ID tty=$SESSION_TTY vtnr=$SESSION_VTNR ctrl_tty=$CTRL_TTY seat=$(loginctl show-session "$SESSION_ID" -p Seat --value 2>/dev/null || true)"
    echo "Binário: $ENGINE_BIN"
    echo "Log: $LOG_FILE"
    cat "$IDENTITY_FILE"
} | tee "$LOG_FILE"

# Força estritamente 1 frame e diagnósticos completos, ignorando valores herdados.
export SGE_USE_DRM_BACKEND=1
export QT_QPA_PLATFORM=sge_vulkan_offscreen
export QT_QPA_PLATFORM_PLUGIN_PATH="$QPA_DIR"
export QSG_RHI_BACKEND=vulkan
export SGE_HOMOLOG_MAX_FRAMES=1
export SGE_GRAPHICS_DIAGNOSTICS=1
export QT_LOGGING_RULES="qt.qpa.*=true;sge.*=true"

echo "Forçado: SGE_HOMOLOG_MAX_FRAMES=$SGE_HOMOLOG_MAX_FRAMES SGE_GRAPHICS_DIAGNOSTICS=$SGE_GRAPHICS_DIAGNOSTICS" | tee -a "$LOG_FILE"

if [[ "${SGE_ENABLE_KERNEL_WATCH:-0}" == "1" ]] && command -v journalctl >/dev/null 2>&1; then
    # Monitoramento externo opcional; não garante recuperação nem persistência total em GPU hang severo.
    journalctl -k -f --no-pager >"$KERNEL_LOG_FILE" 2>&1 &
    KERNEL_WATCH_PID=$!
    echo "Monitoramento auxiliar do kernel ativo (PID=$KERNEL_WATCH_PID -> $KERNEL_LOG_FILE)." | tee -a "$LOG_FILE"
fi

sync "$LOG_FILE" 2>/dev/null || true

if command -v stdbuf >/dev/null 2>&1; then
    stdbuf -oL -eL "$ENGINE_BIN" >>"$LOG_FILE" 2>&1
    RC=$?
else
    "$ENGINE_BIN" >>"$LOG_FILE" 2>&1
    RC=$?
fi

sync "$LOG_FILE" 2>/dev/null || true
echo "=== Encerrado com código de saída: $RC ($(date -Iseconds)) ===" | tee -a "$LOG_FILE"
sync "$LOG_FILE" 2>/dev/null || true
exit "$RC"
