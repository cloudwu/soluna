if (!window.SOLUNA_PLAY_LOADED) {
  window.SOLUNA_PLAY_LOADED = true;
  (function () {
    const qs = (selector, root = document) => root.querySelector(selector);

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

    async function loadText(url) {
      const response = await fetch(url);
      if (!response.ok) {
        throw new Error(`Failed to load ${url}`);
      }
      return response.text();
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
      if (visible) {
        overlay.classList.remove("hidden");
      } else {
        overlay.classList.add("hidden");
      }
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

    async function ensureIsolation(base) {
      if (window.crossOriginIsolated) return true;
      if (!("serviceWorker" in navigator)) {
        setStatus("Cross-origin isolation required.");
        setNote("Service worker is unavailable on this browser.");
        return false;
      }
      try {
        await navigator.serviceWorker.register(`${base}/coi-serviceworker.min.js`);
        if (!navigator.serviceWorker.controller) {
          setStatus("Reloading for cross-origin isolation...");
          window.location.reload();
          return false;
        }
      } catch (err) {
        setStatus("Failed to register COI service worker.");
        setNote(err.message);
        return false;
      }
      return true;
    }

    function updateFrame(exampleId, base) {
      const frame = qs("#soluna-frame");
      if (!frame) return;
      const params = new URLSearchParams({ example: exampleId, base });
      const src = `${base}/playframe/?${params.toString()}`;
      if (frame.getAttribute("src") !== src) {
        frame.setAttribute("src", src);
      }
    }

    let activeExampleId = null;

    function handleMessage(event) {
      if (event.origin !== window.location.origin) return;
      const data = event.data;
      if (!data || typeof data !== "object") return;
      switch (data.type) {
        case "console":
          appendConsole(String(data.text || ""), Boolean(data.isError));
          break;
        case "status":
          setStatus(String(data.text || ""));
          break;
        case "note":
          setNote(String(data.text || ""));
          break;
        case "ready":
          setOverlayVisible(false);
          break;
        default:
          break;
      }
    }

    async function initPlay() {
      const exampleId = getExampleId();
      if (!exampleId) {
        activeExampleId = null;
        return;
      }
      if (exampleId === activeExampleId) return;
      activeExampleId = exampleId;
      const base = getBasePath();

      setOverlayVisible(true);
      setStatus("Loading example source...");
      setNote("");
      resetConsole();

      const codeTarget = qs("#code-content");
      try {
        const sourceText = await loadText(`${base}/runtime/test/${exampleId}.lua`);
        if (codeTarget) codeTarget.textContent = sourceText;
      } catch (err) {
        setStatus("Failed to load example source.");
        setNote(err.message);
        return;
      }

      const isolated = await ensureIsolation(base);
      if (!isolated) return;
      setStatus("Loading runtime...");
      updateFrame(exampleId, base);
    }

    const bootPlay = () => {
      initPlay();
    };

    if (!window.SOLUNA_PLAY_BOUND) {
      window.SOLUNA_PLAY_BOUND = true;
      window.addEventListener("message", handleMessage);
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
