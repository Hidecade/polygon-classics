# Corresponding source

The release's complete modified MAME source is available as
[mame-web-source-20261001.2.tar.gz](https://github.com/Hidecade/polygon-classics/releases/download/web-20261001.2/mame-web-source-20261001.2.tar.gz).
The archive contains source code only, not user-supplied game ROMs or private development files.
Verify its SHA-256 using `SOURCE.json`. It is based on MAME commit
`9d6e9c51c18dd7849e3265b89d6dfd63e1f7dcfb` and includes all local changes and added headers.
Existing license notices are retained throughout the source tree.

## Rebuild

Install Docker. In a clone of this repository, download the source archive and extract it into `source/` so that `source/mame/makefile` exists.

```sh
tar -xzf mame-web-source-20261001.2.tar.gz -C source
docker run --rm -v "$PWD:/work" -w /work emscripten/emsdk:6.0.2 bash /work/source/rebuild-core.sh
```

The build uses SDL3, single-threaded WebAssembly, GPU packet capture, and the four driver source files listed in `rebuild-core.sh`. It produces the eight supported titles and their documented variants. The resulting JavaScript and compressed/chunked WASM are written into `docs/core/`. Diagnostic linking uses `-g2`, matching this release's build command. The Emscripten runtime compatibility changes are applied by `patch-web-runtime.py`.

MAME's build requires substantial memory and disk space. `JOBS` controls compile concurrency.
A bit-for-bit match is not guaranteed across host platforms; `docs/core/manifest.json` identifies the exact published WASM by SHA-256.

## Modification record

- `mame-web.patch`: complete tracked changes from the pinned upstream commit, including Web boot acceleration and display modes.
- `overlay/`: added source headers/includes at their MAME-relative locations. They are already in the complete source archive.
- Browser JavaScript/WGSL/CSS/HTML source: `../docs/`.
- Licensing: MAME is distributed under GPL-2.0 overall; see the archive's `COPYING` and `docs/legal/` for individual component terms. The site includes those notices in `../docs/licenses/`.
