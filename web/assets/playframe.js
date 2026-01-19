(function () {
  const qs = (selector, root = document) => root.querySelector(selector);

  function inferBaseFromReferrer() {
    if (!document.referrer) return "";
    try {
      const ref = new URL(document.referrer);
      return inferBaseFromPath(ref.pathname);
    } catch (err) {
      return "";
    }
  }

  function inferBaseFromPath(pathname) {
    const path = pathname || "/";
    const trimmed = path.replace(/\/index\.html$/, "").replace(/\/$/, "");
    if (trimmed === "") return "";
    const segments = trimmed.split("/").filter(Boolean);
    if (segments.length === 0) return "";
    const exampleIndex = segments.indexOf("examples");
    if (exampleIndex !== -1) {
      const base = segments.slice(0, exampleIndex).join("/");
      return base ? `/${base}` : "";
    }
    if (segments[segments.length - 1] === "playframe") {
      const base = segments.slice(0, -1).join("/");
      return base ? `/${base}` : "";
    }
    return `/${segments.join("/")}`;
  }

  function getBasePath() {
    const params = new URLSearchParams(window.location.search);
    const baseParam = params.get("base");
    if (baseParam) {
      const trimmed = baseParam.replace(/\/$/, "");
      if (trimmed.startsWith("/")) {
        return trimmed;
      }
    }
    const refBase = inferBaseFromReferrer();
    if (refBase) return refBase;
    return inferBaseFromPath(window.location.pathname);
  }

  function getExampleId() {
    const params = new URLSearchParams(window.location.search);
    const value = params.get("example");
    if (!value) return null;
    const trimmed = value.trim();
    return trimmed.length > 0 ? trimmed : null;
  }

  function postMessage(type, payload) {
    if (!window.parent || window.parent === window) return;
    const message = Object.assign({ type }, payload || {});
    window.parent.postMessage(message, window.location.origin);
  }

  function setStatus(text) {
    postMessage("status", { text });
  }

  function setNote(text) {
    postMessage("note", { text: text || "" });
  }

  function appendConsole(text, isError) {
    postMessage("console", { text, isError: Boolean(isError) });
  }

  function createZip(entries) {
    const CRC32_TABLE = createZip.crcTable || (createZip.crcTable = (() => {
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
        crc = CRC32_TABLE[(crc ^ data[i]) & 0xff] ^ (crc >>> 8);
      }
      return (crc ^ 0xffffffff) >>> 0;
    };

    const files = entries.map((entry) => {
      const nameBytes = new TextEncoder().encode(entry.name);
      const data = entry.data;
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

  async function loadText(url) {
    const response = await fetch(url);
    if (!response.ok) {
      throw new Error(`Failed to load ${url}`);
    }
    return response.text();
  }

  async function loadBuffer(url) {
    const response = await fetch(url);
    if (!response.ok) {
      throw new Error(`Failed to load ${url}`);
    }
    return response.arrayBuffer();
  }

  function loadRuntimeScript(src) {
    return new Promise((resolve, reject) => {
      const script = document.createElement("script");
      script.src = src;
      script.onload = () => resolve();
      script.onerror = () => reject(new Error(`Failed to load ${src}`));
      document.head.appendChild(script);
    });
  }

  let resizeHandler = null;

  function setupCanvasResize(canvas) {
    if (!canvas) return;
    if (resizeHandler) {
      window.removeEventListener("resize", resizeHandler);
    }
    resizeHandler = () => {
      const rect = canvas.getBoundingClientRect();
      const ratio = window.devicePixelRatio || 1;
      canvas.width = Math.max(1, Math.floor(rect.width * ratio));
      canvas.height = Math.max(1, Math.floor(rect.height * ratio));
    };
    resizeHandler();
    window.addEventListener("resize", resizeHandler);
  }

  async function initPlay() {
    const exampleId = getExampleId();
    if (!exampleId) return;
    const base = getBasePath();

    setStatus("Loading example source...");
    const canvas = qs("#soluna-canvas");
    setupCanvasResize(canvas);

    let sourceText = "";
    try {
      sourceText = await loadText(`${base}/runtime/test/${exampleId}.lua`);
    } catch (err) {
      setStatus("Failed to load example source.");
      setNote(err.message);
      return;
    }

    if (!window.crossOriginIsolated && "serviceWorker" in navigator) {
      try {
        await navigator.serviceWorker.register(`${base}/coi-serviceworker.min.js`);
        if (!navigator.serviceWorker.controller) {
          setStatus("Reloading for cross-origin isolation...");
          window.location.reload();
          return;
        }
      } catch (err) {
        setStatus("Failed to register COI service worker.");
        setNote(err.message);
        return;
      }
    }

    setStatus("Preparing assets...");
    let assetBuffer;
    try {
      assetBuffer = await loadBuffer(`${base}/runtime/asset.zip`);
    } catch (err) {
      setStatus("Failed to load asset archive.");
      setNote(err.message);
      return;
    }

    const mainGame = "entry : main.lua\nhigh_dpi : true\n";
    const mainLuaBytes = new TextEncoder().encode(sourceText);
    const mainGameBytes = new TextEncoder().encode(mainGame);
    const mainZip = createZip([
      { name: "main.lua", data: mainLuaBytes },
      { name: "main.game", data: mainGameBytes },
    ]);

    setStatus("Starting Soluna runtime...");

    window.Module = {
      arguments: ["zipfile=/data/main.zip:/data/asset.zip"],
      canvas,
      print(text) {
        appendConsole(String(text || ""), false);
      },
      printErr(text) {
        appendConsole(String(text || ""), true);
      },
      locateFile(path) {
        if (path.endsWith(".wasm")) {
          return `${base}/runtime/soluna.wasm`;
        }
        return path;
      },
      preRun: [
        function () {
          Module.FS_createPath("/", "data", true, true);
          Module.addRunDependency("asset-zip");
          Module.addRunDependency("main-zip");
          Module.FS.writeFile("/data/asset.zip", new Uint8Array(assetBuffer), { canOwn: true });
          Module.FS.writeFile("/data/main.zip", mainZip, { canOwn: true });
          Module.removeRunDependency("asset-zip");
          Module.removeRunDependency("main-zip");
        },
      ],
      onAbort(reason) {
        setStatus("Runtime aborted.");
        setNote(String(reason || "Unknown error"));
      },
    };

    try {
      await loadRuntimeScript(`${base}/runtime/soluna.js`);
      postMessage("ready");
    } catch (err) {
      setStatus("Failed to load runtime.");
      setNote(err.message);
    }
  }

  if (document.readyState !== "loading") {
    initPlay();
  } else {
    document.addEventListener("DOMContentLoaded", initPlay);
  }
})();
