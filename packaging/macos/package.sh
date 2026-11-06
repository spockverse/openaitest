#!/bin/bash
set -euo pipefail
project_root="$(cd "$(dirname "$0")/../.." && pwd)"
cd "$project_root"
build_dir="${1:-build}"
output_dir="${2:-dist}"
python3 tools/validate_factory.py
stage_dir="$(mktemp -d)"
trap 'rm -rf "$stage_dir"' EXIT
mkdir -p "$stage_dir/Library/Audio/Plug-Ins/Components" "$stage_dir/Library/Audio/Plug-Ins/VST3" "$stage_dir/Library/Application Support/Spock Audio/WUBROT/Presets" "$output_dir"
cp -R "$build_dir/WUBROT_artefacts/Release/AU/WUBROT.component" "$stage_dir/Library/Audio/Plug-Ins/Components/"
cp -R "$build_dir/WUBROT_artefacts/Release/VST3/WUBROT.vst3" "$stage_dir/Library/Audio/Plug-Ins/VST3/"
cp -R Resources/Presets/Factory "$stage_dir/Library/Application Support/Spock Audio/WUBROT/Presets/"
pkgbuild --root "$stage_dir" --identifier audio.spock.wubrot --version 0.2.0 --install-location / "$output_dir/WUBROT-0.2.0.pkg"
