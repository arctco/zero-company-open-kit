#!/usr/bin/env bash
# Put Open Kit back exactly as it was before the uninstall persistence test.
#
#   ./scripts/restore-after-uninstall-test.sh
#
# The uninstall test moves the module and its two containers out of the game
# rather than deleting them, so this is a move back rather than a reinstall --
# the twelve option files, and therefore which switches were on, come back
# unchanged. See CHANGELOG.md for what the test is asking.
set -euo pipefail

GAME="${ZCOM_GAME_DIR:-$HOME/.steam/steam/steamapps/common/Star Wars Zero Company}/SWZeroCompany"
HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
BACKUP="${OPENKIT_UNINSTALL_BACKUP:-$(dirname "$HERE")/../lab/private/uninstall-test-backup}"

[ -d "$BACKUP/mod/ZCOMOpenKit" ] || { echo "nothing to restore at $BACKUP" >&2; exit 1; }

mv "$BACKUP/mod/ZCOMOpenKit" "$GAME/Binaries/Win64/ue4ss/Mods/ZCOMOpenKit"
mv "$BACKUP"/containers/pakchunk*ZCOMOpenKit*_P.* "$GAME/Content/Paks/~mods/"
rmdir "$BACKUP/mod" "$BACKUP/containers" 2>/dev/null || true

echo "restored:"
find "$GAME/Binaries/Win64/ue4ss/Mods/ZCOMOpenKit" -type f | sed "s|$GAME|  <game>|"
ls "$GAME/Content/Paks/~mods" | grep OpenKit | sed 's|^|  <game>/Content/Paks/~mods/|'
echo
echo "The save backup is kept at:"
echo "  $BACKUP/SaveGames-before-uninstall"
echo "Delete it yourself once you are happy; this script does not touch it."
