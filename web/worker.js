// Twin Embers web Builder: runs the Python Builder (builder/firered3ds_builder) with Pyodide in a
// worker, so the page stays responsive. The ROM arrives from the page and never leaves the device;
// the only network requests are Pyodide itself, the Builder's own files from this site and, when
// asked, the HeartGold sprite files the Builder lists (each checked by size and SHA-256 in Python).
'use strict';

const PYODIDE_VERSION = '0.28.3';
const PYODIDE_URL = `https://cdn.jsdelivr.net/pyodide/v${PYODIDE_VERSION}/full/`;
// SHA-384 of pyodide.js at that version; a worker cannot use the integrity attribute.
const PYODIDE_SHA384 = '4X7gSPzQ4pHfjTE5aBEPJAQcHu55sciq+NWO3OUOZ3zHSJhn4te9CBjUyRSr+nei';
const HOME = '/home/pyodide/builder';
const WORK = '/work';
const FETCHES = 8;

let pyodide = null;
let browser = null;

function post(type, fields, transfer) {
  self.postMessage(Object.assign({ type }, fields), transfer || []);
}

function base64(buffer) {
  let text = '';
  for (const byte of new Uint8Array(buffer)) text += String.fromCharCode(byte);
  return btoa(text);
}

function hex(buffer) {
  return Array.from(new Uint8Array(buffer), (b) => b.toString(16).padStart(2, '0')).join('');
}

async function fetchBytes(url) {
  const response = await fetch(url);
  if (!response.ok) throw new Error(`${url}: HTTP ${response.status}`);
  return response.arrayBuffer();
}

async function loadPyodideChecked() {
  const script = await fetchBytes(PYODIDE_URL + 'pyodide.js');
  if (base64(await crypto.subtle.digest('SHA-384', script)) !== PYODIDE_SHA384) {
    throw new Error('Pyodide did not match its pinned checksum.');
  }
  const url = URL.createObjectURL(new Blob([script], { type: 'text/javascript' }));
  try {
    importScripts(url);
  } finally {
    URL.revokeObjectURL(url);
  }
  return self.loadPyodide({ indexURL: PYODIDE_URL });
}

// The Builder package as tools/build_web.py lists it next to this file, each file checked by SHA-256.
async function installBuilder() {
  const list = JSON.parse(new TextDecoder().decode(await fetchBytes('builder/files.json')));
  for (const file of list.files) {
    const data = await fetchBytes('builder/' + file.path);
    if (hex(await crypto.subtle.digest('SHA-256', data)) !== file.sha256) {
      throw new Error(`Builder file changed in transit: ${file.path}`);
    }
    const target = `${HOME}/${file.path}`;
    pyodide.FS.mkdirTree(target.slice(0, target.lastIndexOf('/')));
    pyodide.FS.writeFile(target, new Uint8Array(data));
  }
  pyodide.runPython(`import sys\nsys.path.insert(0, ${JSON.stringify(HOME)})`);
  browser = pyodide.pyimport('firered3ds_builder.browser');
  return list.version;
}

function clearWork() {
  const fs = pyodide.FS;
  const remove = (path) => {
    if (!fs.analyzePath(path).exists) return;
    if (fs.isDir(fs.stat(path).mode)) {
      for (const name of fs.readdir(path)) if (name !== '.' && name !== '..') remove(`${path}/${name}`);
      fs.rmdir(path);
    } else {
      fs.unlink(path);
    }
  };
  remove(WORK);
  fs.mkdirTree(WORK);
}

// Fetches every listed sprite file into WORK/gen4. A file that fails is simply absent: the Builder
// then leaves that species on FireRed's pictures, exactly like the desktop Builder offline.
async function fetchSprites() {
  const rows = browser.gen4_downloads().toJs();
  const folder = `${WORK}/gen4`;
  pyodide.FS.mkdirTree(folder);
  let next = 0;
  let done = 0;
  let failed = 0;
  async function worker() {
    while (next < rows.length) {
      const [url, name] = rows[next++];
      try {
        pyodide.FS.writeFile(`${folder}/${name}`, new Uint8Array(await fetchBytes(url)));
      } catch (error) {
        failed++;
      }
      done++;
      post('progress', { stage: 'sprites', done, total: rows.length });
    }
  }
  await Promise.all(Array.from({ length: FETCHES }, worker));
  return { folder, failed };
}

function pythonMessage(error) {
  // A Python exception arrives with its traceback; the last line is the Builder's message.
  const lines = String(error && error.message ? error.message : error).trim().split('\n');
  return lines[lines.length - 1].replace(/^\w+(Error|Exception): /, '');
}

async function build(rom, gen4) {
  clearWork();
  pyodide.FS.writeFile(`${WORK}/rom.gba`, new Uint8Array(rom));
  let folder; // undefined reaches Python as None (null would be JsNull)
  let failed = 0;
  if (gen4) {
    post('progress', { stage: 'sprites', done: 0, total: 0 });
    ({ folder, failed } = await fetchSprites());
  }
  post('progress', { stage: 'build' });
  // Let the page paint the new stage before Python holds the worker for a while.
  await new Promise((resolve) => setTimeout(resolve, 0));
  const result = browser.build(`${WORK}/rom.gba`, `${WORK}/twinembers.pak`, folder);
  const info = result.toJs({ dict_converter: Object.fromEntries });
  result.destroy();
  const pak = pyodide.FS.readFile(`${WORK}/twinembers.pak`);
  clearWork();
  if (info.gen4) info.gen4.failed = failed;
  post('done', { pak: pak.buffer, info }, [pak.buffer]);
}

self.onmessage = async (event) => {
  const message = event.data;
  if (message.type !== 'build') return;
  try {
    await build(message.rom, message.gen4);
  } catch (error) {
    try { clearWork(); } catch (ignored) { /* nothing left to clean */ }
    post('error', { message: pythonMessage(error) });
  }
};

(async () => {
  try {
    post('loading', { text: 'Downloading Python for your browser (about 10 MB, kept by the browser for next time)…' });
    pyodide = await loadPyodideChecked();
    post('loading', { text: 'Loading the Builder…' });
    const version = await installBuilder();
    post('ready', { version });
  } catch (error) {
    post('fatal', { message: String(error && error.message ? error.message : error) });
  }
})();
