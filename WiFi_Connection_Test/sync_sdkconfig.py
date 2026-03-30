#!/usr/bin/env python3
"""
Syncs sdkconfig -> sdkconfig.default, stripping WiFi credentials.
Run automatically as a post-build step via CMakeLists.txt.
"""
import re
import sys
from pathlib import Path

EXCLUDE_KEYS = {
    "CONFIG_EXAMPLE_WIFI_SSID",
    "CONFIG_EXAMPLE_WIFI_PASSWORD",
}

# Keys whose resolved values should be replaced with a placeholder in sdkconfig.default
PLACEHOLDER_KEYS = {
    "CONFIG_IDF_INIT_VERSION": '"$IDF_INIT_VERSION"',
}

def filter_sdkconfig(content: str) -> str:
    filtered_lines = []
    for line in content.splitlines(keepends=True):
        if any(
            re.match(rf"^{key}=", line) or re.match(rf"^# {key} is not set", line)
            for key in EXCLUDE_KEYS
        ):
            continue

        for key, placeholder in PLACEHOLDER_KEYS.items():
            if re.match(rf"^{key}=", line):
                line = f"{key}={placeholder}\n"
                break

        filtered_lines.append(line)

    return "".join(filtered_lines)


def sync(project_dir: Path) -> None:
    src = project_dir / "sdkconfig"
    dst = project_dir / "sdkconfig.default"

    if not src.exists():
        print(f"[sync_sdkconfig] Warning: {src} not found, skipping.", file=sys.stderr)
        return

    new_content = filter_sdkconfig(src.read_text(encoding="utf-8"))

    if dst.exists() and dst.read_text(encoding="utf-8") == new_content:
        print("[sync_sdkconfig] sdkconfig.default already up to date, skipping write.")
        return

    dst.write_text(new_content, encoding="utf-8")
    print("[sync_sdkconfig] Synced sdkconfig -> sdkconfig.default (WiFi credentials excluded, IDF version placeholder restored)")


if __name__ == "__main__":
    sync(Path(__file__).parent)
