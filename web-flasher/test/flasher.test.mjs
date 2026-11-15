// Adapted from crosspoint-tools (MIT), b62c8b57880415bfc73ad5a5941cc8087846d256.
import assert from 'node:assert/strict';
import test from 'node:test';
import { webcrypto, createHash } from 'node:crypto';
// Load as ESM on older Node versions too, without package.json or dependencies.
import { readFile } from 'node:fs/promises';
const source = await readFile(new URL('../lib/flasher.js', import.meta.url), 'utf8');
const { X3_PARTITION_TABLE: PT, S3_PARTITION_TABLE: S3, parsePartitionTable, requireLayout, parseOtadata,
  buildNewOtadataSector, pickBootAppPartition, validateFirmwareImage, imageChipName, ReaderFlasher } =
  await import(`data:text/javascript;base64,${Buffer.from(source).toString('base64')}`);
if (!globalThis.crypto) globalThis.crypto = webcrypto;

// Independent IDF CRC fixture, carried over from the reference tests.
function crc32Le(seq) {
  let crc = 0;
  for (const byte of [seq & 255, (seq >>> 8) & 255, (seq >>> 16) & 255, seq >>> 24]) {
    crc ^= byte;
    for (let i = 0; i < 8; i++) crc = crc & 1 ? (crc >>> 1) ^ 0xedb88320 : crc >>> 1;
  }
  return (crc ^ 0xffffffff) >>> 0;
}
function ota(seq, sector = 0, state = 2) {
  const data = new Uint8Array(8192).fill(255);
  const view = new DataView(data.buffer); const at = sector * 4096;
  view.setUint32(at, seq, true); view.setUint32(at + 24, state, true);
  view.setUint32(at + 28, crc32Le(seq), true);
  return data;
}
function table(layout = PT) {
  const data = new Uint8Array(4096).fill(255);
  const codes = [[1, 2], [1, 0], [0, 16], [0, 17], [1, 130], [1, 3]];
  layout.forEach((p, i) => {
    const at = i * 32; data.fill(0, at, at + 32);
    data.set([0xaa, 0x50, ...codes[i]], at);
    const view = new DataView(data.buffer);
    view.setUint32(at + 4, p.offset, true); view.setUint32(at + 8, p.size, true);
  });
  return data;
}
function image(hash = false) {
  const data = new Uint8Array(hash ? 80 : 48);
  data[0] = 0xe9; data[1] = 1; data[12] = 5; data[23] = Number(hash);
  new DataView(data.buffer).setUint32(28, 4, true);
  data.set([1, 2, 3, 4], 32); data[47] = 0xef ^ 1 ^ 2 ^ 3 ^ 4;
  if (hash) data.set(createHash('sha256').update(data.subarray(0, 48)).digest(), 48);
  return data;
}
test('X3 partition table parses and rejects unsafe or unfamiliar layouts', () => {
  assert.deepEqual(parsePartitionTable(table()), PT);
  assert.deepEqual(requireLayout(parsePartitionTable(table()), PT), PT);
  const bad = table(); bad[0] = 0;
  assert.throws(() => parsePartitionTable(bad), /magic/);
  assert.throws(() => requireLayout(PT.map(p => ({ ...p, offset: 0 })), PT), /unsupported/);
  assert.throws(() => requireLayout(PT.slice(1), PT), /unsupported/);
});
test('OTA sequence chooses app independently of sector index', () => {
  for (const sector of [0, 1]) for (const seq of [1, 2, 17, 18]) {
    const raw = ota(seq, sector); const result = parseOtadata(raw);
    assert.equal(result.activeApp, (seq - 1) % 2);
    assert.equal(result.inactiveApp, seq % 2);
    assert.equal(result.targetSector, 1 - sector);
    assert.equal(pickBootAppPartition(PT, raw).type, `app-ota_${(seq - 1) % 2}`);
    const next = buildNewOtadataSector(raw.subarray((1-sector)*4096, (2-sector)*4096), result.newSeq);
    raw.set(next, (1-sector)*4096);
    assert.equal(parseOtadata(raw).activeApp, result.inactiveApp);
  }
});
test('blank, invalid, aborted and bad CRC entries fall back to app0', () => {
  for (const raw of [new Uint8Array(8192).fill(255), ota(9, 0, 3), ota(9, 1, 4), new Uint8Array(8192)]) {
    assert.equal(parseOtadata(raw).activeApp, 0);
    assert.equal(parseOtadata(raw).newSeq, 2);
  }
  assert.throws(() => parseOtadata(new Uint8Array(32)), /truncated/);
  assert.throws(() => parseOtadata(ota(0xfffffffe)), /exhausted/);
});
test('highest eligible sequence wins', () => {
  const raw = ota(7); raw.set(ota(10, 1).subarray(4096), 4096);
  assert.equal(parseOtadata(raw).activeSeq, 10);
});
test('valid app images pass with and without SHA-256', async () => {
  for (const hash of [false, true]) await validateFirmwareImage(image(hash));
  assert.equal(imageChipName(image()), 'ESP32-C3');
});
test('HTML, truncation, overflow, bad segment counts, checksum and SHA fail', async () => {
  const badMagic = image(); badMagic[0] = 0;
  const overflow = image(); new DataView(overflow.buffer).setUint32(28, 0xffffffff, true);
  const badChecksum = image(); badChecksum[32] ^= 1;
  const badHash = image(true); badHash[79] ^= 1;
  const zeroSegments = image(); zeroSegments[1] = 0;
  for (const data of [new Uint8Array(3), badMagic, image().slice(0, 30), overflow, badChecksum, badHash, zeroSegments]) {
    await assert.rejects(validateFirmwareImage(data));
  }
});
test('install writes only inactive X3 slot then opposite OTA sector; verifies selector', async () => {
  for (const activeSector of [0, 1]) for (const seq of [1, 2]) {
    const flasher = new ReaderFlasher(null, 'c3'); let raw = ota(seq, activeSector); const writes = [];
    flasher.loader = {
      readFlash: async address => address === 0x8000 ? table() : raw,
      ui8ToBstr: data => data,
      writeFlash: async options => {
        assert.equal(options.eraseAll, false);
        const file = options.fileArray[0]; writes.push(file.address);
        if (file.address < 0x10000) raw.set(file.data, file.address - 0xe000);
      },
    };
    await flasher.install(image(), () => {});
    assert.deepEqual(writes, [seq % 2 ? 0x780000 : 0x10000, activeSector ? 0xe000 : 0xf000]);
    await assert.rejects(flasher.write(image(), 0, () => {}), /Unsafe/);
    const wrongChip = image(); wrongChip[12] = 9;
    await assert.rejects(flasher.install(wrongChip, () => {}), /ESP32-C3/);
  }
});

test('CRC-valid zero OTA sequence rejects without looping', () => {
  assert.throws(() => parseOtadata(ota(0)), /zero sequence/);
});
test('disconnect releases a real reader lock before closing transport', async () => {
  const stream = new ReadableStream(); const reader = stream.getReader();
  const flasher = new ReaderFlasher(null, 'c3'); let closed = false;
  flasher.loader = {
    after: async mode => assert.equal(mode, 'no_reset_stub'),
    transport: { reader, disconnect: async () => {
      assert.equal(stream.locked, false); closed = true;
    } },
  };
  assert.equal(stream.locked, true);
  await flasher.disconnect();
  assert.equal(closed, true); assert.equal(flasher.loader, null);
});
test('backup rejects same-length corrupt reads using device MD5', async () => {
  const flasher = new ReaderFlasher(null, 'c3');
  flasher.loader = {
    readFlash: async () => new Uint8Array(65536),
    flashMd5sum: async () => 'corrupt',
    toHex: data => Buffer.from(data).toString('hex'),
  };
  await assert.rejects(flasher.backup(() => {}), /verification/);
});
test('write verification callback computes MD5 and app failure prevents selector write', async () => {
  const flasher = new ReaderFlasher(null, 'c3'); const writes = [];
  flasher.loader = {
    readFlash: async address => address === 0x8000 ? table() : ota(1),
    ui8ToBstr: data => Buffer.from(data).toString('latin1'),
    bstrToUi8: data => new Uint8Array(Buffer.from(data, 'latin1')),
    toHex: data => Buffer.from(data).toString('hex'),
    writeFlash: async options => {
      const file = options.fileArray[0]; writes.push(file.address);
      assert.equal(options.calculateMD5Hash(file.data), createHash('md5').update(Buffer.from(file.data, 'latin1')).digest('hex'));
      throw new Error('MD5 of file does not match data in flash!');
    },
  };
  await assert.rejects(flasher.install(image(), () => {}), /MD5/);
  assert.deepEqual(writes, [0x780000]);
});


test('S3 layout is accepted only for S3 models', () => {
  assert.deepEqual(parsePartitionTable(table(S3)), S3);
  for (const model of ['x4c', 'x4pro']) {
    const f = new ReaderFlasher(null, model);
    assert.deepEqual(requireLayout(parsePartitionTable(table(S3)), f.model.layout), S3);
    assert.throws(() => requireLayout(PT, f.model.layout), /unsupported/);
  }
  assert.throws(() => requireLayout(S3, new ReaderFlasher(null, 'c3').model.layout), /unsupported/);
});

test('connection checks chip before reading flash or registers', async () => {
  for (const [model, chip, message] of [
    ['c3', 'ESP32-S3', 'This is an X4 Classic or X4 Pro. Choose it above and connect again.'],
    ['x4c', 'ESP32-C3', 'This is an X3 or X4. Choose it above and connect again.'],
    ['x4pro', 'ESP32', 'This reader is not supported.'],
  ]) {
    const f = new ReaderFlasher(null, model); let reads = 0;
    f.createLoader = async () => ({ chip: { CHIP_NAME: chip }, main: async () => {},
      getFlashSize: async () => { reads++; }, readReg: async () => { reads++; }, readFlash: async () => { reads++; } });
    await assert.rejects(f.connect(), error => error.message === message);
    assert.equal(reads, 0);
  }
});

test('S3 lock checks use S3 registers and encryption parity', async () => {
  for (const [secure, count, locked] of [[1 << 20, 0, true], [0, 1, true], [0, 3, false]]) {
    const f = new ReaderFlasher(null, 'x4c'); const registers = [];
    f.createLoader = async () => ({ chip: { CHIP_NAME: 'ESP32-S3' }, main: async () => {},
      getFlashSize: async () => 16384,
      readReg: async address => { registers.push(address); return address === 0x60007038 ? secure : count << 18; },
      readFlash: async address => address === 0x8000 ? table(S3) : ota(1) });
    if (locked) await assert.rejects(f.connect(), error => error.message === 'This reader is locked. Installing could brick it, so we stopped.');
    else await f.connect();
    assert.deepEqual(registers, [0x60007038, 0x60007034]);
  }
});

test('S3 install uses only its inactive slot and one OTA sector', async () => {
  for (const model of ['x4c', 'x4pro']) for (const seq of [1, 2]) for (const sector of [0, 1]) {
    const f = new ReaderFlasher(null, model); let raw = ota(seq, sector); const writes = [];
    f.loader = { readFlash: async address => address === 0x8000 ? table(S3) : raw,
      ui8ToBstr: data => data, writeFlash: async options => {
        assert.equal(options.eraseAll, false);
        const file = options.fileArray[0]; writes.push(file.address);
        if (file.address < 0x10000) raw.set(file.data, file.address - 0xe000);
      } };
    const s3Image = image(); s3Image[12] = 9;
    await f.install(s3Image, () => {});
    assert.deepEqual(writes, [seq % 2 ? 0x7f0000 : 0x10000, sector ? 0xe000 : 0xf000]);
    await assert.rejects(f.write(s3Image, 0x780000, () => {}), /Unsafe/);
    await assert.rejects(f.install(image(), () => {}), /ESP32-S3/);
    await assert.rejects(new ReaderFlasher(null, 'c3').install(s3Image, () => {}), /ESP32-C3/);
    await assert.rejects(new ReaderFlasher(null, 'c3').write(image(), 0x7f0000, () => {}), /Unsafe/);
  }
});
