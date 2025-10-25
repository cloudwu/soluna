mergeInto(LibraryManager.library, {
  soluna_wasm_setup_ime__deps: ['$withStackSave', '$lengthBytesUTF8', '$stringToUTF8', '$stackAlloc'],
  soluna_wasm_setup_ime: function () {
    if (Module.solunaIme) {
      return;
    }
    if (typeof document === 'undefined' || !document.body) {
      return;
    }
    var globalScope = (typeof window !== 'undefined') ? window : self;
    var ta = document.createElement('textarea');
    ta.setAttribute('autocapitalize', 'off');
    ta.setAttribute('autocomplete', 'off');
    ta.setAttribute('autocorrect', 'off');
    ta.setAttribute('spellcheck', 'false');
    ta.setAttribute('tabindex', '-1');
    ta.style.position = 'absolute';
    ta.style.opacity = '0';
    ta.style.pointerEvents = 'none';
    ta.style.zIndex = '2147483647';
    ta.style.resize = 'none';
    ta.style.overflow = 'hidden';
    ta.style.border = '0';
    ta.style.margin = '0';
    ta.style.padding = '0';
    ta.style.background = 'transparent';
    ta.style.color = 'transparent';
    ta.style.whiteSpace = 'pre';
    ta.style.width = '1px';
    ta.style.height = '1px';
    ta.style.left = '-10000px';
    ta.style.top = '-10000px';
    ta.style.display = 'none';
    document.body.appendChild(ta);

    var callSetComposing = function (flag) {
      if (Module._soluna_wasm_set_composing) {
        Module._soluna_wasm_set_composing(flag | 0);
      }
    };
    Module.solunaSetComposing = callSetComposing;

    var state = {
      node: ta,
      active: false,
      composing: false,
      expectNextInput: false,
      suppressNextInput: false,
      mods: 0,
      rect: { x: 0, y: 0, w: 1, h: 1 }
    };

    state.updateModsFromEvent = function (ev) {
      var mods = 0;
      if (ev && ev.shiftKey) { mods |= 1; }
      if (ev && ev.ctrlKey) { mods |= 2; }
      if (ev && ev.altKey) { mods |= 4; }
      if (ev && ev.metaKey) { mods |= 8; }
      state.mods = mods;
    };

    state.commitText = function (text) {
      if (!text || text.length === 0) {
        return;
      }
      var mods = state.mods;
      withStackSave(function () {
        var len = lengthBytesUTF8(text) + 1;
        var ptr = stackAlloc(len);
        stringToUTF8(text, ptr, len);
        if (Module._soluna_wasm_ime_commit) {
          Module._soluna_wasm_ime_commit(ptr, mods);
        }
      });
    };

    ta.addEventListener('compositionstart', function (ev) {
      state.composing = true;
      state.expectNextInput = false;
      state.suppressNextInput = false;
      state.updateModsFromEvent(ev);
      if (Module.solunaSetComposing) {
        Module.solunaSetComposing(1);
      }
    });

    ta.addEventListener('compositionupdate', function (ev) {
      state.updateModsFromEvent(ev);
    });

    ta.addEventListener('compositionend', function (ev) {
      state.composing = false;
      state.updateModsFromEvent(ev);
      var text = (ev && ev.data) ? ev.data : '';
      if (text.length > 0) {
        state.commitText(text);
        state.suppressNextInput = true;
      } else {
        state.expectNextInput = true;
      }
      state.node.value = '';
      if (Module.solunaSetComposing) {
        Module.solunaSetComposing(0);
      }
    });

    ta.addEventListener('input', function (ev) {
      if (!state.active) {
        return;
      }
      if (state.composing) {
        return;
      }
      if (state.suppressNextInput) {
        state.suppressNextInput = false;
        state.node.value = '';
        return;
      }
      if (!state.expectNextInput) {
        return;
      }
      var text = (ev && typeof ev.data === 'string') ? ev.data : state.node.value;
      if (text && text.length > 0) {
        state.commitText(text);
      }
      state.expectNextInput = false;
      state.node.value = '';
    });

    var updateMods = function (ev) {
      state.updateModsFromEvent(ev);
    };

    globalScope.addEventListener('keydown', updateMods, true);
    globalScope.addEventListener('keyup', updateMods, true);
    globalScope.addEventListener('blur', function () {
      state.mods = 0;
      state.composing = false;
      state.expectNextInput = false;
      state.suppressNextInput = false;
      state.node.value = '';
      if (Module.solunaSetComposing) {
        Module.solunaSetComposing(0);
      }
    });

    Module.solunaIme = state;
  },

  soluna_wasm_dom_show: function (x, y, w, h) {
    var state = Module.solunaIme;
    if (!state || typeof document === 'undefined') {
      return;
    }
    var selector = Module.solunaCanvasSelector || '#canvas';
    var canvas = Module.canvas || document.querySelector(selector) || document.querySelector('canvas');
    if (!canvas) {
      return;
    }
    var rect = canvas.getBoundingClientRect();
    var canvasLeft = rect.left + window.scrollX;
    var canvasTop = rect.top + window.scrollY;
    var left = canvasLeft + x;
    var top = canvasTop + y;
    var width = Math.max(1, Number.isFinite(w) ? w : 1);
    var height = Math.max(1, (Number.isFinite(h) && h > 0) ? h : 16);

    var el = state.node;
    el.style.display = 'block';
    el.style.left = left + 'px';
    el.style.top = top + 'px';
    el.style.width = width + 'px';
    el.style.height = height + 'px';
    el.style.lineHeight = height + 'px';
    if (!state.active) {
      el.value = '';
    }
    state.active = true;
    try {
      el.focus({ preventScroll: true });
    } catch (err) {
      el.focus();
    }
  },

  soluna_wasm_dom_hide: function () {
    var state = Module.solunaIme;
    if (!state || typeof document === 'undefined') {
      return;
    }
    if (!state.active) {
      return;
    }
    state.active = false;
    state.composing = false;
    state.expectNextInput = false;
    state.suppressNextInput = false;
    var el = state.node;
    el.value = '';
    el.style.display = 'none';
    el.blur();
    if (Module.solunaSetComposing) {
      Module.solunaSetComposing(0);
    }
  }
});
