#!/usr/bin/env bash
set -euo pipefail

LOG="$HOME/.local/share/borg-backup.log"

# Repo + env
export BORG_REPO="/srv/pika/pika-local"
export BORG_RELOCATED_REPO_ACCESS_IS_OK=yes

# Load passphrase if present (no sudo)
if [ -f "$HOME/.config/borg/passphrase" ]; then
  # shellcheck disable=SC1090
  source "$HOME/.config/borg/passphrase"
  export BORG_PASSPHRASE
fi

# Optional excludes file (skip if missing)
EXCLUDES="$HOME/.config/pika-backup/excludes-common.txt"
EXCLUDE_ARGS=()
[ -f "$EXCLUDES" ] && EXCLUDE_ARGS=(--exclude-from "$EXCLUDES")

{
  echo "==== Borg backup started at $(date '+%F %T') ===="

  rc=0
  borg create --stats --compression zstd,10 --one-file-system \
    "${EXCLUDE_ARGS[@]}" \
    "$BORG_REPO"::mini-home-$(date +%Y-%m-%d_%H-%M) \
    "$HOME" || rc=$?

  if [ "$rc" -eq 1 ]; then
    echo "borg create completed with warnings (rc=1)"
  elif [ "$rc" -gt 1 ]; then
    echo "borg create failed (rc=$rc)"
    exit "$rc"
  fi

  rc=0
  borg prune -v --list "$BORG_REPO" \
    --keep-hourly=4 --keep-daily=30 --keep-weekly=12 --keep-monthly=6 || rc=$?

  if [ "$rc" -eq 1 ]; then
    echo "borg prune completed with warnings (rc=1)"
  elif [ "$rc" -gt 1 ]; then
    echo "borg prune failed (rc=$rc)"
    exit "$rc"
  fi

  echo "==== Borg backup finished at $(date '+%F %T') ===="
  echo
} >> "$LOG" 2>&1
