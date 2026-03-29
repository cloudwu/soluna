if (!window.SOLUNA_PLAY_LOADED) {
  window.SOLUNA_PLAY_LOADED = true;
  (function () {
    const textEncoder = new TextEncoder();
    const qs = (selector, root = document) => root.querySelector(selector);

    function asArray(value) {
      if (value == null) {
        return [];
      }
      return Array.isArray(value) ? value : [value];
    }

    function normalizeBaseUrl(baseUrl) {
      if (!baseUrl) {
        return new URL("./", window.location.href);
      }
      const value = String(baseUrl);
      const normalized = value.endsWith("/") ? value : `${value}/`;
      return new URL(normalized, window.location.href);
    }

    function normalizeFileData(data) {
      if (data instanceof Uint8Array) {
        return data;
      }
      if (data instanceof ArrayBuffer) {
        return new Uint8Array(data);
      }
      if (ArrayBuffer.isView(data)) {
        return new Uint8Array(data.buffer, data.byteOffset, data.byteLength);
      }
      if (typeof data === "string") {
        return textEncoder.encode(data);
      }
      throw new TypeError("Unsupported file data type.");
    }

    function ensureAbsolutePath(path) {
      if (typeof path !== "string" || path.length === 0 || !path.startsWith("/")) {
        throw new TypeError(`Expected an absolute FS path, got: ${path}`);
      }
      return path;
    }

    function dirname(path) {
      const normalized = ensureAbsolutePath(path);
      const index = normalized.lastIndexOf("/");
      if (index <= 0) {
        return "/";
      }
      return normalized.slice(0, index);
    }

    function ensureParentDirectory(runtimeModule, path) {
      const dir = dirname(path);
      if (dir === "/") {
        return;
      }
      runtimeModule.FS_createPath("/", dir.slice(1), true, true);
    }

    function createFile(path, data, options = {}) {
      return {
        path: ensureAbsolutePath(path),
        data: normalizeFileData(data),
        canOwn: options.canOwn !== false,
      };
    }

    async function fetchText(url, init) {
      const response = await fetch(url, init);
      if (!response.ok) {
        throw new Error(`Failed to load ${url}`);
      }
      return response.text();
    }

    async function fetchArrayBuffer(url, init) {
      const response = await fetch(url, init);
      if (!response.ok) {
        throw new Error(`Failed to load ${url}`);
      }
      return response.arrayBuffer();
    }

    async function ensureCrossOriginIsolation(options = {}) {
      const {
        serviceWorkerUrl,
        reload = () => window.location.reload(),
      } = options;

      if (window.crossOriginIsolated) {
        return true;
      }
      if (!("serviceWorker" in navigator)) {
        return false;
      }

      await navigator.serviceWorker.register(serviceWorkerUrl);
      if (!navigator.serviceWorker.controller) {
        reload();
        return false;
      }
      return true;
    }

    function createZip(entries) {
      const crcTable = createZip.crcTable || (createZip.crcTable = (() => {
        const table = new Uint32Array(256);
        for (let n = 0; n < 256; n++) {
          let c = n;
          for (let k = 0; k < 8; k++) {
            c = (c & 1) ? (0xedb88320 ^ (c >>> 1)) : (c >>> 1);
          }
          table[n] = c >>> 0;
        }
        return table;
      })());

      const calculateCRC32 = (data) => {
        let crc = 0xffffffff;
        for (let i = 0; i < data.length; i++) {
          crc = crcTable[(crc ^ data[i]) & 0xff] ^ (crc >>> 8);
        }
        return (crc ^ 0xffffffff) >>> 0;
      };

      const files = entries.map((entry) => {
        const nameBytes = textEncoder.encode(entry.name);
        const data = normalizeFileData(entry.data);
        return {
          nameBytes,
          data,
          nameLength: nameBytes.length,
          size: data.length,
          crc32: calculateCRC32(data),
          offset: 0,
        };
      });

      let localSize = 0;
      let centralSize = 0;
      files.forEach((file) => {
        localSize += 30 + file.nameLength + file.size;
        centralSize += 46 + file.nameLength;
      });

      const totalSize = localSize + centralSize + 22;
      const output = new Uint8Array(totalSize);
      const view = new DataView(output.buffer);

      let offset = 0;
      files.forEach((file) => {
        file.offset = offset;
        view.setUint32(offset, 0x04034b50, true);
        view.setUint16(offset + 4, 20, true);
        view.setUint16(offset + 6, 0, true);
        view.setUint16(offset + 8, 0, true);
        view.setUint32(offset + 14, file.crc32, true);
        view.setUint32(offset + 18, file.size, true);
        view.setUint32(offset + 22, file.size, true);
        view.setUint16(offset + 26, file.nameLength, true);
        offset += 30;
        output.set(file.nameBytes, offset);
        offset += file.nameLength;
        output.set(file.data, offset);
        offset += file.size;
      });

      const centralDirectoryOffset = offset;
      files.forEach((file) => {
        view.setUint32(offset, 0x02014b50, true);
        view.setUint16(offset + 4, 20, true);
        view.setUint16(offset + 6, 20, true);
        view.setUint16(offset + 8, 0, true);
        view.setUint16(offset + 10, 0, true);
        view.setUint32(offset + 16, file.crc32, true);
        view.setUint32(offset + 20, file.size, true);
        view.setUint32(offset + 24, file.size, true);
        view.setUint16(offset + 28, file.nameLength, true);
        view.setUint32(offset + 42, file.offset, true);
        offset += 46;
        output.set(file.nameBytes, offset);
        offset += file.nameLength;
      });

      const centralDirectorySize = offset - centralDirectoryOffset;
      view.setUint32(offset, 0x06054b50, true);
      view.setUint16(offset + 4, 0, true);
      view.setUint16(offset + 6, 0, true);
      view.setUint16(offset + 8, files.length, true);
      view.setUint16(offset + 10, files.length, true);
      view.setUint32(offset + 12, centralDirectorySize, true);
      view.setUint32(offset + 16, centralDirectoryOffset, true);
      view.setUint16(offset + 20, 0, true);

      return output;
    }

    function createWorkerTracker() {
      const OriginalWorker = globalThis.Worker;
      const workers = new Set();

      class TrackingWorker extends OriginalWorker {
        constructor(url, options) {
          super(url, options);
          if (options && typeof options.name === "string" && options.name.startsWith("em-pthread-")) {
            workers.add(this);
          }
        }
      }

      return {
        install() {
          if (globalThis.Worker === OriginalWorker) {
            globalThis.Worker = TrackingWorker;
          }
        },
        uninstall() {
          if (globalThis.Worker === TrackingWorker) {
            globalThis.Worker = OriginalWorker;
          }
        },
        terminateAll() {
          for (const worker of workers) {
            try {
              worker.terminate();
            } catch (_error) {
            }
          }
          workers.clear();
        },
      };
    }

    class PlayApp {
      constructor(options = {}) {
        const {
          appFactory,
          appBaseUrl,
          canvas = null,
          print = (text) => console.log(text),
          printErr = (text) => console.error(text),
          onAbort = null,
          moduleOverrides = {},
        } = options;

        if (typeof appFactory !== "function") {
          throw new TypeError("appFactory must be a function.");
        }

        this.appFactory = appFactory;
        this.appBaseUrl = appBaseUrl;
        this.canvas = canvas;
        this.print = print;
        this.printErr = printErr;
        this.onAbort = onAbort;
        this.moduleOverrides = moduleOverrides;
        this.instance = null;
        this.stopped = false;
        this.workerTracker = createWorkerTracker();
      }

      async start(options = {}) {
        if (this.instance) {
          throw new Error("PlayApp.start() can only be called once per instance.");
        }
        if (this.stopped) {
          throw new Error("PlayApp has already been stopped.");
        }

        const appBaseUrl = normalizeBaseUrl(this.appBaseUrl);
        const files = asArray(options.files).map((file) => createFile(file.path, file.data, file));
        const preRunHooks = asArray(options.preRun);
        const moduleOverrides = {
          ...this.moduleOverrides,
          ...(options.moduleOverrides || {}),
        };

        this.workerTracker.install();

        let instance;
        try {
          instance = await this.appFactory({
            ...moduleOverrides,
            arguments: asArray(options.arguments),
            canvas: this.canvas,
            print: this.print,
            printErr: this.printErr,
            locateFile(path) {
              return new URL(path, appBaseUrl).toString();
            },
            preRun: [
              (runtimeModule) => {
                files.forEach((file) => {
                  ensureParentDirectory(runtimeModule, file.path);
                  runtimeModule.FS.writeFile(file.path, file.data, { canOwn: file.canOwn });
                });
              },
              ...preRunHooks,
            ],
            onAbort: (reason) => {
              if (typeof this.onAbort === "function") {
                this.onAbort(reason);
              }
            },
          });
        } catch (error) {
          this.workerTracker.terminateAll();
          this.workerTracker.uninstall();
          throw error;
        }

        if (typeof instance.quitApp !== "function" && typeof instance._soluna_runtime_quit === "function") {
          instance.quitApp = () => {
            instance._soluna_runtime_quit();
          };
        }

        this.instance = instance;
        return instance;
      }

      quitApp(instance = this.instance) {
        if (!instance) {
          return;
        }

        try {
          if (typeof instance.quitApp === "function") {
            instance.quitApp();
          }
        } catch (_error) {
        }
      }

      stop() {
        if (this.stopped) {
          return;
        }
        this.stopped = true;

        const instance = this.instance;
        this.instance = null;
        if (!instance) {
          return;
        }

        this.quitApp(instance);
        this.workerTracker.terminateAll();
        this.workerTracker.uninstall();
      }
    }

    let activeExampleId = null;
    let activeRuntime = null;
    let activeRunId = 0;

    function inferBaseFromPath(pathname) {
      const path = pathname || "/";
      const trimmed = path.replace(/\/index\.html$/, "").replace(/\/$/, "");
      if (trimmed === "") return "";
      const segments = trimmed.split("/").filter(Boolean);
      if (segments.length === 0) return "";
      if (segments.length === 1) return `/${segments[0]}`;
      const last = segments[segments.length - 1];
      const penultimate = segments[segments.length - 2];
      if (["examples", "docs"].includes(last)) {
        const base = segments.slice(0, -1).join("/");
        return base ? `/${base}` : "";
      }
      if (["examples", "docs"].includes(penultimate)) {
        const base = segments.slice(0, -2).join("/");
        return base ? `/${base}` : "";
      }
      return `/${segments.join("/")}`;
    }

    function getBasePath() {
      return inferBaseFromPath(window.location.pathname);
    }

    function getExampleId() {
      const path = window.location.pathname || "/";
      const trimmed = path.replace(/\/index\.html$/, "").replace(/\/$/, "");
      if (trimmed === "") return null;
      const segments = trimmed.split("/").filter(Boolean);
      if (segments.length === 0) return null;
      const last = segments[segments.length - 1];
      if (last === "examples") return null;
      return last;
    }

    function setStatus(text) {
      const status = qs("#play-status");
      if (status) status.textContent = text;
    }

    function setNote(text) {
      const note = qs("#play-note");
      if (note) note.textContent = text || "";
    }

    function setOverlayVisible(visible) {
      const overlay = qs("#play-overlay");
      if (!overlay) return;
      overlay.classList.toggle("hidden", !visible);
    }

    function resetConsole() {
      const consoleTarget = qs("#console-output");
      if (consoleTarget) consoleTarget.textContent = "";
    }

    function appendConsole(text, isError) {
      const consoleTarget = qs("#console-output");
      if (!consoleTarget) return;
      const line = document.createElement("div");
      line.textContent = text;
      if (isError) {
        line.style.color = "#8a0000";
      }
      consoleTarget.appendChild(line);
      consoleTarget.scrollTop = consoleTarget.scrollHeight;
    }

    function createCanvas() {
      const host = qs("#soluna-stage-host");
      if (!host) {
        throw new Error("Missing #soluna-stage-host");
      }
      host.replaceChildren();
      const canvas = document.createElement("canvas");
      canvas.id = "soluna-canvas";
      host.appendChild(canvas);
      return canvas;
    }

    function setupCanvasResize(canvas) {
      const resize = () => {
        const rect = canvas.getBoundingClientRect();
        const ratio = window.devicePixelRatio || 1;
        canvas.width = Math.max(1, Math.floor(rect.width * ratio));
        canvas.height = Math.max(1, Math.floor(rect.height * ratio));
      };
      resize();
      return resize;
    }

    function buildMainGame() {
      return [
        "entry : main.lua",
        "high_dpi : true",
        "text_sampler :",
        "  min_filter : linear",
        "  mag_filter : linear",
        "extlua_entry : extlua_init",
        "extlua_preload : sample",
        "",
      ].join("\n");
    }

    async function destroyActiveRuntime() {
      const runtime = activeRuntime;
      activeRuntime = null;
      if (!runtime) {
        return;
      }

      if (runtime.resizeHandler) {
        window.removeEventListener("resize", runtime.resizeHandler);
      }
      if (runtime.runtime) {
        runtime.runtime.stop();
      }
      if (runtime.canvas && runtime.canvas.parentNode) {
        runtime.canvas.remove();
      }
    }

    async function loadAppFactory(base) {
      const runtimeApi = await import(`${base}/runtime/soluna.js`);
      return runtimeApi.default;
    }

    async function loadExampleSource(base, exampleId) {
      setStatus("Loading example source...");
      return fetchText(`${base}/runtime/test/${exampleId}.lua`);
    }

    async function ensureIsolation(base) {
      if (window.crossOriginIsolated) return true;
      if (!("serviceWorker" in navigator)) {
        setStatus("Cross-origin isolation required.");
        setNote("Service worker is unavailable on this browser.");
        return false;
      }
      try {
        const isolated = await ensureCrossOriginIsolation({
          serviceWorkerUrl: `${base}/coi-serviceworker.min.js`,
        });
        if (!isolated) {
          setStatus("Reloading for cross-origin isolation...");
        }
        return isolated;
      } catch (error) {
        setStatus("Failed to register COI service worker.");
        setNote(error.message);
        return false;
      }
    }

    async function loadRuntimeAssets(base, sourceText) {
      setStatus("Preparing assets...");
      const assetBuffer = await fetchArrayBuffer(`${base}/runtime/asset.zip`);
      const sampleWasmPromise = fetchArrayBuffer(`${base}/runtime/sample.wasm`);

      setStatus("Preparing fonts...");
      const fontEntries = [];
      const fontFiles = [
        { url: `${base}/fonts/arial.ttf`, name: "asset/font/arial.ttf" },
        { url: `${base}/fonts/SourceHanSansSC-Regular.ttf`, name: "asset/font/SourceHanSansSC-Regular.ttf" },
      ];
      for (const file of fontFiles) {
        const data = await fetchArrayBuffer(file.url);
        fontEntries.push({ name: file.name, data });
      }

      return {
        assetBuffer,
        fontZip: createZip(fontEntries),
        mainZip: createZip([
          { name: "main.lua", data: textEncoder.encode(sourceText) },
          { name: "main.game", data: textEncoder.encode(buildMainGame()) },
        ]),
        sampleWasmBuffer: await sampleWasmPromise,
      };
    }

    async function startRuntime(createApp, base, canvas, assets) {
      setStatus("Starting Soluna app...");

      const runtime = new PlayApp({
        appFactory: createApp,
        appBaseUrl: `${base}/runtime/`,
        canvas,
        print(text) {
          appendConsole(String(text || ""), false);
        },
        printErr(text) {
          appendConsole(String(text || ""), true);
        },
        onAbort(reason) {
          setStatus("Runtime aborted.");
          setNote(String(reason || "Unknown error"));
        },
      });

      await runtime.start({
        arguments: [
          "zipfile=/data/main.zip:/data/asset.zip:/data/font.zip",
          "cpath=/data/?.wasm",
        ],
        files: [
          { path: "/data/asset.zip", data: assets.assetBuffer },
          { path: "/data/main.zip", data: assets.mainZip },
          { path: "/data/font.zip", data: assets.fontZip },
          { path: "/data/sample.wasm", data: assets.sampleWasmBuffer },
        ],
      });

      return runtime;
    }

    async function initPlay() {
      const exampleId = getExampleId();
      const runId = ++activeRunId;

      if (!exampleId) {
        activeExampleId = null;
        await destroyActiveRuntime();
        return;
      }
      if (exampleId === activeExampleId && activeRuntime) {
        return;
      }

      activeExampleId = exampleId;
      const base = getBasePath();

      setOverlayVisible(true);
      setStatus("Loading example source...");
      setNote("");
      resetConsole();
      await destroyActiveRuntime();

      let createApp;
      try {
        createApp = await loadAppFactory(base);
      } catch (error) {
        if (runId !== activeRunId) return;
        setStatus("Failed to load soluna.js.");
        setNote(error.message);
        return;
      }

      const codeTarget = qs("#code-content");
      let sourceText;
      try {
        sourceText = await loadExampleSource(base, exampleId);
        if (codeTarget) {
          codeTarget.textContent = sourceText;
        }
      } catch (error) {
        if (runId !== activeRunId) return;
        setStatus("Failed to load example source.");
        setNote(error.message);
        return;
      }

      if (!(await ensureIsolation(base))) {
        return;
      }
      if (runId !== activeRunId) return;

      let assets;
      try {
        assets = await loadRuntimeAssets(base, sourceText);
      } catch (error) {
        if (runId !== activeRunId) return;
        if (String(error.message || "").includes("sample.wasm")) {
          setStatus("Failed to load external module sample.wasm.");
        } else if (String(error.message || "").includes("/fonts/")) {
          setStatus("Failed to load font assets.");
        } else if (String(error.message || "").includes("asset.zip")) {
          setStatus("Failed to load asset archive.");
        } else {
          setStatus("Failed to prepare runtime assets.");
        }
        setNote(error.message);
        return;
      }

      const canvas = createCanvas();
      const resizeHandler = setupCanvasResize(canvas);
      window.addEventListener("resize", resizeHandler);

      try {
        const runtime = await startRuntime(createApp, base, canvas, assets);
        if (runId !== activeRunId) {
          runtime.stop();
          canvas.remove();
          window.removeEventListener("resize", resizeHandler);
          return;
        }
        activeRuntime = {
          runtime,
          canvas,
          resizeHandler,
        };
        setOverlayVisible(false);
      } catch (error) {
        window.removeEventListener("resize", resizeHandler);
        canvas.remove();
        if (runId !== activeRunId) return;
        setStatus("Failed to start runtime.");
        setNote(error.message);
      }
    }

    const bootPlay = () => {
      initPlay();
    };

    if (!window.SOLUNA_PLAY_BOUND) {
      window.SOLUNA_PLAY_BOUND = true;
      document.addEventListener("DOMContentLoaded", bootPlay);
      if (document.body) {
        document.body.addEventListener("htmx:afterSwap", bootPlay);
      }
    }

    if (document.readyState !== "loading") {
      bootPlay();
    }
  })();
}
