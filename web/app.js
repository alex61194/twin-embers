// Twin Embers web Builder: the page. The Python Builder runs in worker.js; this file reads the ROM the
// player picks, shows progress, offers the finished pack for download and draws the install QR codes.
'use strict';

// The page's language (index.html English, es.html Spanish, de.html German): L(english, spanish, german) picks the
// words, as CTR_TR does in the game (English where a German one is missing).
const LANG = document.documentElement.lang;
const SPANISH = LANG === 'es';
const GERMAN = LANG === 'de';
const L = (en, es, de) => (SPANISH ? es : GERMAN && de !== undefined ? de : en);

const ROM_SIZE = 16 * 1024 * 1024;
// The ROMs the Builder takes: each makes the pack for its own .3dsx (the Spanish one, twinembers-es.3dsx; the German one, twinembers-de.3dsx).
const ROMS = {
  '41cb23d8dccc8ebd7c649cd8fbb58eeace6e2fdc': { name: 'FireRed (USA)', game: 'twinembers.3dsx' },
  'ab8f6bfe0ccdaf41188cd015c8c74c314d02296a': { name: L('Rojo Fuego (Spain)', 'Rojo Fuego (España)', 'Rojo Fuego (Spanien)'), game: 'twinembers-es.3dsx' },
  '18a3758ceeef2c77b315144be2c3910d6f1f69fe': { name: L('Feuerrote Edition (Germany)', 'Feuerrote Edition (Alemania)', 'Feuerrote Edition (Deutschland)'), game: 'twinembers-de.3dsx' },
};
const RELEASES = 'https://github.com/alex61194/twin-embers/releases/latest/download/';
const FORWARDER_URL = RELEASES + 'TwinEmbers-Forwarder.cia';
const UNISTORE_URL = new URL('twinembers.unistore', location.href).href;

const $ = (id) => document.getElementById(id);
const ui = {
  romInput: $('rom-input'), romButton: $('rom-button'), romPill: $('rom-pill'),
  gen4: $('gen4-toggle'), npcs: $('npcs-toggle'), backgrounds: $('backgrounds-toggle'),
  followers: $('followers-toggle'), hd: $('hd-toggle'), looks: $('looks-toggle'),
  build: $('build-button'),
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
  ui.gen4.disabled = ui.npcs.disabled = ui.backgrounds.disabled = ui.followers.disabled = ui.hd.disabled = ui.looks.disabled = state.busy;
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
    setPill(L('No ROM chosen', 'Ninguna ROM elegida', 'Kein ROM gewählt'));
    return;
  }
  if (file.size !== ROM_SIZE) {
    setPill(L('Not a 16 MiB FireRed ROM', 'No es una ROM de FireRed de 16 MiB', 'Kein 16-MiB-Feuerrot-ROM'), 'bad');
    showError(L(`${file.name} is ${file.size} bytes. The Builder needs the exact 16 MiB dump of Pokémon FireRed Version (USA), Edición Rojo Fuego (Spain) or Feuerrote Edition (Germany), revision 0; trimmed files are not supported.`,
      `${file.name} ocupa ${file.size} bytes. El Builder necesita el volcado exacto de 16 MiB de Pokémon Edición Rojo Fuego (España), FireRed Version (USA) o Feuerrote Edition (Alemania), revisión 0; los archivos recortados no sirven.`,
      `${file.name} ist ${file.size} Bytes groß. Der Builder braucht den genauen 16-MiB-Dump von Pokémon Feuerrote Edition (Deutschland), FireRed Version (USA) oder Edición Rojo Fuego (Spanien), Revision 0; gekürzte Dateien gehen nicht.`));
    return;
  }
  setPill(L('Checking…', 'Comprobando…', 'Wird geprüft…'), 'busy');
  const data = await file.arrayBuffer();
  if (crypto.subtle) {
    const sha1 = hex(await crypto.subtle.digest('SHA-1', data));
    const rom = ROMS[sha1];
    if (!rom) {
      setPill(L('Unsupported ROM', 'ROM no compatible', 'ROM nicht unterstützt'), 'bad');
      showError(L(`SHA-1 ${sha1} is not FireRed (USA), Rojo Fuego (Spain) or Feuerrote Edition (Germany), revision 0. Revision 1, LeafGreen and other languages are not supported.`,
        `El SHA-1 ${sha1} no es de Rojo Fuego (España), FireRed (USA) ni Feuerrote Edition (Alemania), revisión 0. La revisión 1, Verde Hoja y otros idiomas no son compatibles.`,
        `SHA-1 ${sha1} ist weder Feuerrote Edition (Deutschland), FireRed (USA) noch Rojo Fuego (Spanien), Revision 0. Revision 1, Blattgrün und andere Sprachen werden nicht unterstützt.`));
      return;
    }
    setPill(L(`${rom.name} verified`, `${rom.name} verificada`, `${rom.name} geprüft`), 'ok');
    showGame(rom.game);
  } else {
    setPill(L('Will be checked when building', 'Se comprobará al crear el pack', 'Wird beim Erstellen geprüft'), 'busy');
  }
  state.rom = file;
  state.romOk = true;
  refresh();
}

// The worker's loading steps, in the page's language.
const LOADING = SPANISH ? {
  'Downloading Python for your browser (about 10 MB, kept by the browser for next time)…':
    'Descargando Python para tu navegador (unos 10 MB, el navegador lo guarda para la próxima vez)…',
  'Loading the Builder…': 'Cargando el Builder…',
} : GERMAN ? {
  'Downloading Python for your browser (about 10 MB, kept by the browser for next time)…':
    'Python für deinen Browser wird geladen (etwa 10 MB, der Browser behält es für das nächste Mal)…',
  'Loading the Builder…': 'Der Builder wird geladen…',
} : {};

const worker = new Worker('worker.js');

worker.onmessage = (event) => {
  const message = event.data;
  if (message.type === 'loading') {
    ui.status.textContent = LOADING[message.text] || message.text;
  } else if (message.type === 'ready') {
    state.ready = true;
    ui.status.textContent = state.romOk ? L('Ready to build.', 'Listo para crear el pack.', 'Bereit zum Erstellen.')
      : L('Builder ready. Choose your ROM to start.', 'Builder listo. Elige tu ROM para empezar.', 'Builder bereit. Wähle dein ROM, um zu beginnen.');
    refresh();
  } else if (message.type === 'fatal') {
    ui.status.textContent = L('The Builder could not start.', 'El Builder no ha podido arrancar.', 'Der Builder konnte nicht starten.');
    showError(`${message.message} ${L('Check your connection and reload the page.', 'Comprueba tu conexión y recarga la página.', 'Prüfe deine Verbindung und lade die Seite neu.')}`);
  } else if (message.type === 'progress') {
    if (message.stage === 'sprites') {
      showProgress(message.total ? message.done / message.total : 0);
      ui.status.textContent = message.total
        ? `${L('Downloading the optional pictures…', 'Descargando las imágenes opcionales…', 'Die optionalen Bilder werden geladen…')} ${message.done}/${message.total}`
        : L('Downloading the optional pictures…', 'Descargando las imágenes opcionales…', 'Die optionalen Bilder werden geladen…');
    } else {
      showProgress(null);
      ui.status.textContent = L('Rebuilding the pack from your ROM and checking every record… This can take a minute, more on a phone.',
        'Reconstruyendo el pack a partir de tu ROM y comprobando cada registro… Puede tardar un minuto, más en un móvil.',
        'Das Paket wird aus deinem ROM erstellt und jeder Eintrag geprüft… Das kann eine Minute dauern, auf dem Handy länger.');
    }
  } else if (message.type === 'done') {
    finish(message.pak, message.info);
  } else if (message.type === 'error') {
    state.busy = false;
    ui.progress.classList.add('hidden');
    ui.status.textContent = L('No pack was made.', 'No se ha creado el pack.', 'Es wurde kein Paket erstellt.');
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
    `${info.entries} ${L('entries', 'entradas', 'Einträge')}, ${mib(info.bytes)}, ${L('engine ABI', 'ABI del motor', 'Engine-ABI')} ${info.abi.toString(16).padStart(8, '0')}`,
    `SHA-256 ${info.sha256}`,
  ];
  if (info.gen4) {
    if (info.gen4.species || info.gen4.missing.length) {
      lines.push(L(`HeartGold battle sprites: ${info.gen4.species} species`, `Sprites de combate de HeartGold: ${info.gen4.species} especies`,
        `HeartGold-Kampf-Sprites: ${info.gen4.species} Arten`) +
        (info.gen4.missing.length ? L(`, ${info.gen4.missing.length} kept on FireRed's pictures (download failed)`,
          `, ${info.gen4.missing.length} con las imágenes de Rojo Fuego (falló la descarga)`,
          `, ${info.gen4.missing.length} mit den Bildern aus Feuerrot (Download fehlgeschlagen)`) : ''));
    }
    if (info.gen4.trainers) lines.push(L('HeartGold trainer pictures: ', 'Imágenes de entrenadores de HeartGold: ', 'HeartGold-Trainerbilder: ') + info.gen4.trainers);
    if (info.gen4.backs) lines.push(L('HeartGold-style player backs: ', 'Espaldas del jugador al estilo HeartGold: ', 'Spielerrücken im HeartGold-Stil: ') + info.gen4.backs);
    if (info.gen4.npcs) lines.push(L(`New people on the map: ${info.gen4.npcs} of 74`, `Gente nueva en el mapa: ${info.gen4.npcs} de 74`,
      `Neue Leute auf der Karte: ${info.gen4.npcs} von 74`));
    if (info.gen4.backgrounds) lines.push(L(`Battle backgrounds: ${info.gen4.backgrounds} of FireRed's 20`,
      `Fondos de combate: ${info.gen4.backgrounds} de los 20 de Rojo Fuego`, `Kampfhintergründe: ${info.gen4.backgrounds} der 20 aus Feuerrot`));
    if (info.gen4.followers) lines.push(L(`Pokémon that walk behind you: ${info.gen4.followers}`, `Pokémon que andan tras de ti: ${info.gen4.followers}`,
      `Pokémon, die dir folgen: ${info.gen4.followers}`));
    if (info.gen4.hd) lines.push(L(`HD battle scenes: ${info.gen4.hd} files`, `Escenas de combate HD: ${info.gen4.hd} archivos`,
      `HD-Kampfszenen: ${info.gen4.hd} Dateien`));
    if (info.gen4.looks) lines.push(L(`More characters to be: ${info.gen4.looks} looks`, `Más personajes: ${info.gen4.looks} aspectos`,
      `Mehr Figuren: ${info.gen4.looks} Looks`));
  }
  // A ticked set whose files could not be had (no network, or a file did not match) is not a success: say so, and that building
  // again tries it again. The boxes are disabled while it builds, so they still show what was asked.
  const g = info.gen4 || {};
  for (const [box, count, en, es, de] of [
    [ui.gen4, g.species, 'HeartGold battle sprites', 'Sprites de combate de HeartGold', 'HeartGold-Kampf-Sprites'],
    [ui.npcs, g.npcs, 'New people on the map', 'Gente nueva en el mapa', 'Neue Leute auf der Karte'],
    [ui.backgrounds, g.backgrounds, 'Battle backgrounds', 'Fondos de combate', 'Kampfhintergründe'],
    [ui.followers, g.followers, 'Pokémon that walk behind you', 'Pokémon que andan tras de ti', 'Pokémon, die dir folgen'],
    [ui.hd, g.hd, 'HD battle scenes', 'Escenas de combate HD', 'HD-Kampfszenen'],
    [ui.looks, g.looks, 'More characters to be', 'Más personajes', 'Mehr Figuren'],
  ]) {
    if (box && box.checked && !count) {
      lines.push(L(`${en}: nothing was added (no network, or a file did not match). Build again to retry.`,
        `${es}: no se ha añadido nada (sin red, o un archivo no coincidía). Vuelve a crear el pack para reintentarlo.`,
        `${de}: nichts hinzugefügt (kein Netz, oder eine Datei passte nicht). Erstelle das Paket erneut, um es zu wiederholen.`));
    }
  }
  ui.details.textContent = lines.join('\n');
  ui.details.style.whiteSpace = 'pre-line';
  ui.result.classList.remove('hidden');
  ui.status.textContent = L('Pack built and verified.', 'Pack creado y verificado.', 'Paket erstellt und geprüft.');
  refresh();
}

async function startBuild() {
  if (!state.rom || state.busy) return;
  state.busy = true;
  ui.error.classList.add('hidden');
  ui.result.classList.add('hidden');
  showProgress(null);
  ui.status.textContent = L('Starting…', 'Empezando…', 'Es geht los…');
  refresh();
  try {
    const rom = await state.rom.arrayBuffer();
    worker.postMessage({ type: 'build', rom, gen4: ui.gen4.checked, npcs: ui.npcs.checked,
      backgrounds: ui.backgrounds.checked, followers: ui.followers.checked, hd: ui.hd.checked, looks: ui.looks.checked }, [rom]);
  } catch (error) {
    state.busy = false;
    showError(`${L('Could not read the ROM again:', 'No se ha podido volver a leer la ROM:', 'Das ROM konnte nicht erneut gelesen werden:')} ${error.message}`);
    refresh();
  }
}

// The game the download code and button offer: the page's language's, then the one for the ROM chosen.
function showGame(name) {
  const url = RELEASES + name;
  drawQr('qr-game', 'url-game', url);
  $('game-download').href = url;
  $('game-download').textContent = GERMAN ? `${name} herunterladen` : `${L('Download', 'Descargar')} ${name}`;
  $('game-name').textContent = name;
}

// QR codes as SVG, drawn from qrcode-generator's module matrix with the standard 4-module margin.
function drawQr(boxId, urlId, text) {
  $(urlId).textContent = text;
  if (typeof qrcode !== 'function') {
    $(boxId).textContent = L('QR code unavailable offline.', 'Código QR no disponible sin conexión.', 'QR-Code offline nicht verfügbar.');
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
showGame(SPANISH ? 'twinembers-es.3dsx' : GERMAN ? 'twinembers-de.3dsx' : 'twinembers.3dsx');
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
