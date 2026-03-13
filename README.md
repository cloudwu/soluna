<div align="center">

Sokol + Lua = Soluna

</div>

# Soluna

[中文](./README.zh-CN.md)

[Live Examples](https://cloudwu.github.io/soluna/)

A framework you can use to make 2D games in Lua with multithreading, living on Windows, Linux, macOS and modern Browsers (via WebAssembly).

It is built on top of [sokol](https://github.com/floooh/sokol) and leverages the power of ltask for multithreading.

[![Nightly](/../../actions/workflows/nightly.yml/badge.svg)](/../../actions/workflows/nightyly.yml)

## Documentation

- [API Reference](./docs)
- [Examples](./test)
- [Wiki](https://github.com/cloudwu/soluna/wiki)

## Precompiled Binaries

You can download precompiled binaries for Windows, Linux, macOS and WebAssembly from the [Nightly Releases](/../../releases/tag/nightly) page.

## Building from Source

You can build Soluna from source by `make` for Windows and by `luamake` for all platforms. See [actions](./.github/actions/soluna) for details.

### Integration with the Actions of your projects

```yaml
- uses: actions/checkout@v5
  with:
    repository: cloudwu/soluna
    ref: <a fixed commit hash to avoid breaking changes>
    path: soluna
    submodules: recursive
- uses: ./soluna/.github/actions/soluna
  id: soluna
  with:
    soluna_path: soluna
- run: |
    echo "Soluna binary is at ${{ steps.soluna.outputs.SOLUNA_PATH }}"
    echo "Soluna WASM binary is at ${{ steps.soluna.outputs.SOLUNA_WASM_PATH }}"
    echo "Soluna js glue is at ${{ steps.soluna.outputs.SOLUNA_JS_PATH }}"

```
## Run CI Locally with Act

You can easily run GitHub workflows locally through `act` via luamake.

Prerequisites:

- Docker (daemon running)
- [act](https://github.com/nektos/act) in `PATH`
- Python 3 in `PATH` (used by local preview server for `pages`)
- `unzip` and `tar` (or equivalent tools available in your environment)

Examples:

```bash
luamake act pages
luamake act nightly
```

Notes:

- `luamake act pages` runs `.github/workflows/pages.yml`, extracts the generated pages artifact, and serves it locally at `http://127.0.0.1:8080/soluna/`.
- `luamake act nightly` runs `.github/workflows/nightly.yml`.
- Use `PORT` to change preview port (for example: `PORT=9000 luamake act pages`).

## Projects made with Soluna

- [Deep Future](https://github.com/cloudwu/deepfuture), a digital version of boardgame Deep Future.

## License

Soluna is licensed under the MIT License. See [LICENSE](./LICENSE) for details.
