#!/usr/bin/env bash

set -euo pipefail

script_root="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
repository_root="$(cd -- "$script_root/.." && pwd)"
executable_path="${1:-$repository_root/build-linux/install/bin/desktopctl}"
output_directory="${2:-$repository_root/build-linux/package}"

if [[ ! -f "$executable_path" ]]; then
    printf 'Executable not found: %s\n' "$executable_path" >&2
    exit 1
fi

manifest_path="$repository_root/plugin/desktopctl/plugin.json"
package_version="$(sed -n 's/^[[:space:]]*"version":[[:space:]]*"\([^"]*\)".*/\1/p' "$manifest_path" | head -n 1)"
cmake_version="$(sed -n 's/^project(desktopctl VERSION \([^ ]*\) LANGUAGES CXX).*/\1/p' "$repository_root/CMakeLists.txt")"
if [[ -z "$package_version" || "$package_version" != "$cmake_version" ]]; then
    printf 'Version mismatch between plugin.json and CMakeLists.txt.\n' >&2
    exit 1
fi

machine="$(LC_ALL=C readelf -h "$executable_path" | sed -n 's/^[[:space:]]*Machine:[[:space:]]*//p')"
case "$machine" in
    *X86-64*|*x86-64*)
        package_architecture="x86_64"
        ;;
    *AArch64*)
        package_architecture="aarch64"
        ;;
    *)
        printf 'Unsupported Linux executable architecture: %s\n' "$machine" >&2
        exit 1
        ;;
esac

package_name="desktopctl-plugin-$package_version-linux-$package_architecture"
output_directory="$(mkdir -p -- "$output_directory" && cd -- "$output_directory" && pwd)"
stage_root="$output_directory/$package_name"
zip_path="$output_directory/$package_name.zip"

if [[ -e "$stage_root" ]]; then
    if [[ "$(dirname -- "$stage_root")" != "$output_directory" ||
          "$(basename -- "$stage_root")" != "$package_name" ]]; then
        printf 'Refusing to replace an unexpected staging directory: %s\n' "$stage_root" >&2
        exit 1
    fi
    rm -rf -- "$stage_root"
fi

plugin_destination="$stage_root/plugins/desktopctl"
marketplace_destination="$stage_root/.agents/plugins"
mkdir -p -- "$plugin_destination/bin" "$marketplace_destination"
cp -a -- "$repository_root/plugin/desktopctl/." "$plugin_destination/"
install -m 0755 -- "$executable_path" "$plugin_destination/bin/desktopctl"
cp -- "$repository_root/plugin/marketplace.json" \
    "$marketplace_destination/marketplace.json"
cp -- "$repository_root/plugin/package/README.md" "$stage_root/README.md"

cat > "$plugin_destination/mcp.json" <<'EOF'
{
  "$schema": "https://agent-plugins.org/schemas/1.0.0/mcp.schema.json",
  "mcpServers": {
    "desktopctl": {
      "type": "stdio",
      "command": "${PLUGIN_ROOT}/bin/desktopctl",
      "args": ["mcp", "serve"],
      "cwd": "${PLUGIN_ROOT}"
    }
  }
}
EOF

cat > "$plugin_destination/.mcp.json" <<'EOF'
{
  "mcpServers": {
    "desktopctl": {
      "command": "${PLUGIN_ROOT}/bin/desktopctl",
      "args": ["mcp", "serve"],
      "cwd": "${PLUGIN_ROOT}",
      "enabled": true
    }
  }
}
EOF

checksums_path="$stage_root/checksums.sha256"
while IFS= read -r file_path; do
    relative_path="${file_path#"$stage_root/"}"
    printf '%s  %s\n' "$(sha256sum -- "$file_path" | cut -d ' ' -f 1)" "$relative_path"
done < <(find "$stage_root" -type f ! -name checksums.sha256 | LC_ALL=C sort) \
    > "$checksums_path"

rm -f -- "$zip_path"
(cd -- "$stage_root" && cmake -E tar cf "$zip_path" --format=zip .)

printf 'Package directory: %s\n' "$stage_root"
printf 'Local UI package archive: %s\n' "$zip_path"
printf 'Runtime SHA256: %s\n' "$(sha256sum -- "$executable_path" | cut -d ' ' -f 1)"
