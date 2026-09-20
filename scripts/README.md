# Helper tools

Rescued from `.spite-cache/` before it was deleted; they were written while porting the compiler and are not
part of any build.

- `patch_tool.py` -- applies a patch file to `bootstrap/source/generation/generator.spite`.
- `regenerate_type_shape.py` -- regenerates type-shape tables.

The old `conformance.sh`, `rebuild.sh` and `selfcompile.sh` are gone: `check.sh` at the repository root
replaces all three.
