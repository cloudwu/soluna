import { strToU8, zipSync } from 'fflate'

interface RuntimeModule {
  FS: {
    writeFile: (path: string, data: Uint8Array, options?: { canOwn?: boolean }) => void
  }
  FS_createPath: (root: string, path: string, canRead: boolean, canWrite: boolean) => void
  _soluna_runtime_quit?: () => void
}

interface PlayAppOptions {
  appFactory: (options: Record<string, unknown>) => Promise<RuntimeModule>
  appBaseUrl: string
  canvas: HTMLCanvasElement
  print: (text: string) => void
  printErr: (text: string) => void
  onAbort: (reason: unknown) => void
}

interface StartOptions {
  arguments: string[]
  files: Array<{
    path: string
    data: Uint8Array
    canOwn?: boolean
  }>
}

interface PlayOptions {
  exampleSource: string
}

interface RuntimeHandle {
  stop: () => void
}

declare global {
  interface Window {
    SOLUNA_PLAY_ACTIVE?: RuntimeHandle
  }
}

function qs<T extends Element>(selector: string, root: ParentNode = document): T | null {
  return root.querySelector<T>(selector)
}

function normalizeBaseUrl(baseUrl: string): URL {
  const normalized = baseUrl.endsWith('/') ? baseUrl : `${baseUrl}/`
  return new URL(normalized, window.location.href)
}

function normalizeFileData(data: Uint8Array | ArrayBuffer | ArrayBufferView): Uint8Array {
  if (data instanceof Uint8Array) {
    return data
  }
  if (data instanceof ArrayBuffer) {
    return new Uint8Array(data)
  }
  return new Uint8Array(data.buffer, data.byteOffset, data.byteLength)
}

function ensureAbsolutePath(path: string): string {
  if (!path.startsWith('/')) {
    throw new TypeError(`Expected an absolute FS path, got: ${path}`)
  }
  return path
}

function dirname(path: string): string {
  const normalized = ensureAbsolutePath(path)
  const index = normalized.lastIndexOf('/')
  if (index <= 0) {
    return '/'
  }
  return normalized.slice(0, index)
}

function ensureParentDirectory(runtimeModule: RuntimeModule, path: string): void {
  const dir = dirname(path)
  if (dir === '/') {
    return
  }
  runtimeModule.FS_createPath('/', dir.slice(1), true, true)
}

async function fetchArrayBuffer(url: string): Promise<ArrayBuffer> {
  const response = await fetch(url)
  if (!response.ok) {
    throw new Error(`Failed to load ${url}`)
  }
  return response.arrayBuffer()
}

async function ensureCrossOriginIsolation(serviceWorkerUrl: string): Promise<boolean> {
  if (window.crossOriginIsolated) {
    return true
  }
  if (!('serviceWorker' in navigator)) {
    return false
  }

  await navigator.serviceWorker.register(serviceWorkerUrl)
  if (!navigator.serviceWorker.controller) {
    window.location.reload()
    return false
  }
  return true
}

function installRuntimeFiles(runtimeModule: RuntimeModule, files: StartOptions['files']): void {
  files.forEach((file) => {
    ensureParentDirectory(runtimeModule, file.path)
    runtimeModule.FS.writeFile(file.path, file.data, { canOwn: file.canOwn })
  })
}

function resolveQuitApp(instance: RuntimeModule): (() => void) | undefined {
  if (typeof instance._soluna_runtime_quit === 'function') {
    return () => {
      instance._soluna_runtime_quit?.()
    }
  }
}

async function createRuntimeHandle(
  playOptions: PlayAppOptions,
  startOptions: StartOptions,
): Promise<RuntimeHandle> {
  const appBaseUrl = normalizeBaseUrl(playOptions.appBaseUrl)
  const instance = await playOptions.appFactory({
    arguments: startOptions.arguments,
    canvas: playOptions.canvas,
    print: playOptions.print,
    printErr: playOptions.printErr,
    locateFile(path: string) {
      return new URL(path, appBaseUrl).toString()
    },
    preRun: [
      (runtimeModule: RuntimeModule) => {
        installRuntimeFiles(runtimeModule, startOptions.files)
      },
    ],
    onAbort: (reason: unknown) => {
      playOptions.onAbort(reason)
    },
  })

  const quitApp = resolveQuitApp(instance)
  let stopped = false

  return {
    stop() {
      if (stopped) {
        return
      }
      stopped = true

      if (quitApp) {
        try {
          quitApp()
        }
        catch {
          // Ignore quit failures during teardown.
        }
      }
    },
  }
}

function setStatus(text: string): void {
  const status = qs<HTMLElement>('#play-status')
  if (status) {
    status.textContent = text
  }
}

function setNote(text: string): void {
  const note = qs<HTMLElement>('#play-note')
  if (note) {
    note.textContent = text
  }
}

function setOverlayVisible(visible: boolean): void {
  const overlay = qs<HTMLElement>('#play-overlay')
  if (overlay) {
    overlay.classList.toggle('hidden', !visible)
  }
}

function resetConsole(): void {
  const consoleTarget = qs<HTMLElement>('#console-output')
  if (consoleTarget) {
    consoleTarget.textContent = ''
  }
}

function appendConsole(text: string, isError: boolean): void {
  const consoleTarget = qs<HTMLElement>('#console-output')
  if (!consoleTarget) {
    return
  }

  const line = document.createElement('div')
  line.textContent = text
  if (isError) {
    line.style.color = '#8a0000'
  }
  consoleTarget.appendChild(line)
  consoleTarget.scrollTop = consoleTarget.scrollHeight
}

function createCanvas(): HTMLCanvasElement {
  const host = qs<HTMLElement>('#soluna-stage-host')
  if (!host) {
    throw new Error('Missing #soluna-stage-host.')
  }

  host.replaceChildren()
  const canvas = document.createElement('canvas')
  canvas.id = 'soluna-canvas'
  host.appendChild(canvas)
  return canvas
}

function setupCanvasResize(canvas: HTMLCanvasElement): () => void {
  const resize = () => {
    const rect = canvas.getBoundingClientRect()
    const ratio = window.devicePixelRatio || 1
    canvas.width = Math.max(1, Math.floor(rect.width * ratio))
    canvas.height = Math.max(1, Math.floor(rect.height * ratio))
  }
  resize()
  return resize
}

function buildMainGame(): string {
  return [
    'entry : main.lua',
    'high_dpi : true',
    'text_sampler :',
    '  min_filter : linear',
    '  mag_filter : linear',
    'extlua_entry : extlua_init',
    'extlua_preload : sample',
    '',
  ].join('\n')
}

async function destroyActiveRuntime(): Promise<void> {
  const runtime = window.SOLUNA_PLAY_ACTIVE
  window.SOLUNA_PLAY_ACTIVE = undefined
  runtime?.stop()
}

async function loadAppFactory(basePath: string): Promise<PlayAppOptions['appFactory']> {
  const runtimeUrl = new URL(`${basePath}runtime/soluna.js`, window.location.href).href
  const runtimeApi = await import(/* @vite-ignore */ runtimeUrl)
  if (typeof runtimeApi.default !== 'function') {
    throw new TypeError('soluna.js does not export createApp.')
  }
  return runtimeApi.default as PlayAppOptions['appFactory']
}

async function ensureIsolation(basePath: string): Promise<boolean> {
  if (window.crossOriginIsolated) {
    return true
  }
  if (!('serviceWorker' in navigator)) {
    setStatus('Cross-origin isolation required.')
    setNote('Service worker is unavailable on this browser.')
    return false
  }

  try {
    const isolated = await ensureCrossOriginIsolation(`${basePath}coi-serviceworker.min.js`)
    if (!isolated) {
      setStatus('Reloading for cross-origin isolation...')
    }
    return isolated
  }
  catch (error) {
    setStatus('Failed to register COI service worker.')
    setNote(error instanceof Error ? error.message : String(error))
    return false
  }
}

async function loadRuntimeAssets(basePath: string, exampleSource: string) {
  setStatus('Preparing assets...')

  const assetBuffer = normalizeFileData(await fetchArrayBuffer(`${basePath}runtime/asset.zip`))
  const sampleWasmPromise = fetchArrayBuffer(`${basePath}runtime/sample.wasm`)

  setStatus('Preparing fonts...')
  const fontEntries = {
    'asset/font/arial.ttf': normalizeFileData(await fetchArrayBuffer(`${basePath}fonts/arial.ttf`)),
    'asset/font/SourceHanSansSC-Regular.ttf': normalizeFileData(
      await fetchArrayBuffer(`${basePath}fonts/SourceHanSansSC-Regular.ttf`),
    ),
  }

  return {
    assetBuffer,
    fontZip: zipSync(fontEntries),
    mainZip: zipSync({
      'main.lua': strToU8(exampleSource),
      'main.game': strToU8(buildMainGame()),
    }),
    sampleWasmBuffer: normalizeFileData(await sampleWasmPromise),
  }
}

async function startRuntime(
  createApp: PlayAppOptions['appFactory'],
  basePath: string,
  canvas: HTMLCanvasElement,
  assets: Awaited<ReturnType<typeof loadRuntimeAssets>>,
) {
  setStatus('Starting Soluna app...')

  return createRuntimeHandle(
    {
      appFactory: createApp,
      appBaseUrl: `${basePath}runtime/`,
      canvas,
      print(text) {
        appendConsole(String(text || ''), false)
      },
      printErr(text) {
        appendConsole(String(text || ''), true)
      },
      onAbort(reason) {
        setStatus('Runtime aborted.')
        setNote(String(reason || 'Unknown error'))
      },
    },
    {
      arguments: [
        'zipfile=/data/main.zip:/data/asset.zip:/data/font.zip',
        'cpath=/data/?.wasm',
      ],
      files: [
        { path: '/data/asset.zip', data: assets.assetBuffer, canOwn: true },
        { path: '/data/main.zip', data: assets.mainZip, canOwn: true },
        { path: '/data/font.zip', data: assets.fontZip, canOwn: true },
        { path: '/data/sample.wasm', data: assets.sampleWasmBuffer, canOwn: true },
      ],
    },
  )
}

export default async function initPlay(options: PlayOptions): Promise<void> {
  const basePath = import.meta.env.BASE_URL
  const codeTarget = qs<HTMLElement>('#code-content')
  if (codeTarget) {
    codeTarget.textContent = options.exampleSource
  }

  setOverlayVisible(true)
  setStatus('Loading example source...')
  setNote('')
  resetConsole()
  await destroyActiveRuntime()

  let createApp: PlayAppOptions['appFactory']
  try {
    createApp = await loadAppFactory(basePath)
  }
  catch (error) {
    setStatus('Failed to load soluna.js.')
    setNote(error instanceof Error ? error.message : String(error))
    return
  }

  if (!(await ensureIsolation(basePath))) {
    return
  }

  let assets: Awaited<ReturnType<typeof loadRuntimeAssets>>
  try {
    assets = await loadRuntimeAssets(basePath, options.exampleSource)
  }
  catch (error) {
    const message = error instanceof Error ? error.message : String(error)
    if (message.includes('sample.wasm')) {
      setStatus('Failed to load external module sample.wasm.')
    }
    else if (message.includes('/fonts/')) {
      setStatus('Failed to load font assets.')
    }
    else if (message.includes('asset.zip')) {
      setStatus('Failed to load asset archive.')
    }
    else {
      setStatus('Failed to prepare runtime assets.')
    }
    setNote(message)
    return
  }

  const canvas = createCanvas()
  const resizeHandler = setupCanvasResize(canvas)
  window.addEventListener('resize', resizeHandler)

  try {
    const runtime = await startRuntime(createApp, basePath, canvas, assets)
    window.SOLUNA_PLAY_ACTIVE = {
      stop() {
        window.removeEventListener('resize', resizeHandler)
        runtime.stop()
        canvas.remove()
      },
    }
    setOverlayVisible(false)
  }
  catch (error) {
    window.removeEventListener('resize', resizeHandler)
    canvas.remove()
    setStatus('Failed to start runtime.')
    setNote(error instanceof Error ? error.message : String(error))
  }
}
