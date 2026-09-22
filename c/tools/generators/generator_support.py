"""Common paths and immutable capture provenance for offline regeneration."""
import json
import os
from pathlib import Path

REPOSITORY = Path(__file__).resolve().parents[3]
INPUT = REPOSITORY / 'c/tests/fixtures/inputs'
OUTPUT = Path(os.environ.get('TDX_GENERATED_ROOT', str(REPOSITORY))).resolve()
_SOURCES = json.loads((INPUT.parent / 'sources.json').read_text(encoding='utf-8'))

def source_info(name):
    return _SOURCES[name]
