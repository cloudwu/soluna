<div align="center">

Sokol + Lua = Soluna

</div>

# Soluna

[Live Examples / 在线示例](https://cloudwu.github.io/soluna/)

Soluna is a 2D game framework for Lua. It is built on top of [sokol](https://github.com/floooh/sokol), integrates ltask for multithreading, and runs on Windows, Linux, macOS, and modern browsers through WebAssembly.

Soluna 是一个 Lua 2D 游戏框架。它基于 [sokol](https://github.com/floooh/sokol)，整合 ltask 作为多线程框架，可运行在 Windows、Linux、macOS 以及通过 WebAssembly 支持的现代浏览器中。

[![Nightly](/../../actions/workflows/nightly.yml/badge.svg)](/../../actions/workflows/nightly.yml)

## Documentation / 文档

- [API Reference / API 参考](./docs)
- [Examples / 示例](./test)
- [Wiki](https://github.com/cloudwu/soluna/wiki)

## Precompiled Binaries / 预编译二进制文件

Precompiled binaries for Windows, Linux, macOS, and WebAssembly are available from [Nightly Releases](/../../releases/tag/nightly).

Windows、Linux、macOS 和 WebAssembly 的预编译二进制文件可从 [Nightly Releases](/../../releases/tag/nightly) 下载。

## Building from Source / 从源码构建

Soluna can be built with `make` on Windows and with `luamake` on all supported platforms. The GitHub Action in [`.github/actions/soluna`](./.github/actions/soluna) shows the exact CI build flow.

Soluna 可在 Windows 上通过 `make` 构建，也可在所有支持平台上通过 `luamake` 构建。[`.github/actions/soluna`](./.github/actions/soluna) 展示了 CI 使用的完整构建流程。

### GitHub Actions Integration / GitHub Actions 集成

```yaml
- uses: actions/checkout@v6
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

## Local Website Build / 本地构建与运行网站

The website is an Astro app in `website/`. It renders the homepage from this README, generates API pages from `docs/`, and builds live example pages from `test/`.

网站是位于 `website/` 目录的 Astro 应用。它使用本 README 生成首页，从 `docs/` 生成 API 页面，并从 `test/` 生成在线示例页面。

Build the WebAssembly runtime from the repository root first:

先在仓库根目录构建 WebAssembly runtime：

```bash
luamake -compiler emcc
luamake -compiler emcc sample
```

Then install dependencies and start the local dev server:

然后安装依赖并启动本地开发服务器：

```bash
cd website
pnpm install
pnpm run dev
```

## Projects Made with Soluna / 使用 Soluna 制作的项目

- [Deep Future](https://github.com/cloudwu/deepfuture), a digital version of the board game Deep Future. / 电子版桌游《深远未来》。

## License / 许可证

Soluna is licensed under the MIT License. See [LICENSE](./LICENSE) for details.

Soluna 使用 MIT 许可证。详情见 [LICENSE](./LICENSE)。
