// ELM327 init sequence and header-switch helper, built on serial.js.

const INIT_COMMANDS = ["ATZ", "ATE0", "ATL0", "ATH0"];
const ERROR_PATTERN = /^(NO DATA|UNABLE TO CONNECT|ERROR|BUS INIT|\?)/i;

let lastHeader = null;
let lastRax = null;

/**
 * Runs the fixed ELM327 init sequence. `ATZ`'s response is the adapter's
 * banner text (not `OK`) -- any non-error-looking response is accepted for
 * that step specifically; the rest must not look like an ELM327 error.
 */
export async function initSession(serial) {
  lastHeader = null;
  lastRax = null;
  for (const command of INIT_COMMANDS) {
    const response = await serial.sendCommand(command);
    if (ERROR_PATTERN.test(response.trim())) {
      throw new Error(`ELM327 init failed on ${command}: ${response}`);
    }
  }
}

/**
 * Issues ATSH<hdr> / ATCRA<rax> only if either differs from the last call,
 * so a poll loop over many commands sharing a header pays no extra
 * round trips.
 */
export async function ensureHeader(serial, hdr, rax) {
  if (hdr !== lastHeader) {
    await serial.sendCommand(`ATSH${hdr}`);
    lastHeader = hdr;
  }
  if (rax !== lastRax) {
    await serial.sendCommand(`ATCRA${rax}`);
    lastRax = rax;
  }
}
