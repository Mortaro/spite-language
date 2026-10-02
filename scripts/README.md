# Helper tools

Rescued from `.spite-cache/` before it was deleted; they were written while porting the compiler and are not
part of any build.

- `patch_tool.py`: applies a patch file to `bootstrap/source/generation/generator.spite`.
- `regenerate_type_shape.py`: regenerates type-shape tables.

`formatting/` lists every `.spite` file under the folders it is given that the compiler's formatter would change,
writing nothing: `check.sh` runs it over the repository, since a compile formats a program's own files but never
`library/`. It loads the compiler's sources, whose comments link to `../docs` from the entry file's folder, so
`check.sh` builds it from a copy with `docs/` beside it.

The old `conformance.sh`, `rebuild.sh` and `selfcompile.sh` are gone: `check.sh` at the repository root
replaces all three.
