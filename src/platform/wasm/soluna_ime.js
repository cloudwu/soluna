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
    ta.style.color = '#000';
    ta.style.whiteSpace = 'pre';
    ta.style.width = '1px';
    ta.style.height = '1px';
    ta.style.left = '-10000px';
    ta.style.top = '-10000px';
    ta.style.display = 'none';
    document.body.appendChild(ta);

    var label = document.createElement('div');
    label.setAttribute('aria-hidden', 'true');
    label.style.position = 'absolute';
    label.style.pointerEvents = 'none';
    label.style.zIndex = '2147483647';
    label.style.whiteSpace = 'pre';
    label.style.margin = '0';
    label.style.padding = '0';
    label.style.border = '0';
    label.style.background = 'transparent';
    label.style.display = 'none';
    label.style.left = '-10000px';
    label.style.top = '-10000px';
    label.style.font = '16px sans-serif';
    document.body.appendChild(label);

    var callSetComposing = function (flag) {
      if (Module._soluna_wasm_set_composing) {
        Module._soluna_wasm_set_composing(flag | 0);
      }
    };
    Module.solunaSetComposing = callSetComposing;

    var state = {
      node: ta,
      preedit: label,
      preeditText: '',
      active: false,
      composing: false,
      expectNextInput: false,
      suppressNextInput: false,
      mods: 0,
      rect: { x: 0, y: 0, w: 1, h: 1 },
      customFont: null,
      customFontSize: 0
    };

    state.resolveCanvas = function () {
      if (Module.canvas) {
        return Module.canvas;
      }
      if (typeof document === 'undefined') {
        return null;
      }
      var selector = Module.solunaCanvasSelector || '#canvas';
      var canvas = null;
      try {
        canvas = document.querySelector(selector);
      } catch (err) {
        canvas = null;
      }
      if (!canvas) {
        canvas = document.querySelector('canvas');
      }
      return canvas || null;
    };

    state.updateModsFromEvent = function (ev) {
      var mods = 0;
      if (ev && ev.shiftKey) { mods |= 1; }
      if (ev && ev.ctrlKey) { mods |= 2; }
      if (ev && ev.altKey) { mods |= 4; }
      if (ev && ev.metaKey) { mods |= 8; }
      state.mods = mods;
    };

    state.applyFontOverride = function () {
      var textEl = state.node;
      var labelEl = state.preedit;
      if (!textEl) {
        return;
      }
      if (state.customFont && state.customFont.length > 0) {
        textEl.style.fontFamily = state.customFont;
        if (labelEl) {
          labelEl.style.fontFamily = state.customFont;
        }
      } else {
        textEl.style.fontFamily = '';
      }
      if (state.customFontSize > 0) {
        var sizePx = state.customFontSize + 'px';
        textEl.style.fontSize = sizePx;
        if (labelEl) {
          labelEl.style.fontSize = sizePx;
        }
      } else {
        textEl.style.fontSize = '';
      }
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

    state.syncStylesFromCanvas = function (canvas, height) {
      var labelEl = state.preedit;
      if (labelEl && canvas && typeof window !== 'undefined' && window.getComputedStyle) {
        var computed = null;
        try {
          computed = window.getComputedStyle(canvas);
        } catch (err) {
          computed = null;
        }
        if (computed) {
          if (computed.font && computed.font.length > 0) {
            labelEl.style.font = computed.font;
          } else {
            if (computed.fontSize) {
              labelEl.style.fontSize = computed.fontSize;
            }
            if (computed.fontFamily) {
              labelEl.style.fontFamily = computed.fontFamily;
            }
            if (computed.fontWeight) {
              labelEl.style.fontWeight = computed.fontWeight;
            }
          }
        }
      }
      if (labelEl && Number.isFinite(height) && height > 0) {
        labelEl.style.lineHeight = height + 'px';
      }
      state.applyFontOverride();
    };

    state.hidePreedit = function () {
      state.preeditText = '';
      var labelEl = state.preedit;
      if (labelEl) {
        labelEl.textContent = '';
        labelEl.style.display = 'none';
      }
    };

    state.positionPreedit = function () {
      var labelEl = state.preedit;
      if (!labelEl || labelEl.style.display === 'none') {
        return;
      }
      var canvas = state.resolveCanvas();
      if (!canvas) {
        return;
      }
      var rect = canvas.getBoundingClientRect();
      var scrollX = (typeof window !== 'undefined' && typeof window.scrollX === 'number') ? window.scrollX : 0;
      var scrollY = (typeof window !== 'undefined' && typeof window.scrollY === 'number') ? window.scrollY : 0;
      var canvasLeft = rect.left + scrollX;
      var canvasTop = rect.top + scrollY;
      var caretX = canvasLeft + state.rect.x;
      var caretY = canvasTop + state.rect.y;
      var caretWidth = state.rect.w;
      var caretHeight = state.rect.h;
      if (!Number.isFinite(caretWidth) || caretWidth <= 0) {
        caretWidth = 1;
      }
      if (!Number.isFinite(caretHeight) || caretHeight <= 0) {
        caretHeight = 16;
      }
      var labelWidth = labelEl.offsetWidth;
      var labelHeight = labelEl.offsetHeight;
      if (labelWidth <= 0) {
        labelWidth = caretWidth;
      }
      if (labelHeight <= 0) {
        labelHeight = caretHeight;
      }
      var canvasRight = canvasLeft + rect.width;
      var canvasBottom = canvasTop + rect.height;
      var left = caretX;
      if (left + labelWidth > canvasRight) {
        left = canvasRight - labelWidth;
      }
      if (left < canvasLeft) {
        left = canvasLeft;
      }
      var baseline = caretY + caretHeight;
      var top = baseline - labelHeight;
      if (top < canvasTop) {
        top = caretY + caretHeight;
        if (top + labelHeight > canvasBottom) {
          top = Math.max(canvasTop, Math.min(canvasBottom - labelHeight, baseline - labelHeight));
        }
      }
      labelEl.style.left = Math.round(left) + 'px';
      labelEl.style.top = Math.round(top) + 'px';
    };

    state.setPreeditText = function (text) {
      var labelEl = state.preedit;
      if (!labelEl) {
        return;
      }
      state.preeditText = text || '';
      if (!state.preeditText) {
        state.hidePreedit();
        return;
      }
      labelEl.textContent = state.preeditText;
      labelEl.style.display = 'inline-block';
      state.positionPreedit();
    };

    ta.addEventListener('compositionstart', function (ev) {
      state.composing = true;
      state.expectNextInput = false;
      state.suppressNextInput = false;
      state.updateModsFromEvent(ev);
      state.setPreeditText('');
      if (Module.solunaSetComposing) {
        Module.solunaSetComposing(1);
      }
    });

    ta.addEventListener('compositionupdate', function (ev) {
      state.updateModsFromEvent(ev);
      var text = (ev && typeof ev.data === 'string') ? ev.data : '';
      state.setPreeditText(text);
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
      state.hidePreedit();
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

    var reposition = function () {
      state.positionPreedit();
    };

    globalScope.addEventListener('keydown', updateMods, true);
    globalScope.addEventListener('keyup', updateMods, true);
    globalScope.addEventListener('blur', function () {
      state.mods = 0;
      state.composing = false;
      state.expectNextInput = false;
      state.suppressNextInput = false;
      state.node.value = '';
      state.hidePreedit();
      if (Module.solunaSetComposing) {
        Module.solunaSetComposing(0);
      }
    });
    globalScope.addEventListener('resize', reposition);
    if (typeof document !== 'undefined') {
      document.addEventListener('scroll', reposition, true);
    }

    Module.solunaIme = state;
  },

  soluna_wasm_dom_show: function (x, y, w, h) {
    var state = Module.solunaIme;
    if (!state || typeof document === 'undefined') {
      return;
    }
    var canvas = state.resolveCanvas();
    if (!canvas) {
      return;
    }
    var rect = canvas.getBoundingClientRect();
    var scrollX = (typeof window !== 'undefined' && typeof window.scrollX === 'number') ? window.scrollX : 0;
    var scrollY = (typeof window !== 'undefined' && typeof window.scrollY === 'number') ? window.scrollY : 0;
    var canvasLeft = rect.left + scrollX;
    var canvasTop = rect.top + scrollY;
    var left = canvasLeft + x;
    var top = canvasTop + y;
    var width = Math.max(1, Number.isFinite(w) ? w : 1);
    var height = Math.max(1, (Number.isFinite(h) && h > 0) ? h : 16);

    state.rect.x = x;
    state.rect.y = y;
    state.rect.w = width;
    state.rect.h = height;

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
    state.syncStylesFromCanvas(canvas, height);
    state.positionPreedit();
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
    state.hidePreedit();
    el.blur();
    if (Module.solunaSetComposing) {
      Module.solunaSetComposing(0);
    }
  },

  soluna_wasm_dom_set_font__deps: ['$UTF8ToString'],
  soluna_wasm_dom_set_font: function (namePtr, size) {
    var state = Module.solunaIme;
    if (!state) {
      return;
    }
    var resolvedName = null;
    if (namePtr) {
      try {
        resolvedName = UTF8ToString(namePtr);
      } catch (err) {
        resolvedName = '';
      }
    }
    if (resolvedName && resolvedName.length === 0) {
      resolvedName = null;
    }
    var numericSize = 0;
    if (Number.isFinite(size) && size > 0) {
      numericSize = size;
    }
    var hasCustomFont = !!(resolvedName && resolvedName.length > 0);
    state.customFont = hasCustomFont ? resolvedName : null;
    state.customFontSize = numericSize;
    if (!hasCustomFont && numericSize === 0) {
      var canvas = state.resolveCanvas ? state.resolveCanvas() : null;
      if (canvas) {
        state.syncStylesFromCanvas(canvas, state.rect ? state.rect.h : 0);
      } else {
        state.applyFontOverride();
      }
    } else {
      state.applyFontOverride();
    }
    state.positionPreedit();
  }
});
