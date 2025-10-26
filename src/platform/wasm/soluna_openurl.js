mergeInto(LibraryManager.library, {
  soluna_wasm_open_url__deps: ['$UTF8ToString'],
  soluna_wasm_open_url: function (urlPtr) {
    if (!urlPtr) {
      return;
    }
    var href = UTF8ToString(urlPtr);
    if (!href) {
      return;
    }
    if (typeof window !== 'undefined' && typeof window.open === 'function') {
      window.open(href, '_blank');
      return;
    }
    if (typeof self !== 'undefined' && typeof self.open === 'function') {
      self.open(href, '_blank');
    }
  }
});
