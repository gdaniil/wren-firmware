import { X3Flasher } from './lib/flasher.js';

const $ = id => document.getElementById(id);
let flasher = null;
let busy = false;
let backedUp = false;
let completed = false;
const supported = Boolean(navigator.serial);
$('unsupported').hidden = supported;
$('flow').hidden = !supported;
$('credits').hidden = !supported;

fetch('./firmware.json', { cache: 'no-store' }).then(response => {
  if (!response.ok) throw new Error('Build information unavailable');
  return response.json();
}).then(build => {
  $('build').textContent = `Build ${build.short_sha} · ${new Date(build.built_at).toLocaleDateString(undefined, { year: 'numeric', month: 'short', day: 'numeric' })}`;
}).catch(() => { $('build').textContent = 'Firmware build information unavailable.'; });

function render() {
  const ready = backedUp || $('already').checked;
  $('connect').hidden = Boolean(flasher) || completed;
  $('connect').disabled = busy;
  $('backup-step').hidden = !flasher || completed;
  $('backup').hidden = backedUp;
  $('backup').disabled = busy;
  $('backup').classList.toggle('secondary', ready);
  $('already').disabled = busy;
  $('install').disabled = busy || !ready;
  $('install').classList.toggle('secondary', !ready);
  if (flasher && !completed) {
    $('step').textContent = `Step ${ready ? 3 : 2} of 3`;
    $('heading').textContent = ready ? 'Install Wren' : 'Keep a copy of your X3';
    $('hint').textContent = ready ? 'Keep the cable connected until installation finishes.' : 'Save the full 16 MB flash before installing. Keep the downloaded file somewhere safe.';
  }
}
function progress(value) {
  $('progress-wrap').hidden = false;
  $('progress').value = value;
  $('percent').textContent = `${Math.floor(value)}%`;
}
async function release() {
  const current = flasher;
  flasher = null;
  try { await current?.disconnect(); } catch { /* Cable may already be unplugged. */ }
}
async function fail(error, action) {
  await release();
  backedUp = false;
  $('already').checked = false;
  $('heading').textContent = 'Connection ended';
  $('step').textContent = 'Step 1 of 3';
  $('hint').textContent = 'Unplug the cable, wake the X3, reconnect it, and try again.';
  const message = error?.message || 'The connection was interrupted';
  $('status').textContent = message.startsWith('This X3 is locked.') ? message : `${action} failed: ${message.replace(/[.!?]+$/, '')}; unplug and reconnect the X3, close other serial apps, and try again.`;
}
$('already').addEventListener('change', render);
$('connect').addEventListener('click', async () => {
  if (busy) return;
  busy = true;
  $('status').textContent = 'Connecting and checking your X3…';
  render();
  try {
    // Request the chooser before any await to preserve the user gesture.
    const port = await navigator.serial.requestPort({ filters: [{ usbVendorId: 0x303a, usbProductId: 0x1001 }] });
    flasher = new X3Flasher(port);
    await flasher.connect();
    $('status').textContent = 'X3 connected.';
  } catch (error) { await fail(error, 'Connection'); }
  finally { busy = false; render(); }
});
$('backup').addEventListener('click', async () => {
  if (busy || !flasher) return;
  busy = true; render(); progress(0);
  $('progress-label').textContent = 'Saving backup';
  $('status').textContent = 'Reading the full flash; this may take several minutes.';
  try {
    const data = await flasher.backup(progress);
    const url = URL.createObjectURL(new Blob([data], { type: 'application/octet-stream' }));
    const link = document.createElement('a');
    link.href = url;
    link.download = `x3-backup-${new Date().toISOString().slice(0, 10).replaceAll('-', '')}.bin`;
    document.body.append(link); link.click(); link.remove();
    setTimeout(() => URL.revokeObjectURL(url), 60000);
    backedUp = true;
    $('status').textContent = 'Backup downloaded. Check that you saved the file before installing.';
  } catch (error) { await fail(error, 'Backup'); }
  finally { busy = false; render(); }
});
$('install').addEventListener('click', async () => {
  if (busy || !flasher || !(backedUp || $('already').checked)) return;
  busy = true; render(); progress(0);
  $('progress-label').textContent = 'Installing Wren';
  $('status').textContent = 'Downloading and checking firmware…';
  try {
    const response = await fetch('./firmware.bin', { cache: 'no-store' });
    if (!response.ok) throw new Error('Firmware could not be downloaded');
    const data = new Uint8Array(await response.arrayBuffer());
    $('status').textContent = 'Installing Wren. Keep the cable connected…';
    await flasher.install(data, progress);
    completed = true;
    await release();
    $('step').textContent = 'Step 3 of 3';
    $('heading').textContent = 'Ready to read';
    $('hint').textContent = '';
    $('status').textContent = 'Done. Unplug the cable and press the power button to restart the X3.';
  } catch (error) { await fail(error, 'Installation'); }
  finally { busy = false; render(); }
});
navigator.serial?.addEventListener('disconnect', async event => {
  if (flasher && event.target === flasher.port && !busy) {
    await fail(new Error('The X3 was unplugged'), 'Connection'); render();
  }
});
render();
