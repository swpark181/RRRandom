#!/bin/sh
# Copies this PC's Claude Code rules into the repo before a ship, so they travel with the code to other PCs:
#   memory notes  (~/.claude/projects/<project>/memory/*.md)  -> Docs/ClaudeMemory/
#   own skills    (~/.claude/skills/<name>/, not "synced")     -> Docs/ClaudeSkills/<name>/
#   user settings (~/.claude/settings.json, keybindings.json, CLAUDE.md) and any CLAUDE.md / .claude/settings*.json in
#   the folders above the repo                                  -> Docs/ClaudeSettings/
# Never copies credentials (.credentials.json). A file is copied only when it is missing in the repo or the local one is
# newer (cp -u), so a PC whose notes are older than the repo's doesn't overwrite them; nothing in the repo is deleted.
# Prints what changed and the repo files that have no local source (deleted here, or written on another PC).
# Usage: sh sync_claude.sh   (from inside the repo)
set -e
repo=$(git rev-parse --show-toplevel)
claude="$HOME/.claude"
docs="$repo/Docs"

# The memory folder: Claude names a project after its folder (C:\work -> C--work); the repo or the nearest folder above it
here=$(cd "$repo" && (pwd -W 2>/dev/null || pwd))
mem=""
p=$here
while :; do
  name=$(printf '%s' "$p" | sed 's#[:/\\]#-#g')
  if [ -d "$claude/projects/$name/memory" ]; then mem="$claude/projects/$name/memory"; break; fi
  parent=$(dirname "$p")
  [ "$parent" = "$p" ] && break
  p=$parent
done

copy() { # copy <from> <to>: only if missing or older there; says so
  mkdir -p "$(dirname "$2")"
  if [ ! -f "$2" ] || [ "$1" -nt "$2" ]; then
    if [ ! -f "$2" ] || ! cmp -s "$1" "$2"; then cp -p "$1" "$2"; echo "copied ${2#$repo/}"; fi
  fi
}

if [ -n "$mem" ]; then
  for f in "$mem"/*.md; do [ -f "$f" ] && copy "$f" "$docs/ClaudeMemory/$(basename "$f")"; done
  for f in "$docs"/ClaudeMemory/*.md; do
    b=$(basename "$f"); [ "$b" = README.md ] && continue
    [ -f "$mem/$b" ] || echo "only in repo: Docs/ClaudeMemory/$b"
  done
else
  echo "no memory folder found for $here"
fi

for d in "$claude"/skills/*/; do
  s=$(basename "$d")
  [ "$s" = synced ] && continue
  [ -f "$d/SKILL.md" ] || continue
  (cd "$d" && find . -type f) | while read -r f; do copy "$d/$f" "$docs/ClaudeSkills/$s/${f#./}"; done
done

for f in settings.json keybindings.json CLAUDE.md; do
  [ -f "$claude/$f" ] && copy "$claude/$f" "$docs/ClaudeSettings/$f"
done
# Rules in the folders above the repo (e.g. C:\work\CLAUDE.md, C:\work\.claude\settings.json)
p=$(dirname "$here")
while :; do
  tag=$(printf '%s' "$p" | sed 's#[:/\\]#-#g')
  for f in CLAUDE.md CLAUDE.local.md .claude/settings.json .claude/settings.local.json; do
    [ -f "$p/$f" ] && copy "$p/$f" "$docs/ClaudeSettings/$tag/$(echo "$f" | tr '/' '_')"
  done
  parent=$(dirname "$p")
  [ "$parent" = "$p" ] && break
  p=$parent
done
exit 0
