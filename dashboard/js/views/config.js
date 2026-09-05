// Thin wrapper over config-store.js: upload a signalset file, list/select/
// delete profiles. Never touches the serial port or poll engine directly.

import * as configStore from "../config-store.js";

let listEl = null;
let statusEl = null;

function setStatus(text, isError) {
  statusEl.textContent = text;
  statusEl.className = isError ? "config-status config-status--error" : "config-status";
}

function render() {
  const profiles = configStore.listProfiles();
  const activeId = configStore.getActiveProfileId();
  listEl.innerHTML = "";

  for (const profile of profiles) {
    const isActive = profile.id === activeId;
    const row = document.createElement("div");
    row.className = `profile-row${isActive ? " profile-row--active" : ""}`;

    const nameEl = document.createElement("span");
    nameEl.className = "profile-name";
    nameEl.textContent = isActive ? `${profile.name} (active)` : profile.name;
    row.appendChild(nameEl);

    const activateButton = document.createElement("button");
    activateButton.type = "button";
    activateButton.textContent = "Make active";
    activateButton.disabled = isActive;
    activateButton.addEventListener("click", () => {
      configStore.setActiveProfileId(profile.id);
      render();
    });
    row.appendChild(activateButton);

    if (!profile.isBuiltin) {
      const deleteButton = document.createElement("button");
      deleteButton.type = "button";
      deleteButton.textContent = "Delete";
      deleteButton.addEventListener("click", () => {
        configStore.deleteProfile(profile.id);
        render();
      });
      row.appendChild(deleteButton);
    }

    listEl.appendChild(row);
  }
}

async function handleFileUpload(file) {
  let json;
  try {
    json = JSON.parse(await file.text());
  } catch (err) {
    setStatus(`Could not parse "${file.name}" as JSON: ${err.message}`, true);
    return;
  }

  const name = file.name.replace(/\.json$/i, "");
  const result = configStore.saveUploadedProfile(name, json);
  if (result.error) {
    setStatus(result.error, true);
    return;
  }
  setStatus(`Uploaded "${name}".`, false);
  render();
}

export function init() {
  const root = document.getElementById("view-config");
  root.innerHTML = "";

  const uploadLabel = document.createElement("label");
  uploadLabel.className = "config-upload";
  uploadLabel.append("Upload signalset: ");
  const fileInput = document.createElement("input");
  fileInput.type = "file";
  fileInput.accept = ".json,application/json";
  uploadLabel.appendChild(fileInput);

  statusEl = document.createElement("div");
  statusEl.className = "config-status";

  listEl = document.createElement("div");
  listEl.className = "profile-list";

  const attribution = document.createElement("p");
  attribution.className = "config-attribution";
  const attributionLink = document.createElement("a");
  attributionLink.href = "https://github.com/OBDb/SAEJ1979";
  attributionLink.target = "_blank";
  attributionLink.rel = "noopener";
  attributionLink.textContent = "OBDb/SAEJ1979";
  attribution.append("Built-in generic OBD-II profile: ", attributionLink, " (CC BY-SA 4.0).");

  root.append(uploadLabel, statusEl, listEl, attribution);

  fileInput.addEventListener("change", () => {
    const file = fileInput.files[0];
    if (file) {
      handleFileUpload(file);
    }
    fileInput.value = "";
  });

  render();
}
