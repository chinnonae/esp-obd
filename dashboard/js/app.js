// Bootstraps the app shell: service worker registration, tab switching, and
// the Connect button wired to the Web Serial + ELM327 init flow (or the
// in-browser simulator, for local dev/testing without real hardware).

import * as serial from "./serial.js";
import * as simulator from "./simulator.js";
import * as elm from "./elm.js";

function registerServiceWorker() {
  if (!("serviceWorker" in navigator)) {
    return;
  }
  window.addEventListener("load", () => {
    navigator.serviceWorker.register("./sw.js").catch((err) => {
      console.error("Service worker registration failed:", err);
    });
  });
}

function initTabs() {
  const tabButtons = document.querySelectorAll(".tab-button");
  const views = document.querySelectorAll(".view");

  tabButtons.forEach((button) => {
    button.addEventListener("click", () => {
      const target = button.dataset.tab;

      tabButtons.forEach((btn) => btn.setAttribute("aria-selected", String(btn === button)));
      views.forEach((view) => {
        view.hidden = view.dataset.view !== target;
      });
    });
  });
}

function setStatus(statusEl, text, variant) {
  statusEl.textContent = text;
  statusEl.className = `status status--${variant}`;
}

function initConnectButton() {
  const connectButton = document.getElementById("connect-button");
  const statusEl = document.getElementById("connection-status");
  const simToggle = document.getElementById("simulator-toggle");

  const hasWebSerial = "serial" in navigator;
  if (!hasWebSerial) {
    simToggle.checked = true;
    simToggle.disabled = true; // Web Serial unavailable -- simulator is the only option
  }

  let transport = null; // whichever module (serial.js or simulator.js) is currently connected

  async function doConnect(mod) {
    connectButton.disabled = true;
    simToggle.disabled = true;
    setStatus(statusEl, "Connecting...", "disconnected");
    try {
      await mod.connect();
      await elm.initSession(mod);
      transport = mod;
      setStatus(statusEl, mod === simulator ? "Connected (simulator)" : "Connected", "connected");
      connectButton.textContent = "Disconnect";
    } catch (err) {
      console.error("Connect failed:", err);
      setStatus(statusEl, "Connect failed", "error");
      await mod.disconnect().catch(() => {});
      simToggle.disabled = !hasWebSerial;
    } finally {
      connectButton.disabled = false;
    }
  }

  connectButton.addEventListener("click", async () => {
    if (transport?.isConnected()) {
      await transport.disconnect();
      transport = null;
      setStatus(statusEl, "Disconnected", "disconnected");
      connectButton.textContent = "Connect";
      simToggle.disabled = !hasWebSerial;
      return;
    }
    await doConnect(simToggle.checked ? simulator : serial);
  });

  // Try to reopen a previously authorized real port without a new user gesture.
  if (hasWebSerial) {
    serial.tryReconnect().then((reconnected) => {
      if (reconnected) {
        elm
          .initSession(serial)
          .then(() => {
            transport = serial;
            setStatus(statusEl, "Connected", "connected");
            connectButton.textContent = "Disconnect";
            simToggle.disabled = true;
          })
          .catch((err) => {
            console.error("Reconnect init failed:", err);
            setStatus(statusEl, "Connect failed", "error");
          });
      }
    });
  }
}

registerServiceWorker();
initTabs();
initConnectButton();
