#!/usr/bin/env bash
set -euo pipefail

HOMOLOG_DIR="${SYSTALIA_HOMOLOG_DIR:-$HOME/.local/systalia-homolog}"
BUILD_DIR="${SYSTALIA_HOMOLOG_BUILD:-$HOME/.cache/sge-build-homolog}"
LOG_DIR="${SYSTALIA_HOMOLOG_LOGS:-$HOME/systalia-homolog-logs}"

echo "=== SYSTALIA 03.2.4.6 — Verificação Não Invasiva Pré-Homologação ==="
echo "Data: $(date -Iseconds)"
echo "Usuário: $(id -un) (UID=$EUID)"
echo "Diretório instalado: $HOMOLOG_DIR"

if [[ $EUID -eq 0 ]]; then
    echo "[FAIL] Não executar verificação ou homologação como root." >&2
    exit 1
fi

for req in \
    "$HOMOLOG_DIR/bin/systalia_glass_engine" \
    "$HOMOLOG_DIR/bin/systalia-session" \
    "$HOMOLOG_DIR/bin/platforms/libsge_vulkan_offscreen.so" \
    "$HOMOLOG_DIR/share/wayland-sessions/systalia.desktop" \
    "$HOMOLOG_DIR/share/systalia/eglfs_kms.json"; do
    if [[ ! -e "$req" ]]; then
        echo "[FAIL] Artefato ausente: $req" >&2
        exit 1
    fi
    echo "[OK] $req"
done

if [[ ! -c /dev/dri/card1 || ! -c /dev/dri/renderD128 ]]; then
    echo "[FAIL] Nós DRM esperados (/dev/dri/card1, /dev/dri/renderD128) ausentes." >&2
    exit 1
fi
echo "[OK] Nós DRM presentes: /dev/dri/card1, /dev/dri/renderD128"

if [[ -n "${SGE_USE_DRM_BACKEND:-}" ]]; then
    echo "[WARN] SGE_USE_DRM_BACKEND está definido no ambiente atual ($SGE_USE_DRM_BACKEND)."
else
    echo "[OK] SGE_USE_DRM_BACKEND desativado por padrão."
fi

mkdir -p "$LOG_DIR"
echo "[OK] Diretório de logs: $LOG_DIR"

echo "=== Conectores DRM (sysfs, leitura não invasiva) ==="
for conn in /sys/class/drm/card1-*; do
    [[ -d "$conn" ]] || continue
    status="$(cat "$conn/status" 2>/dev/null || echo unknown)"
    mode="$(head -n 1 "$conn/modes" 2>/dev/null || echo none)"
    echo "  $(basename "$conn"): status=$status preferred_or_first_mode=$mode"
done

echo "=== Verificação ldd (libsge_vulkan_offscreen.so / systalia_glass_engine) ==="
if ldd "$HOMOLOG_DIR/bin/systalia_glass_engine" | grep -q "not found"; then
    echo "[FAIL] Dependência ausente em systalia_glass_engine." >&2
    exit 1
fi
if ldd "$HOMOLOG_DIR/bin/platforms/libsge_vulkan_offscreen.so" | grep -q "not found"; then
    echo "[FAIL] Dependência ausente em libsge_vulkan_offscreen.so." >&2
    exit 1
fi
echo "[OK] Todas as bibliotecas compartilhadas resolvidas."
echo "=== Verificação concluída com sucesso ==="
