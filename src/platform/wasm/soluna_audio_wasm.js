// Override emscripten_request_animation_frame_loop to support JSPI.
//
// When JSPI is enabled (-sJSPI), audio worklet initialization calls
// emscripten_sleep() which requires the WebAssembly call stack to be
// in a "promising" context (wrapped with WebAssembly.promising).
//
// This override wraps the C frame callback with WebAssembly.promising
// so that emscripten_sleep works correctly during audio init.
mergeInto(LibraryManager.library, {
  emscripten_request_animation_frame_loop__deps: ['$getWasmTableEntry'],
  emscripten_request_animation_frame_loop__sig: 'vpp',
  emscripten_request_animation_frame_loop: function(cb, userData) {
    var fn = getWasmTableEntry(cb);
    var promisingFn = null;
    if (typeof WebAssembly !== 'undefined' && typeof WebAssembly.promising === 'function') {
      try {
        promisingFn = WebAssembly.promising(fn);
      } catch (e) {
        promisingFn = null;
      }
    }
    function tick(rAF_time) {
      if (promisingFn) {
        promisingFn(rAF_time, userData).then(function(keepGoing) {
          if (keepGoing) requestAnimationFrame(tick);
        }).catch(function(e) {
          console.error('Soluna: JSPI frame callback error (WebAssembly.promising):', e instanceof Error ? e.message : String(e), e);
        });
      } else {
        if (fn(rAF_time, userData)) {
          requestAnimationFrame(tick);
        }
      }
    }
    requestAnimationFrame(tick);
  },
});
