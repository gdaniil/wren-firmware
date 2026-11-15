import { ReaderFlasher } from './lib/flasher.js';

const $ = id => document.getElementById(id);
const choices = document.querySelectorAll('input[name="model"]');
const selectedModel = () => document.querySelector('input[name="model"]:checked')?.value;
const files = { c3: 'firmware.bin', x4c: 'firmware-x4c.bin', x4pro: 'firmware-x4pro.bin' };
const defaultError = 'unplug the cable, wake the reader, plug it back in and try again';
let port = null;
let flasher = null;
let busy = false;

function preventUnload(event) { event.preventDefault(); }
function setState(name) {
  window.removeEventListener('beforeunload', preventUnload);
  $('installer').dataset.state = name;
  $('status').textContent = '';
  if (name === 'installing') window.addEventListener('beforeunload', preventUnload);
}
function progress(value) {
  $('progress').value = value;
  $('percent').textContent = `${Math.floor(value)}%`;
}
function setBusy(value) {
  busy = value;
  $('install').disabled = value;
  for (const choice of choices) choice.disabled = value;
}
async function release() {
  const current = flasher;
  flasher = null;
  try { await current?.disconnect(); } catch { /* Cable may already be unplugged. */ }
}
async function fail(error, action) {
  await release();
  const message = error?.message || 'the connection was interrupted';
  setState('error');
  $('error-text').textContent = defaultError;
  $('retry').hidden = false;
  if (message.startsWith('This reader is locked.')) {
    $('error-text').textContent = 'this reader is locked, so wren cannot be installed.';
    $('retry').hidden = true;
  } else if (message === 'This reader is not supported.' || message === 'This reader does not have 16 MB of flash.') {
    $('error-text').textContent = 'this reader is not supported.';
    $('retry').hidden = true;
  } else {
    $('status').textContent = `${action} failed: ${message.charAt(0).toLowerCase()}${message.slice(1)}`;
  }
}

fetch('./firmware.json', { cache: 'no-store' }).then(response => {
  if (!response.ok) throw new Error('Build information unavailable');
  return response.json();
}).then(build => {
  if (build.source_url) $('source-link').href = build.source_url;
  if (build.fonts_bytes != null) $('fonts-label').textContent = `fonts.zip · ${(build.fonts_bytes / 1e6).toFixed(1)} mb`;
}).catch(() => { /* Local preview has no firmware.json; keep the defaults. */ });

$('connect').addEventListener('click', async () => {
  try {
    port = await navigator.serial.requestPort({ filters: [{ usbVendorId: 0x303a }] });
    setState('choose');
  } catch (error) {
    if (error?.name !== 'NotFoundError') await fail(error, 'connection');
  }
});
$('install').addEventListener('click', async () => {
  if (busy || !port) return;
  setBusy(true);
  $('status').textContent = 'checking your reader…';
  let action = 'connection';
  try {
    const model = selectedModel();
    flasher = new ReaderFlasher(port, model);
    await flasher.connect();
    action = 'installation';
    setState('installing');
    progress(0);
    const response = await fetch(`./${files[model]}`, { cache: 'no-store' });
    if (!response.ok) throw new Error('firmware could not be downloaded');
    const data = new Uint8Array(await response.arrayBuffer());
    await flasher.install(data, progress);
    await release();
    progress(100);
    setState('done');
  } catch (error) {
    const message = error?.message || '';
    if (action === 'connection' && (message.startsWith('This is an X4 Classic or X4 Pro.') || message.startsWith('This is an X3 or X4.'))) {
      await release();
      setState('choose');
      $('status').textContent = message.startsWith('This is an X4 Classic or X4 Pro.')
        ? 'this looks like an x4 classic or x4 pro. pick it and press install again'
        : 'this looks like an x3 or x4. pick it and press install again';
    } else {
      await fail(error, action);
    }
  } finally { setBusy(false); }
});
$('retry').addEventListener('click', async () => {
  await release();
  port = null;
  setState('connect');
});
navigator.serial?.addEventListener('disconnect', event => {
  if (event.target === port && !busy && $('installer').dataset.state === 'choose') {
    port = null;
    setState('error');
    $('error-text').textContent = defaultError;
    $('retry').hidden = false;
    $('status').textContent = 'the reader was unplugged';
  }
});

const phone = (matchMedia('(pointer: coarse)').matches && !matchMedia('(any-pointer: fine)').matches)
  || /Android|iPhone|iPad|Mobile/.test(navigator.userAgent);
setState(navigator.serial ? 'connect' : phone ? 'phone' : 'unsupported');
const preview = new URLSearchParams(location.search).get('state');
if (['localhost', '127.0.0.1'].includes(location.hostname)
    && ['unsupported', 'phone', 'connect', 'choose', 'installing', 'done', 'error'].includes(preview)) {
  setState(preview);
  if (preview === 'installing') progress(22);
  if (preview === 'done') progress(100);
}

// Wren idle moves: one every 6 s, never the same twice in a row. Off for reduced motion and hidden tabs.
const wren = $('wren');
const moves = ['blink', 'flick', 'hop', 'sing', 'peck', 'turn'];
let lastMove = 'turn';
function nextMove() {
  if (document.hidden || matchMedia('(prefers-reduced-motion: reduce)').matches) return;
  const options = moves.filter(move => move !== lastMove);
  lastMove = options[Math.floor(Math.random() * options.length)];
  delete wren.dataset.move;
  void wren.getBoundingClientRect(); // restart the CSS animation
  wren.dataset.move = lastMove;
}
setInterval(nextMove, 6000);
