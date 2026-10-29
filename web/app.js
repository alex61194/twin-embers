// Twin Embers web Builder: the page. The Python Builder runs in worker.js; this file reads the ROM the
// player picks, shows progress, offers the finished pack for download and draws the install QR codes.
'use strict';

const ROM_SIZE = 16 * 1024 * 1024;
const ROM_SHA1 = '41cb23d8dccc8ebd7c649cd8fbb58eeace6e2fdc';
const RELEASES = 'https://github.com/alex61194/twin-embers/releases/latest/download/';
const FORWARDER_URL = RELEASES + 'TwinEmbers-Forwarder.cia';
const UNISTORE_URL = new URL('twinembers.unistore', location.href).href;

const $ = (id) => document.getElementById(id);
const ui = {
  romInput: $('rom-input'), romButton: $('rom-button'), romPill: $('rom-pill'),
  gen4: $('gen4-toggle'), build: $('build-button'),
  progress: $('progress'), fill: $('progress-fill'), status: $('status'),
  result: $('result'), download: $('download'), details: $('result-details'), error: $('error'),
};

const state = { ready: false, busy: false, rom: null, romOk: false, pakUrl: null };

function setPill(text, kind) {
  ui.romPill.textContent = text;
  ui.romPill.className = 'pill' + (kind ? ' ' + kind : '');
}

function refresh() {
  ui.build.disabled = !(state.ready && state.romOk && !state.busy);
  ui.romInput.disabled = state.busy;
  ui.gen4.disabled = state.busy;
  ui.romButton.setAttribute('aria-disabled', state.busy ? 'true' : 'false');
}

function showProgress(fraction) {
  ui.progress.classList.remove('hidden');
  ui.progress.classList.toggle('indeterminate', fraction === null);
  ui.fill.style.width = fraction === null ? '' : `${Math.round(fraction * 100)}%`;
}

function showError(text) {
  ui.error.textContent = text;
  ui.error.classList.remove('hidden');
}

function hex(buffer) {
  return Array.from(new Uint8Array(buffer), (b) => b.toString(16).padStart(2, '0')).join('');
}

function mib(bytes) {
  return (bytes / 1048576).toFixed(1) + ' MiB';
}

// A quick check on the page so a wrong file is reported at once; the Builder checks again itself.
async function chooseRom(file) {
  state.rom = null;
  state.romOk = false;
  ui.error.classList.add('hidden');
  ui.result.classList.add('hidden');
  refresh();
  if (!file) {
    setPill('No ROM chosen');
    return;
  }
  if (file.size !== ROM_SIZE) {
    setPill('Not a 16 MiB FireRed ROM', 'bad');
    showError(`${file.name} is ${file.size} bytes. The Builder needs the exact 16 MiB dump of Pokémon FireRed Version (USA), revision 0; trimmed files are not supported.`);
    return;
  }
  setPill('Checking…', 'busy');
  const data = await file.arrayBuffer();
  if (crypto.subtle) {
    const sha1 = hex(await crypto.subtle.digest('SHA-1', data));
    if (sha1 !== ROM_SHA1) {
      setPill('Unsupported ROM', 'bad');
      showError(`SHA-1 ${sha1} is not FireRed (USA) revision 0 (${ROM_SHA1}). Revision 1, LeafGreen and other languages are not supported.`);
      return;
    }
    setPill('FireRed (USA) verified', 'ok');
  } else {
    setPill('Will be checked when building', 'busy');
  }
  state.rom = file;
  state.romOk = true;
  refresh();
}

const worker = new Worker('worker.js');

worker.onmessage = (event) => {
  const message = event.data;
  if (message.type === 'loading') {
    ui.status.textContent = message.text;
  } else if (message.type === 'ready') {
    state.ready = true;
    ui.status.textContent = state.romOk ? 'Ready to build.' : 'Builder ready. Choose your ROM to start.';
    refresh();
  } else if (message.type === 'fatal') {
    ui.status.textContent = 'The Builder could not start.';
    showError(`${message.message} Check your connection and reload the page.`);
  } else if (message.type === 'progress') {
    if (message.stage === 'sprites') {
      showProgress(message.total ? message.done / message.total : 0);
      ui.status.textContent = message.total
        ? `Downloading HeartGold battle sprites… ${message.done}/${message.total}`
        : 'Downloading HeartGold battle sprites…';
    } else {
      showProgress(null);
      ui.status.textContent = 'Rebuilding the pack from your ROM and checking every record… This can take a minute, more on a phone.';
    }
  } else if (message.type === 'done') {
    finish(message.pak, message.info);
  } else if (message.type === 'error') {
    state.busy = false;
    ui.progress.classList.add('hidden');
    ui.status.textContent = 'No pack was made.';
    showError(message.message);
    refresh();
  }
};

function finish(pak, info) {
  state.busy = false;
  showProgress(1);
  if (state.pakUrl) URL.revokeObjectURL(state.pakUrl);
  state.pakUrl = URL.createObjectURL(new Blob([pak], { type: 'application/octet-stream' }));
  ui.download.href = state.pakUrl;
  const lines = [
    `${info.entries} entries, ${mib(info.bytes)}, engine ABI ${info.abi.toString(16).padStart(8, '0')}`,
    `SHA-256 ${info.sha256}`,
  ];
  if (info.gen4) {
    lines.push(`HeartGold battle sprites: ${info.gen4.species} species` +
      (info.gen4.missing.length ? `, ${info.gen4.missing.length} kept on FireRed's pictures (download failed)` : ''));
  }
  ui.details.textContent = lines.join('\n');
  ui.details.style.whiteSpace = 'pre-line';
  ui.result.classList.remove('hidden');
  ui.status.textContent = 'Pack built and verified.';
  refresh();
}

async function startBuild() {
  if (!state.rom || state.busy) return;
  state.busy = true;
  ui.error.classList.add('hidden');
  ui.result.classList.add('hidden');
  showProgress(null);
  ui.status.textContent = 'Starting…';
  refresh();
  try {
    const rom = await state.rom.arrayBuffer();
    worker.postMessage({ type: 'build', rom, gen4: ui.gen4.checked }, [rom]);
  } catch (error) {
    state.busy = false;
    showError(`Could not read the ROM again: ${error.message}`);
    refresh();
  }
}

// QR codes as SVG, drawn from qrcode-generator's module matrix with the standard 4-module margin.
function drawQr(boxId, urlId, text) {
  $(urlId).textContent = text;
  if (typeof qrcode !== 'function') {
    $(boxId).textContent = 'QR code unavailable offline.';
    return;
  }
  const qr = qrcode(0, 'M');
  qr.addData(text, 'Byte');
  qr.make();
  const count = qr.getModuleCount();
  const size = count + 8;
  let path = '';
  for (let y = 0; y < count; y++) {
    for (let x = 0; x < count; x++) {
      if (qr.isDark(y, x)) path += `M${x + 4} ${y + 4}h1v1h-1z`;
    }
  }
  $(boxId).innerHTML = `<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 ${size} ${size}" shape-rendering="crispEdges">` +
    `<rect width="${size}" height="${size}" fill="#fff"/><path d="${path}" fill="#000"/></svg>`;
}

ui.romInput.addEventListener('change', () => chooseRom(ui.romInput.files[0]).catch((e) => showError(e.message)));
ui.romButton.addEventListener('keydown', (event) => {
  if ((event.key === 'Enter' || event.key === ' ') && !state.busy) {
    event.preventDefault();
    ui.romInput.click();
  }
});
ui.build.addEventListener('click', startBuild);
drawQr('qr-unistore', 'url-unistore', UNISTORE_URL);
drawQr('qr-fbi', 'url-fbi', FORWARDER_URL);
refresh();

// The gallery: a capture opens larger in a dialog.
(function () {
  const dialog = document.getElementById('lightbox');
  if (!dialog || typeof dialog.showModal !== 'function') return;
  const img = document.getElementById('lightbox-img');
  const caption = document.getElementById('lightbox-caption');
  document.querySelectorAll('#gallery .shot').forEach((button) => {
    button.addEventListener('click', () => {
      const thumb = button.querySelector('img');
      img.src = button.dataset.full;
      img.alt = thumb.alt;
      caption.textContent = button.parentElement.querySelector('figcaption').textContent;
      dialog.showModal();
    });
  });
  dialog.addEventListener('click', (event) => { if (event.target === dialog) dialog.close(); });
})();

// The embers of the lower screen (3ds_touch_skin_impl.h, EmbersStep): sparks rising slowly with a sway, a pale core
// and a halo in one of three fire colours, brighter low down where the fire is. Drawn at a third of the window's
// pixels so they look like the 3DS's.
(function () {
  const canvas = document.getElementById('embers');
  if (!canvas || !canvas.getContext) return;
  const ctx = canvas.getContext('2d');
  const SCALE = 3;
  const HUES = [[255, 200, 80], [255, 150, 40], [255, 96, 32]];
  const SWAY = [0, 9, 17, 22, 24, 22, 17, 9, 0, -9, -17, -22, -24, -22, -17, -9].map((k) => k / 8);
  const still = window.matchMedia && window.matchMedia('(prefers-reduced-motion: reduce)').matches;
  let w = 0, h = 0, sparks = [];

  function spawn(s, anywhere) {
    s.x = Math.random() * w;
    s.y = anywhere ? Math.random() * h : h + 2 + Math.random() * 20;
    s.vy = 0.19 + Math.random() * 0.43;
    s.phase = Math.random() * 65536;
    s.step = 200 + Math.random() * 500;
    s.big = Math.random() < 0.25;
    s.hue = HUES[Math.floor(Math.random() * 3)];
    return s;
  }

  function resize() {
    w = Math.ceil(window.innerWidth / SCALE);
    h = Math.ceil(window.innerHeight / SCALE);
    canvas.width = w;
    canvas.height = h;
    // As many sparks for the window as the 3DS has for its 320 x 240 (22).
    const want = Math.max(12, Math.min(120, Math.round(22 * w * h / (320 * 240))));
    while (sparks.length < want) sparks.push(spawn({}, true));
    sparks.length = want;
    sparks.forEach((s) => { if (s.x > w || s.y > h + 22) spawn(s, true); });
  }

  function draw() {
    ctx.clearRect(0, 0, w, h);
    for (const s of sparks) {
      const x = Math.round(s.x + SWAY[(s.phase >> 12) & 15]);
      const y = Math.round(s.y);
      const a = (120 + Math.max(0, Math.min(135, y * 135 / h))) / 255;
      const size = s.big ? 2 : 1;
      ctx.fillStyle = `rgba(${s.hue[0]}, ${s.hue[1]}, ${s.hue[2]}, ${a * 0.5})`;
      ctx.fillRect(x - 1, y, 1, size);
      ctx.fillRect(x + size, y, 1, size);
      ctx.fillRect(x, y - 1, size, 1);
      ctx.fillRect(x, y + size, size, 1);
      ctx.fillStyle = `rgba(255, 236, 170, ${a})`;
      ctx.fillRect(x, y, size, size);
    }
  }

  function step() {
    for (const s of sparks) {
      s.y -= s.vy;
      s.phase = (s.phase + s.step) % 65536;
      if (s.y < -4) spawn(s, false);
    }
    draw();
    requestAnimationFrame(step);
  }

  resize();
  window.addEventListener('resize', () => { resize(); if (still) draw(); });
  if (still) draw();
  else requestAnimationFrame(step);
})();
