<div align="center">

Sokol + Lua = Soluna

</div>

# Soluna

[English](./README.md)

[在线演示](https://cloudwu.github.io/soluna/)

一个基于 [sokol](https://github.com/floooh/sokol) 的 2D 游戏引擎。它以 Lua 为编程语言，整合了 ltask 作为多线程框架。sokol + lua 是其名字的由来。

Soluna 可以运行在 Windows、Linux、macOS 和现代浏览器（通过 WebAssembly）上。

[![Nightly](/../../actions/workflows/nightly.yml/badge.svg)](/../../actions/workflows/nightyly.yml)

## 文档

- [API 参考](./docs)
- [示例](./test)
- [Wiki](https://github.com/cloudwu/soluna/wiki)

## 预编译二进制文件

你可以从 [Nightly Releases](/../../releases/tag/nightly) 页面下载 Windows、Linux、macOS 和
WebAssembly 的预编译二进制文件。

## 从源码构建

你可以通过 `make` 在 Windows 上构建 Soluna，通过 `luamake` 在所有平台上构建 Soluna。详情见
[actions](./.github/actions/soluna)。

### 在你的项目的 Actions 中集成

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

## 使用 Act 在本地运行 CI

你可以通过 `act` 和 luamake target 在本地执行仓库中的 workflow。

前置要求：

- Docker（daemon 已启动）
- `PATH` 中可用的 [act](https://github.com/nektos/act)
- `PATH` 中可用的 Python 3（`pages` 本地预览会用到）
- `unzip` 与 `tar`（或环境中等价可用工具）

示例：

```bash
luamake act pages
luamake act nightly
```

说明：

- `luamake act pages` 会执行 `.github/workflows/pages.yml`，解包生成的 pages artifact，并在本地 `http://127.0.0.1:8080/soluna/` 提供预览。
- `luamake act nightly` 会执行 `.github/workflows/nightly.yml`。
- 可通过 `PORT` 修改预览端口（例如：`PORT=9000 luamake act pages`）。

## 使用 Soluna 制作的项目

- [Deep Future](https://github.com/cloudwu/deepfuture), 一款桌游 Deep Future 的数字版。

## 许可证

Soluna 使用 MIT 许可证。详情见 [LICENSE](./LICENSE)。
