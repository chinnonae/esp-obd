// Pure OBDb signal decoding: bit extraction, scaling/mapping, and
// `ratio` synthetics. No DOM/localStorage/serial dependency (see D04).

/**
 * Reads a single bit from a byte array, counting bit 0 as the MSB of byte 0.
 */
function getBit(bytes, bitPosition) {
  const byteIndex = bitPosition >> 3;
  const bitInByte = bitPosition & 7;
  return (bytes[byteIndex] >> (7 - bitInByte)) & 1;
}

/**
 * Extracts an unsigned (or two's-complement signed) integer of `len` bits
 * starting at bit offset `bix` (MSB-first) from `bytes`.
 *
 * `blsb` (byte-swapped) reverses the order of the bytes spanned by the field
 * before extraction -- correct for the byte-aligned multi-byte fields OBDb
 * signalsets use it for.
 */
export function extractBits(bytes, bix, len, { blsb = false, sign = false } = {}) {
  const startByte = bix >> 3;
  const startBitInByte = bix & 7;
  const spanBytes = Math.ceil((startBitInByte + len) / 8);

  let window = Array.from(bytes.slice(startByte, startByte + spanBytes));
  if (blsb) {
    window = window.slice().reverse();
  }

  let raw = 0;
  for (let i = 0; i < len; i++) {
    raw = (raw << 1) | getBit(window, startBitInByte + i);
  }

  if (sign && len < 32 && (raw & (1 << (len - 1))) !== 0) {
    raw -= 1 << len;
  }

  return raw;
}

/**
 * Applies a signal's `fmt` scaling (`mul`/`div`/`add`) or `map` lookup to a
 * raw extracted integer, returning the final numeric or mapped-string value.
 */
export function applyScale(rawValue, fmt) {
  if (fmt.map) {
    const entry = fmt.map[String(rawValue)];
    return entry ? entry.value : undefined;
  }

  let value = rawValue;
  if (fmt.mul !== undefined) {
    value *= fmt.mul;
  }
  if (fmt.div !== undefined) {
    value /= fmt.div;
  }
  if (fmt.add !== undefined) {
    value += fmt.add;
  }
  return value;
}

/**
 * Decodes every signal in a `commands[]` entry from the response's data
 * bytes (already stripped of the `41 <pid>`/`62 <pid>` echo by the caller).
 */
export function decodeResponse(command, rawBytes) {
  const values = {};
  for (const signal of command.signals) {
    const { fmt } = signal;
    const bix = fmt.bix ?? 0;
    const rawValue = extractBits(rawBytes, bix, fmt.len, { blsb: fmt.blsb, sign: fmt.sign });
    values[signal.id] = applyScale(rawValue, fmt);
  }
  return values;
}

/**
 * Computes any `ratio` synthetics whose inputs are both present in
 * `signalValues`. Never throws on missing inputs -- just omits that
 * synthetic from the result.
 */
export function computeSynthetics(signalValues, synthetics) {
  const result = {};
  for (const synthetic of synthetics ?? []) {
    const { formula } = synthetic;
    if (!formula || formula.op !== "ratio") {
      continue;
    }
    const a = signalValues[formula.a];
    const b = signalValues[formula.b];
    if (typeof a !== "number" || typeof b !== "number") {
      continue;
    }
    result[synthetic.id] = a / b;
  }
  return result;
}
