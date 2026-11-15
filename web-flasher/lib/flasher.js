/**
 * Wren reader flasher, derived from CrossPoint ESP32 Flasher (MIT).
 * Source: crosspoint-reader/crosspoint-tools, b62c8b57880415bfc73ad5a5941cc8087846d256.
 * See ../LICENSE-THIRD-PARTY.md. Uses the locally vendored esptool-js.
 */
// --- CRC32 ---

const CRC32_TABLE = new Uint32Array(256);
(function buildTable() {
  for (let i = 0; i < 256; i++) {
    let crc = i;
    for (let j = 0; j < 8; j++) {
      crc = (crc & 1) ? (0xEDB88320 ^ (crc >>> 1)) : (crc >>> 1);
    }
    CRC32_TABLE[i] = crc >>> 0;
  }
})();

function crc32(data, previous = 0) {
  let crc = previous === 0 ? 0 : (previous ^ 0xFFFFFFFF) >>> 0;
  for (let i = 0; i < data.length; i++) {
    crc = CRC32_TABLE[(crc ^ data[i]) & 0xFF] ^ (crc >>> 8);
  }
  return (crc ^ 0xFFFFFFFF) >>> 0;
}

// --- Byte Utilities ---

function u32ToLeBytes(val) {
  return new Uint8Array([val & 0xFF, (val >>> 8) & 0xFF, (val >>> 16) & 0xFF, (val >>> 24) & 0xFF]);
}

function leBytesToU32(bytes) {
  return ((bytes[0] || 0) + (((bytes[1] || 0) << 8) >>> 0) +
    (((bytes[2] || 0) << 16) >>> 0) + (((bytes[3] || 0) << 24) >>> 0)) >>> 0;
}

function isEqualBytes(a, b) {
  if (a.length !== b.length) return false;
  for (let i = 0; i < a.length; i++) {
    if (a[i] !== b[i]) return false;
  }
  return true;
}

function generateCrc32Le(sequence) {
  // ESP-IDF stores crc32_le(UINT32_MAX, ota_seq, 4) in otadata entries.
  return u32ToLeBytes(crc32(u32ToLeBytes(sequence), 0xFFFFFFFF));
}

// --- MD5 ---
//
// The partition table's checksum row stores an MD5 digest and WebCrypto
// doesn't offer MD5, so this is a minimal RFC 1321 implementation.

const MD5_K = new Uint32Array(64);
for (let i = 0; i < 64; i++) MD5_K[i] = Math.floor(Math.abs(Math.sin(i + 1)) * 2 ** 32);
const MD5_S = [
  7, 12, 17, 22, 7, 12, 17, 22, 7, 12, 17, 22, 7, 12, 17, 22,
  5, 9, 14, 20, 5, 9, 14, 20, 5, 9, 14, 20, 5, 9, 14, 20,
  4, 11, 16, 23, 4, 11, 16, 23, 4, 11, 16, 23, 4, 11, 16, 23,
  6, 10, 15, 21, 6, 10, 15, 21, 6, 10, 15, 21, 6, 10, 15, 21,
];

function md5(input) {
  const bitLen = input.length * 8;
  const paddedLen = (Math.floor((input.length + 8) / 64) + 1) * 64;
  const msg = new Uint8Array(paddedLen);
  msg.set(input);
  msg[input.length] = 0x80;
  // 64-bit LE bit length; JS bit ops are 32-bit, so split manually.
  msg.set(u32ToLeBytes(bitLen >>> 0), paddedLen - 8);
  msg.set(u32ToLeBytes(Math.floor(bitLen / 2 ** 32)), paddedLen - 4);

  let a0 = 0x67452301, b0 = 0xefcdab89, c0 = 0x98badcfe, d0 = 0x10325476;
  const M = new Uint32Array(16);
  for (let chunk = 0; chunk < paddedLen; chunk += 64) {
    for (let i = 0; i < 16; i++) M[i] = leBytesToU32(msg.subarray(chunk + i * 4, chunk + i * 4 + 4));
    let A = a0, B = b0, C = c0, D = d0;
    for (let i = 0; i < 64; i++) {
      let F, g;
      if (i < 16) { F = (B & C) | (~B & D); g = i; }
      else if (i < 32) { F = (D & B) | (~D & C); g = (5 * i + 1) % 16; }
      else if (i < 48) { F = B ^ C ^ D; g = (3 * i + 5) % 16; }
      else { F = C ^ (B | ~D); g = (7 * i) % 16; }
      F = (F + A + MD5_K[i] + M[g]) >>> 0;
      A = D; D = C; C = B;
      B = (B + ((F << MD5_S[i]) | (F >>> (32 - MD5_S[i])))) >>> 0;
    }
    a0 = (a0 + A) >>> 0; b0 = (b0 + B) >>> 0; c0 = (c0 + C) >>> 0; d0 = (d0 + D) >>> 0;
  }
  const out = new Uint8Array(16);
  out.set(u32ToLeBytes(a0), 0);
  out.set(u32ToLeBytes(b0), 4);
  out.set(u32ToLeBytes(c0), 8);
  out.set(u32ToLeBytes(d0), 12);
  return out;
}

// --- Firmware Image Validation ---

const ESP_IMAGE_MAGIC = 0xE9;
const IMG_HEADER_SIZE = 24;

// esp_chip_id_t from the ESP image extended header (bytes 12-13, LE). Legacy
// images carry 0xFFFF ("any"), so an unrecognized id resolves to null and is
// not treated as a mismatch.
const ESP_IMAGE_CHIP_IDS = {
  'ESP32': 0x0000,
  'ESP32-S2': 0x0002,
  'ESP32-C3': 0x0005,
  'ESP32-S3': 0x0009,
};

export function imageChipName(data) {
  if (data.length < 14) return null;
  const id = data[12] | (data[13] << 8);
  const entry = Object.entries(ESP_IMAGE_CHIP_IDS).find(([, chipId]) => chipId === id);
  return entry ? entry[0] : null;
}
const IMG_SEG_HEADER_SIZE = 8;
const IMG_SHA_TRAILER = 32;
const IMG_CHECKSUM_SEED = 0xEF;
// header[23] bit 0 = hash_appended; default IDF builds set it.
const IMG_HASH_APPENDED_OFFSET = 23;

// Walk the ESP image structure: 24-byte header, segCount segments each with an
// 8-byte header + dataLen bytes, padding-to-16, 1-byte XOR checksum at
// padEnd - 1, optional 32-byte SHA-256 over [0, totalSize - 32). Rejects HTML
// error pages, truncated downloads, and wrong-shape binaries that would
// otherwise pass the only previous check (a length range).
//
// Headroom-first arithmetic on every bound: `totalSize - pos < N`, never
// `pos + N > totalSize`. Hostile dataLen = 0xFFFFFFFF wraps the addition form
// into "valid" and admits a 4 GB read; the subtraction form catches it.
export async function validateFirmwareImage(data) {
  const totalSize = data.length;
  if (totalSize < IMG_HEADER_SIZE) {
    throw new Error('Firmware too small: header is truncated.');
  }
  if (data[0] !== ESP_IMAGE_MAGIC) {
    throw new Error('Invalid firmware: ESP image magic byte (0xE9) missing. Are you sure this is a firmware .bin?');
  }
  const segCount = data[1];
  if (segCount < 1 || segCount > 16) throw new Error('Invalid firmware segment count.');
  const hashAppended = (data[IMG_HASH_APPENDED_OFFSET] & 0x01) !== 0;

  let xorAccum = IMG_CHECKSUM_SEED;
  let pos = IMG_HEADER_SIZE;
  for (let i = 0; i < segCount; i++) {
    if (totalSize - pos < IMG_SEG_HEADER_SIZE) {
      throw new Error('Invalid firmware: segment header runs past end of file.');
    }
    const dataLen = leBytesToU32(data.subarray(pos + 4, pos + 8));
    pos += IMG_SEG_HEADER_SIZE;
    if (dataLen > totalSize - pos) {
      throw new Error('Invalid firmware: segment data runs past end of file.');
    }
    const end = pos + dataLen;
    for (let j = pos; j < end; j++) xorAccum ^= data[j];
    pos = end;
  }

  // (pos + 16) & ~15 lands in [1, 16] bytes past pos; the byte at padEnd - 1
  // holds the XOR-of-segment-data checksum.
  const padEnd = (pos + 16) & ~15;
  const expectedTotal = padEnd + (hashAppended ? IMG_SHA_TRAILER : 0);
  if (expectedTotal !== totalSize) {
    throw new Error(`Invalid firmware: declared size ${expectedTotal} does not match file size ${totalSize}.`);
  }
  const storedChecksum = data[padEnd - 1];
  if ((xorAccum & 0xFF) !== storedChecksum) {
    throw new Error(`Invalid firmware: segment checksum mismatch (computed 0x${(xorAccum & 0xFF).toString(16)}, stored 0x${storedChecksum.toString(16)}).`);
  }
  if (hashAppended) {
    const body = data.subarray(0, totalSize - IMG_SHA_TRAILER);
    const digest = new Uint8Array(await crypto.subtle.digest('SHA-256', body));
    const stored = data.subarray(totalSize - IMG_SHA_TRAILER);
    if (!isEqualBytes(digest, stored)) {
      throw new Error('Invalid firmware: SHA-256 trailer mismatch. File is corrupt or truncated.');
    }
  }
}

export const X3_PARTITION_TABLE = [
  { type: 'data-nvs', offset: 0x9000, size: 0x5000 },
  { type: 'data-ota', offset: 0xe000, size: 0x2000 },
  { type: 'app-ota_0', offset: 0x10000, size: 0x770000 },
  { type: 'app-ota_1', offset: 0x780000, size: 0x770000 },
  { type: 'data-spiffs', offset: 0xef0000, size: 0x100000 },
  { type: 'data-coredump', offset: 0xff0000, size: 0x10000 },
];

export const S3_PARTITION_TABLE = [
  { type: 'data-nvs', offset: 0x9000, size: 0x5000 },
  { type: 'data-ota', offset: 0xe000, size: 0x2000 },
  { type: 'app-ota_0', offset: 0x10000, size: 0x7e0000 },
  { type: 'app-ota_1', offset: 0x7f0000, size: 0x7e0000 },
  { type: 'data-spiffs', offset: 0xfd0000, size: 0x14000 },
  { type: 'data-coredump', offset: 0xfe4000, size: 0x1c000 },
];

export const MODELS = {
  c3: { name: 'X3 or X4', chip: 'ESP32-C3', efuse: { secureReg: 0x60008838, cryptReg: 0x60008834 }, layout: X3_PARTITION_TABLE },
  x4c: { name: 'X4 Classic', chip: 'ESP32-S3', efuse: { secureReg: 0x60007038, cryptReg: 0x60007034 }, layout: S3_PARTITION_TABLE },
  x4pro: { name: 'X4 Pro', chip: 'ESP32-S3', efuse: { secureReg: 0x60007038, cryptReg: 0x60007034 }, layout: S3_PARTITION_TABLE },
};

// --- OTA Partition ---

// IDF otadata format: exactly two 4 KB flash sectors, each holding one
// esp_ota_select_entry_t at byte 0. This is a protocol constant, not a
// layout choice, so it's hardcoded rather than read from the PT.
const OTA_SECTOR_BYTES = 0x1000;
const OTADATA_BYTES = 2 * OTA_SECTOR_BYTES;

const OTA_STATE = { NEW: 0, PENDING_VERIFY: 1, VALID: 2, INVALID: 3, ABORTED: 4, UNDEFINED: 0xFFFFFFFF };
const INVALID_STATES = new Set([OTA_STATE.INVALID, OTA_STATE.ABORTED]);

function parseOtaPartitionSlot(data, offset) {
  const seqBytes = data.slice(offset, offset + 4);
  const sequence = leBytesToU32(seqBytes);
  const stateVal = leBytesToU32(data.slice(offset + 0x18, offset + 0x1C));
  const crcBytes = data.slice(offset + 0x1C, offset + 0x20);
  const expectedCrc = generateCrc32Le(sequence);
  return { sequence, state: stateVal, crcValid: isEqualBytes(crcBytes, expectedCrc) };
}

// IDF OTA model: active app slot = (active_seq - 1) % NUM_OTA_PARTITIONS.
// The otadata sector that holds the active entry has no fixed relation to
// the app slot it boots; the new entry goes into the OTHER sector. Pairing
// sector index with app label drifts out of sync once otadata leaves
// canonical state (interrupted OTA, a prior write that used the wrong
// mapping) and silently writes firmware into the slot the bootloader is
// about to skip.
export function parseOtadata(data) {
  if (data.length !== OTADATA_BYTES) throw new Error('OTA data is truncated.');
  const slot0 = parseOtaPartitionSlot(data, 0);
  const slot1 = parseOtaPartitionSlot(data, 0x1000);

  if ([slot0, slot1].some(slot => slot.crcValid && slot.sequence === 0)) {
    throw new Error('OTA data has an invalid zero sequence.');
  }
  const eligible = [];
  if (slot0.sequence !== 0xFFFFFFFF && slot0.crcValid && !INVALID_STATES.has(slot0.state)) {
    eligible.push({ sector: 0, seq: slot0.sequence });
  }
  if (slot1.sequence !== 0xFFFFFFFF && slot1.crcValid && !INVALID_STATES.has(slot1.state)) {
    eligible.push({ sector: 1, seq: slot1.sequence });
  }
  eligible.sort((a, b) => b.seq - a.seq);

  let activeSector, activeSeq, activeApp;
  if (eligible.length === 0) {
    activeSector = -1;
    activeSeq = 0;
    activeApp = 0;
  } else {
    activeSector = eligible[0].sector;
    activeSeq = eligible[0].seq;
    activeApp = (activeSeq - 1) % 2;
  }
  const inactiveApp = 1 - activeApp;
  // Smallest seq > activeSeq with (newSeq - 1) % 2 == inactiveApp. For an
  // existing active entry this is always activeSeq + 1; for the no-eligible
  // case (factory-fresh otadata, activeSeq = 0, activeApp = 0 by IDF
  // default) it has to step to 2 so the bootloader picks inactiveApp = 1.
  let newSeq = activeSeq + 1;
  while (((newSeq - 1) % 2) !== inactiveApp) newSeq++;
  const targetSector = activeSector < 0 ? 0 : (1 - activeSector);

  if (newSeq >= 0xFFFFFFFF) throw new Error('OTA sequence is exhausted.');
  return {
    slot0, slot1,
    activeApp, inactiveApp,
    activeSeq, newSeq,
    targetSector,
  };
}

export function otaStateName(state) {
  switch (state) {
    case OTA_STATE.NEW: return 'new';
    case OTA_STATE.PENDING_VERIFY: return 'pending_verify';
    case OTA_STATE.VALID: return 'valid';
    case OTA_STATE.INVALID: return 'invalid';
    case OTA_STATE.ABORTED: return 'aborted';
    case OTA_STATE.UNDEFINED: return 'undefined';
    default: return `unknown (${state})`;
  }
}

// Builds the 4 KB target sector with the new entry stamped at byte 0. The
// other sector is left untouched on flash so a power cut during the erase /
// write window leaves the old (still-valid) entry as a fallback boot
// pointer, instead of blanking both sectors and forcing the bootloader's
// "no eligible otadata" default (app0).
//
// State NEW preserves rollback where supported; X3 bootloaders ignore it.
export function buildNewOtadataSector(existingSectorData, newSeq) {
  const newData = new Uint8Array(existingSectorData);
  newData.set(u32ToLeBytes(newSeq), 0);
  newData.set(u32ToLeBytes(OTA_STATE.NEW), 0x18);
  newData.set(generateCrc32Le(newSeq), 0x1C);
  return newData;
}

function assertOtadataSwitch(ota, expectedApp, expectedSeq) {
  if (ota.activeSeq !== expectedSeq || ota.activeApp !== expectedApp) {
    throw new Error(
      `OTA boot selector did not verify after write. Expected app${expectedApp} via seq ${expectedSeq}, ` +
      `got app${ota.activeApp} via seq ${ota.activeSeq} ` +
      `(slot0 seq ${ota.slot0.sequence} crc ${ota.slot0.crcValid ? 'ok' : 'bad'}, ` +
      `slot1 seq ${ota.slot1.sequence} crc ${ota.slot1.crcValid ? 'ok' : 'bad'}).`
    );
  }
}

const PARTITION_TYPES = {
  0x00: { 0x00: 'app-factory', 0x10: 'app-ota_0', 0x11: 'app-ota_1' },
  0x01: { 0x00: 'data-ota', 0x01: 'data-phy', 0x02: 'data-nvs', 0x03: 'data-coredump', 0x82: 'data-spiffs' },
};

export function parsePartitionTable(data) {
  const partitions = [];
  for (let offset = 0; offset < data.length; offset += 32) {
    const chunk = data.slice(offset, offset + 32);
    if (chunk.length !== 32) break;
    let allFF = true;
    for (let i = 0; i < 32; i++) { if (chunk[i] !== 0xFF) { allFF = false; break; } }
    if (allFF) break;
    if (chunk[0] === 0xEB && chunk[1] === 0xEB) break;
    if (chunk[0] !== 0xAA || chunk[1] !== 0x50) throw new Error('Invalid partition table magic.');

    const type = PARTITION_TYPES[chunk[2]]?.[chunk[3]] || 'unknown';
    const off = leBytesToU32(chunk.slice(4, 8));
    const size = leBytesToU32(chunk.slice(8, 12));
    partitions.push({ type, offset: off, size });
  }
  return partitions;
}


export const FLASH_BYTES = 0x1000000;

// Only the selected known layout is accepted: no device-supplied write addresses.
export function requireLayout(partitions, layout) {
  if (JSON.stringify(partitions) !== JSON.stringify(layout)) {
    throw new Error('This reader has an unsupported partition layout.');
  }
  return partitions;
}

export function pickBootAppPartition(partitions, raw, layout = X3_PARTITION_TABLE) {
  requireLayout(partitions, layout);
  const app = raw ? parseOtadata(raw).activeApp : 0;
  return partitions.find(p => p.type === `app-ota_${app}`);
}

export class ReaderFlasher {
  constructor(port, modelKey) {
    if (!Object.hasOwn(MODELS, modelKey)) throw new Error('Unknown reader model.');
    this.port = port; this.model = MODELS[modelKey]; this.loader = null;
  }

  async createLoader() {
    const { ESPLoader, Transport } = await import('./esptool.bundle.js');
    return new ESPLoader({ transport: new Transport(this.port, false),
      baudrate: 115200, romBaudrate: 115200, enableTracing: false });
  }

  async connect() {
    this.loader = await this.createLoader();
    await this.loader.main();
    const chip = this.loader.chip?.CHIP_NAME;
    if (chip !== this.model.chip) {
      if (chip === 'ESP32-S3' && this.model.chip === 'ESP32-C3') {
        throw new Error('This is an X4 Classic or X4 Pro. Choose it above and connect again.');
      }
      if (chip === 'ESP32-C3' && this.model.chip === 'ESP32-S3') {
        throw new Error('This is an X3 or X4. Choose it above and connect again.');
      }
      throw new Error('This reader is not supported.');
    }
    if (await this.loader.getFlashSize() !== FLASH_BYTES / 1024) {
      throw new Error('This reader does not have 16 MB of flash.');
    }
    // eFuse registers: Espressif esptool target definitions.
    // Vendored getFlashSize() returns KiB; readReg() allows two cheap checks.
    const secure = await this.loader.readReg(this.model.efuse.secureReg);
    const crypt = await this.loader.readReg(this.model.efuse.cryptReg);
    const count = (crypt & (7 << 18)) >>> 18;
    const encrypted = ((count & 1) + ((count >>> 1) & 1) + ((count >>> 2) & 1)) % 2;
    if ((secure & (1 << 20)) || encrypted) {
      throw new Error('This reader is locked. Installing could brick it, so we stopped.');
    }
    await this.readTarget();
  }

  async readTarget() {
    requireLayout(parsePartitionTable(await this.loader.readFlash(0x8000, 0x1000)), this.model.layout);
    this.otadata = await this.loader.readFlash(0xe000, OTADATA_BYTES);
    this.ota = parseOtadata(this.otadata);
  }

  async backup(progress) {
    // Chunk reads avoid esptool's repeated concatenation of a whole 16 MB dump.
    const data = new Uint8Array(FLASH_BYTES);
    const chunk = 0x10000;
    for (let offset = 0; offset < FLASH_BYTES; offset += chunk) {
      const bytes = await this.loader.readFlash(offset, chunk);
      if (bytes.length !== chunk) throw new Error('The backup read was incomplete.');
      const expected = await this.loader.flashMd5sum(offset, chunk);
      if (this.loader.toHex(md5(bytes)) !== expected) throw new Error('Backup verification failed.');
      data.set(bytes, offset);
      progress((offset + chunk) / FLASH_BYTES * 100);
    }
    return data;
  }

  async write(data, address, progress) {
    // Explicit allowlist, including sector-rounded app writes within each slot.
    const slot = this.model.layout.find(p => p.offset === address && p.type.startsWith('app-'));
    const ota = (address === 0xe000 || address === 0xf000) && data.length === OTA_SECTOR_BYTES;
    if (!ota && (!slot || data.length === 0 || Math.ceil(data.length / 4096) * 4096 > slot.size)) {
      throw new Error('Unsafe flash write was blocked.');
    }
    await this.loader.writeFlash({
      fileArray: [{ data: this.loader.ui8ToBstr(data), address }],
      flashSize: 'keep', flashMode: 'keep', flashFreq: 'keep',
      eraseAll: false, compress: true,
      calculateMD5Hash: binary => this.loader.toHex(md5(this.loader.bstrToUi8(binary))),
      reportProgress: (_, written, total) => progress(written / total * 100),
    });
  }

  async install(data, progress) {
    await validateFirmwareImage(data);
    if (imageChipName(data) !== this.model.chip) throw new Error(`Firmware is not for ${this.model.chip}.`);
    await this.readTarget();
    const { inactiveApp, newSeq, targetSector } = this.ota;
    const slot = this.model.layout.find(p => p.type === `app-ota_${inactiveApp}`);
    if (data.length > slot.size) throw new Error('Firmware is too large for this reader.');
    await this.write(data, slot.offset, value => progress(value * 0.95));
    const start = targetSector * OTA_SECTOR_BYTES;
    const sector = buildNewOtadataSector(this.otadata.subarray(start, start + OTA_SECTOR_BYTES), newSeq);
    await this.write(sector, 0xe000 + start, value => progress(95 + value * 0.04));
    assertOtadataSwitch(parseOtadata(await this.loader.readFlash(0xe000, OTADATA_BYTES)), inactiveApp, newSeq);
    progress(100);
  }

  async disconnect() {
    if (!this.loader) return;
    try { await this.loader.after('no_reset_stub'); }
    finally {
      const transport = this.loader.transport;
      try {
        // The vendored disconnect cancels but does not release its reader lock.
        if (transport.reader) {
          try { await transport.reader.cancel(); }
          finally { transport.reader.releaseLock(); transport.reader = undefined; }
        }
      } finally {
        try { await transport.disconnect(); }
        finally { this.loader = null; }
      }
    }
  }
}
