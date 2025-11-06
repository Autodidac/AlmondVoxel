 
#!/usr/bin/env bash

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
TARGET_NAME="${1:-terrain_demo}"

declare -a SEARCH_ROOTS=(
  "${SCRIPT_DIR}/out/build"
  "${SCRIPT_DIR}/build"
  "${SCRIPT_DIR}/bin"
  "${SCRIPT_DIR}/Bin"
)

for root in "${SEARCH_ROOTS[@]}"; do
  [[ -d "${root}" ]] || continue
  while IFS= read -r -d '' candidate; do
    echo "Launching ${candidate}" >&2
    exec "${candidate}"
  done < <(find "${root}" -type f \( -name "${TARGET_NAME}" -o -name "${TARGET_NAME}.exe" \) -perm -u+x -print0)
done

cat <<'MSG' >&2
No runnable example was found.
Build an example (e.g., cmake --build --preset debug-examples) and rerun this script,
or specify a different target name as the first argument.
MSG

exit 1
