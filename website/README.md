# Website

[中文](./README.zh-CN.md)

This directory contains the Astro-based website for Soluna Live Examples.

It is responsible for:

- rendering the website pages
- generating API documentation pages from `../docs/`
- generating online example pages from `../test/`
- using the repository `../README.md` as the homepage content
- packaging `asset.zip` from `../asset/`

The core WebAssembly runtime is not built here. `soluna.js`, `soluna.wasm`,
and `sample.wasm` are produced by `luamake` from the repository root.

## Local development

Build the web runtime from the root:

```bash
luamake -compiler emcc
luamake -compiler emcc sample
```

Then start the website from the `website/` directory:

```bash
pnpm install
pnpm run dev
```
