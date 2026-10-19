#!/usr/bin/env bash
# UFC 2 native port (bring-up, Linux). Usage:
#   ./ufc2.sh /path/to/CUSA01968       plays UFC 2 from that folder and remembers it
#                                       (an adjacent CUSA01968-patch folder is merged automatically)
#   ./ufc2.sh                          plays the remembered folder
#   ./ufc2.sh --register [/path]       records this dump's executable hash (share/ufc2_builds.json)
#   ./ufc2.sh --info [/path]           prints the dump's title ID, version and executable hash
# Settings: ~/.local/share/UFC2Native/ufc2.conf (KEY=value lines, e.g. BB_FPS_LIMIT=60).
# Controls and overlay: bbport.ini in the same folder. Logs: logs/ there.
# There is no GTK launcher and no online play for UFC 2 yet (docs/ufc2/BRINGUP.md).
set -euo pipefail
here=$(cd -- "$(dirname -- "$0")" && pwd)
# From a source checkout (packaging/linux/ufc2.sh) the port's root is two folders up.
[[ -f $here/run.sh ]] || here=$(cd -- "$here/../.." && pwd)
command -v python3 >/dev/null || { echo "UFC 2: python3 is needed (install it with your package manager)" >&2; exit 1; }
data=${BB_DATA_DIR:-${XDG_DATA_HOME:-$HOME/.local/share}/UFC2Native}
mkdir -p "$data"
[[ -f $data/ufc2.conf ]] || printf '# KEY=value lines; a variable set in the environment wins.\n# BB_FPS_LIMIT=60\n# BB_NEO=0\n' > "$data/ufc2.conf"

action=play
case ${1:-} in --register|--info) action=${1#--}; shift ;; esac
if [[ $# -gt 0 && -d $1 ]]; then
    game=$(cd -- "$1" && pwd); shift
    printf '%s\n' "$game" > "$data/game-folder"
elif [[ -f $data/game-folder ]]; then
    game=$(cat "$data/game-folder")
else
    sed -n '2,10p' "$0" | sed 's/^# \{0,1\}//' >&2; exit 1
fi
[[ -f $game/eboot.bin ]] || { echo "UFC 2: no eboot.bin in $game" >&2; exit 1; }
game=$(python3 "$here/scripts/merge_update.py" "$game" | tail -1)

case $action in
    register) exec python3 "$here/scripts/game_check.py" "$game" --register ;;
    info) exec python3 "$here/scripts/game_check.py" "$game" --info ;;
esac
profile=$(python3 "$here/scripts/game_check.py" "$game") || { echo "$profile" >&2; exit 2; }
if [[ $profile != ufc2 ]]; then
    echo "UFC 2: $game is not UFC 2 (game check says: $profile). For UFC 3 use ./ufc3.sh." >&2; exit 2
fi
while IFS= read -r line || [[ -n $line ]]; do
    [[ $line =~ ^([A-Z][A-Z0-9_]*)=(.*)$ ]] || continue
    key=${BASH_REMATCH[1]} value=${BASH_REMATCH[2]}
    [[ -n $value && -z ${!key+x} ]] && export "$key=$value"
done < "$data/ufc2.conf"
[[ -d $here/bin ]] && export BB_PREBUILT=${BB_PREBUILT:-1}  # a release package; a checkout builds
export BB_DATA_DIR="$data" BB_GAME_DIR="$game" BB_SAVE_LOG=${BB_SAVE_LOG:-1}
[[ -d $here/bin/lib ]] && export LD_LIBRARY_PATH="$here/bin/lib${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}"
cd "$here"
exec "$here/run.sh" "$@"
