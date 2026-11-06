#!/usr/bin/env python3
"""Expose compiler errors as Actions annotations when full logs require sign-in."""
import sys
from pathlib import Path
lines = Path(sys.argv[1]).read_text(errors='replace').splitlines()
indices = set()
for i, line in enumerate(lines):
    if any(marker in line.lower() for marker in ('error:', 'fatal error', 'undefined symbols', 'duplicate symbol', 'linker command failed')):
        indices.update(range(max(0, i - 2), min(len(lines), i + 9)))
message = '\n'.join(lines[i] for i in sorted(indices)) if indices else '\n'.join(lines[-65:])
message = message[-14000:].replace('%', '%25').replace('\r', '%0D').replace('\n', '%0A')
print('::error title=macOS compiler diagnostics::' + message)
