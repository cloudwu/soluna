# Website

[English](./README.md)

这个目录包含 Soluna 在线示例网站的 Astro 源码。

它负责：

- 渲染网站页面
- 从 `../docs/` 生成 API 文档页面
- 从 `../test/` 生成在线示例页面
- 使用仓库根目录的 `../README.md` 作为首页内容
- 从 `../asset/` 打包 `asset.zip`

核心 WebAssembly 运行时并不在这里构建。`soluna.js`、`soluna.wasm`
和 `sample.wasm` 由仓库根目录的 `luamake` 生成。

## 本地开发

先在仓库根目录构建 Web 运行时：

```bash
luamake -compiler emcc
luamake -compiler emcc sample
```

然后进入 `website/` 目录启动站点：

```bash
pnpm install
pnpm run dev
```
