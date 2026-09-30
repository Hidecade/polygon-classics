# Polygon Classics

Web arcade emulator · 20261001.5

**Play:** https://hidecade.github.io/polygon-classics/

Polygon Classics is a browser-based arcade emulator with GPU rendering, widescreen display options, and touch controls. Bring your own compatible ROM files to explore polygon-era arcade hardware.

Select your ROM folder in the browser. The game selector appears only after loading a folder and lists the compatible game ZIPs found there. The selected game and its BIOS ZIPs are found automatically, including subfolders. Game ROMs and BIOS ROMs are not included. Selected files are read locally; they are not uploaded to this site.

- DISPLAY: 4:3 (1440×1080), FHD (1920×1080), Ultra Wide (2580×1080).
- WebGPU rendering with CPU fallback. Ultra Wide can expose missing or unstable graphics.
- Boot checks run with frame skipping behind a LOADING screen.
- Touch controls and supported-browser motion controls. iPhone Safari vibration is not supported.
- Browser performance varies by device. Wi-Fi multiplayer is not included in this Web release.

## Publishing

GitHub Pages serves **main /docs**. Only the contents of `docs/` become the website. No native launch API or ROM storage is deployed. `.nojekyll` keeps the site static.

The core is single-threaded WebAssembly. GitHub Pages does not apply Cloudflare `_headers`; this build does not require cross-origin isolation. WebGPU and motion controls use HTTPS.

## Source and build

The browser application sources are the JavaScript modules and shaders in `docs/`.
The complete modified MAME source is attached to release [web-20261001.2](https://github.com/Hidecade/polygon-classics/releases/tag/web-20261001.2).
`source/mame-web.patch` and `source/overlay/` document the modifications against upstream commit `9d6e9c51c18dd7849e3265b89d6dfd63e1f7dcfb`.

See [source/README.md](source/README.md) for rebuilding the core.
MAME and bundled third-party code retain their original licenses and copyright notices: [licenses](docs/licenses.html).
Game names, logos and game content belong to their respective rights holders; no ownership of those assets is claimed.

ROM-folder indexing tests: `node --test tests/rom-library.test.mjs`. Folder access lasts for this page session; choose the folder again after reloading. Startup errors appear above the menu.
