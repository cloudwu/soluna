import { execFile } from 'node:child_process'
import { copyFile, mkdir, rm, stat } from 'node:fs/promises'
import path from 'node:path'
import process from 'node:process'
import { fileURLToPath } from 'node:url'
import { promisify } from 'node:util'

const execFileAsync = promisify(execFile)

const websiteDir = path.resolve(fileURLToPath(new URL('..', import.meta.url)))
const rootDir = path.resolve(websiteDir, '..')
const websiteRuntimeDir = path.join(websiteDir, 'public', 'runtime')

function resolveMode() {
  return process.env.SOLUNA_MODE || 'release'
}

function resolveRuntimePath(name, fallback) {
  const configuredPath = process.env[name]
  if (!configuredPath) {
    return fallback
  }
  if (path.isAbsolute(configuredPath)) {
    return configuredPath
  }
  return path.resolve(rootDir, configuredPath)
}

function exists(filePath) {
  return stat(filePath).then(() => true, () => false)
}

async function ensureFile(sourcePath, label) {
  if (!(await exists(sourcePath))) {
    throw new Error(`Missing ${label}: ${sourcePath}`)
  }
}

async function createAssetZip(outputPath) {
  await execFileAsync('zip', ['-qr', outputPath, 'asset'], {
    cwd: rootDir,
  })
}

async function main() {
  const mode = resolveMode()
  const solunaJsPath = resolveRuntimePath('SOLUNA_JS_PATH', path.join(rootDir, 'bin', 'emcc', mode, 'soluna.js'))
  const solunaWasmPath = resolveRuntimePath('SOLUNA_WASM_PATH', path.join(rootDir, 'bin', 'emcc', mode, 'soluna.wasm'))
  const sampleWasmPath = resolveRuntimePath('SAMPLE_WASM_PATH', path.join(rootDir, 'bin', 'emcc', 'release', 'sample.wasm'))
  const solunaWasmMapPath = resolveRuntimePath(
    'SOLUNA_WASM_MAP_PATH',
    path.join(rootDir, 'bin', 'emcc', mode, 'soluna.wasm.map'),
  )

  await ensureFile(solunaJsPath, 'soluna.js')
  await ensureFile(solunaWasmPath, 'soluna.wasm')
  await ensureFile(sampleWasmPath, 'sample.wasm')

  await rm(websiteRuntimeDir, { recursive: true, force: true })
  await mkdir(websiteRuntimeDir, { recursive: true })

  await copyFile(solunaJsPath, path.join(websiteRuntimeDir, 'soluna.js'))
  await copyFile(solunaWasmPath, path.join(websiteRuntimeDir, 'soluna.wasm'))
  await copyFile(sampleWasmPath, path.join(websiteRuntimeDir, 'sample.wasm'))

  if (await exists(solunaWasmMapPath)) {
    await copyFile(solunaWasmMapPath, path.join(websiteRuntimeDir, 'soluna.wasm.map'))
  }

  await createAssetZip(path.join(websiteRuntimeDir, 'asset.zip'))

  process.stdout.write(`Prepared website runtime in ${websiteRuntimeDir}\n`)
}

main().catch((error) => {
  process.stderr.write(`${error instanceof Error ? error.message : String(error)}\n`)
  process.exitCode = 1
})
