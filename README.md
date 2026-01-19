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

## Projects made with Soluna

- [Deep Future](https://github.com/cloudwu/deepfuture), a digital version of boardgame Deep Future.

## License

Soluna is licensed under the MIT License. See [LICENSE](./LICENSE) for details.
