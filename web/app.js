(() => {
  const POLL_INTERVAL_MS = 1000;
  const PICKER_ITEM_HEIGHT = 48;
  const PICKER_MAX_VALUE = 100;
  const PICKER_HALF_HEIGHT = 120;
  const PICKER_EDGE_PADDING = 8;
  const PICKER_HIDE_MS = 420;
  const TARGET_CONFIRM_ANIMATION_MS = 500;
  const SECONDARY_CARD_LONG_PRESS_MS = 300;
  const PAGE_SCREEN_TRANSITION_MS = 350;
  const PAGE_PRIMARY_ENTER_SETTLE_MS = 1200;
  const PAGE_SECONDARY_ENTER_SETTLE_MS = 1400;
  const POWER_STATUS_SYNC_GRACE_MS = 3000;
  const PAGE_PRIMARY_BACK_BTN_DELAY_MS = 40;
  const SYSTEM_STATUS_STORAGE_KEY = "smartDehumidifier:lastSystemStatus";

  const els = {
    onlineBadge: document.getElementById("onlineBadge"),
    tempNumber: document.getElementById("tempNumber"),
    tempUnit: document.getElementById("tempUnit"),
    humidityNumber: document.getElementById("humidityNumber"),
    humidityUnit: document.getElementById("humidityUnit"),
    iaqValue: document.getElementById("iaqValue"),
    waterLevelValue: document.getElementById("waterLevelValue"),
    secondaryTempValue: document.getElementById("secondaryTempValue"),
    secondaryHumidityValue: document.getElementById("secondaryHumidityValue"),
    secondaryTempStatusText: document.getElementById("secondaryTempStatusText"),
    secondaryHumidityStatusText: document.getElementById("secondaryHumidityStatusText"),
    secondaryIaqStatusText: document.getElementById("secondaryIaqStatusText"),
    secondaryIaqValue: document.getElementById("secondaryIaqValue"),
    secondaryWaterStatusText: document.getElementById("secondaryWaterStatusText"),
    secondaryWaterLevelValue: document.getElementById("secondaryWaterLevelValue"),
    uvToggleBtn: document.getElementById("uvToggleBtn"),
    autoToggleBtn: document.getElementById("autoToggleBtn"),
    manualChangeBtn: document.getElementById("manualChangeBtn"),
    powerValue: document.getElementById("powerValue"),
    powerButtonsWrap: document.querySelector(".power-buttons"),
    targetHumidityWrap: document.getElementById("targetHumidityWrap"),
    targetHumidityClip: document.getElementById("targetHumidityClip"),
    targetDisplayBtn: document.getElementById("targetDisplayBtn"),
    targetValueText: document.getElementById("targetValueText"),
    pickerBackdrop: document.getElementById("pickerBackdrop"),
    humidityPicker: document.getElementById("humidityPicker"),
    humidityPickerScroll: document.getElementById("humidityPickerScroll"),
    lastUpdateText: document.getElementById("lastUpdateText"),
    backBtn:
      document.getElementById("backBtn") ||
      document.querySelector("button[aria-label='Back']"),
    startBtn:
      document.getElementById("startBtn") ||
      document.querySelector(".start-btn"),
    pageViewPrimary: document.getElementById("pageViewPrimary"),
    pageViewSecondary: document.getElementById("pageViewSecondary"),
  };
  const powerButtons = Array.from(document.querySelectorAll(".power-btn"));
  let pickerItems = [];
  const POWER_LEVELS = [0, 40, 70, 100];
  const MANUAL_CHANGE_MIN_ANIMATION_MS = 2700;

  let stateEndpointAvailable = true;
  let manualChangeBusy = false;
  let autoModeEnabled = false;
  let targetHumidity = 65;
  let tempHumidity = 65;
  let pickerOpen = false;
  let pickerScrollRaf = null;
  let pickerHideTimer = null;
  let lockedScrollY = 0;
  let targetConfirmTimer = null;
  let showingPrimaryPage = false;
  let pageTransitionInFlight = false;
  let pageExitTimer = null;
  let pageEnterTimer = null;
  let backBtnEnterTimer = null;
  let pendingStatus = "";
  let pendingStatusTimer = null;
  let lastKnownSystemStatus = "";

  function clearPageTransitionTimers() {
    if (pageExitTimer) {
      clearTimeout(pageExitTimer);
      pageExitTimer = null;
    }
    if (pageEnterTimer) {
      clearTimeout(pageEnterTimer);
      pageEnterTimer = null;
    }
    if (backBtnEnterTimer) {
      clearTimeout(backBtnEnterTimer);
      backBtnEnterTimer = null;
    }
  }

  function clearPendingStatus() {
    pendingStatus = "";
    if (pendingStatusTimer) {
      clearTimeout(pendingStatusTimer);
      pendingStatusTimer = null;
    }
  }

  function normalizeSystemStatus(status) {
    const normalized = String(status || "").toLowerCase();
    if (normalized === "running" || normalized === "standby") return normalized;
    return "";
  }

  function persistSystemStatus(status) {
    const normalized = normalizeSystemStatus(status);
    if (!normalized) return;
    lastKnownSystemStatus = normalized;
    try {
      window.localStorage.setItem(SYSTEM_STATUS_STORAGE_KEY, normalized);
    } catch (_err) {
      // Ignore storage write failures in restrictive browsers.
    }
  }

  function restoreSystemStatus() {
    try {
      const cached = window.localStorage.getItem(SYSTEM_STATUS_STORAGE_KEY);
      const normalized = normalizeSystemStatus(cached);
      if (normalized) return normalized;
    } catch (_err) {
      // Ignore storage read failures in restrictive browsers.
    }
    return "";
  }

  function setPendingStatus(status) {
    clearPendingStatus();
    pendingStatus = normalizeSystemStatus(status);
    if (!pendingStatus) return;
    persistSystemStatus(pendingStatus);
    pendingStatusTimer = window.setTimeout(() => {
      clearPendingStatus();
    }, POWER_STATUS_SYNC_GRACE_MS);
  }

  function resetPageTransitionState(page) {
    if (!page) return;
    page.classList.remove("is-entering", "is-exiting");
  }

  function setHeaderControlVisible(control, visible) {
    if (!control) return;
    control.hidden = false;
    control.classList.toggle("is-nav-hidden", !visible);
    control.setAttribute("aria-hidden", visible ? "false" : "true");
  }

  function finalizeActivePage(showPrimary) {
    showingPrimaryPage = showPrimary;
    resetPageTransitionState(els.pageViewPrimary);
    resetPageTransitionState(els.pageViewSecondary);
    if (els.pageViewPrimary) {
      els.pageViewPrimary.classList.toggle("is-active", showPrimary);
      els.pageViewPrimary.setAttribute("aria-hidden", showPrimary ? "false" : "true");
    }
    if (els.pageViewSecondary) {
      els.pageViewSecondary.classList.toggle("is-active", !showPrimary);
      els.pageViewSecondary.setAttribute("aria-hidden", showPrimary ? "true" : "false");
    }
    setHeaderControlVisible(els.backBtn, showPrimary);
  }

  function setActivePage(showPrimary, immediate = false) {
    const primaryPage = els.pageViewPrimary;
    const secondaryPage = els.pageViewSecondary;

    if (!primaryPage || !secondaryPage) {
      finalizeActivePage(showPrimary);
      return;
    }

    if (immediate) {
      clearPageTransitionTimers();
      pageTransitionInFlight = false;
      finalizeActivePage(showPrimary);
      return;
    }

    if (pageTransitionInFlight || showPrimary === showingPrimaryPage) return;

    const currentPage = showingPrimaryPage ? primaryPage : secondaryPage;
    const nextPage = showPrimary ? primaryPage : secondaryPage;
    const enterSettleMs = showPrimary ? PAGE_PRIMARY_ENTER_SETTLE_MS : PAGE_SECONDARY_ENTER_SETTLE_MS;

    pageTransitionInFlight = true;
    clearPageTransitionTimers();
    resetPageTransitionState(currentPage);
    resetPageTransitionState(nextPage);

    currentPage.classList.remove("is-active");
    currentPage.classList.add("is-exiting");
    currentPage.setAttribute("aria-hidden", "true");

    if (!showPrimary) {
      setHeaderControlVisible(els.backBtn, false);
    }

    pageExitTimer = window.setTimeout(() => {
      resetPageTransitionState(currentPage);
      nextPage.classList.add("is-entering");
      nextPage.setAttribute("aria-hidden", "false");

      if (showPrimary) {
        setHeaderControlVisible(els.backBtn, false);
        backBtnEnterTimer = window.setTimeout(() => {
          setHeaderControlVisible(els.backBtn, true);
          backBtnEnterTimer = null;
        }, PAGE_PRIMARY_BACK_BTN_DELAY_MS);
      }

      pageEnterTimer = window.setTimeout(() => {
        showingPrimaryPage = showPrimary;
        resetPageTransitionState(nextPage);
        nextPage.classList.add("is-active");
        pageTransitionInFlight = false;
        pageExitTimer = null;
        pageEnterTimer = null;
      }, enterSettleMs);
    }, PAGE_SCREEN_TRANSITION_MS);
  }

  function waterLevelText(value) {
    if (value === "empty") return "empty";
    if (value === "normal") return "normal";
    if (value === "full" || value === "fulll") return "full";
    if (value === "error") return "error";
    return value || "unknown";
  }

  async function sendCommand(url) {
    const response = await fetch(url, {
      method: "GET",
      cache: "no-store",
      headers: {
        "X-Requested-With": "fetch",
      },
    });
    if (!response.ok) {
      throw new Error(`Command failed: ${response.status}`);
    }
  }

  function wait(ms) {
    return new Promise((resolve) => setTimeout(resolve, ms));
  }

  function clampHumidity(value) {
    const n = Number(value);
    if (!Number.isFinite(n)) return 0;
    return Math.min(PICKER_MAX_VALUE, Math.max(0, Math.round(n)));
  }

  function setToggleButton(button, enabled) {
    if (!button) return;
    button.dataset.state = enabled ? "on" : "off";
    button.setAttribute("aria-checked", enabled ? "true" : "false");
  }

  function normalizePowerLevel(value) {
    const level = Number(value);
    if (!Number.isFinite(level)) return POWER_LEVELS[0];
    if (POWER_LEVELS.includes(level)) return level;

    let nearest = POWER_LEVELS[0];
    let minDistance = Math.abs(level - nearest);
    for (let i = 1; i < POWER_LEVELS.length; i += 1) {
      const distance = Math.abs(level - POWER_LEVELS[i]);
      if (distance < minDistance) {
        minDistance = distance;
        nearest = POWER_LEVELS[i];
      }
    }
    return nearest;
  }

  function setPowerLevelUI(value) {
    const level = normalizePowerLevel(value);
    const index = POWER_LEVELS.indexOf(level);

    if (els.powerButtonsWrap && index >= 0) {
      els.powerButtonsWrap.style.setProperty("--power-index", String(index));
    }

    powerButtons.forEach((button) => {
      const isActive = Number(button.getAttribute("data-level")) === level;
      button.classList.toggle("is-active", isActive);
      button.setAttribute("aria-pressed", isActive ? "true" : "false");
    });

    if (els.powerValue) {
      els.powerValue.textContent = String(level);
    }
  }

  function setTargetHumidityValue(value, animate = false) {
    targetHumidity = clampHumidity(value);
    if (!els.targetValueText) return;
    els.targetValueText.textContent = String(targetHumidity);
    if (animate) {
      triggerTargetConfirmFeedback();
    }
  }

  function triggerTargetConfirmFeedback() {
    if (!els.targetDisplayBtn || !els.targetValueText) return;
    els.targetDisplayBtn.classList.remove("confirm-pop");
    els.targetValueText.classList.remove("confirm-flash");
    // Force reflow so repeated confirmations replay animation.
    void els.targetDisplayBtn.offsetWidth;
    els.targetDisplayBtn.classList.add("confirm-pop");
    els.targetValueText.classList.add("confirm-flash");
    if (targetConfirmTimer) clearTimeout(targetConfirmTimer);
    targetConfirmTimer = window.setTimeout(() => {
      if (els.targetDisplayBtn) els.targetDisplayBtn.classList.remove("confirm-pop");
      if (els.targetValueText) els.targetValueText.classList.remove("confirm-flash");
    }, TARGET_CONFIRM_ANIMATION_MS);
  }

  function updatePickerSelection() {
    pickerItems.forEach((item) => {
      const value = Number(item.dataset.value);
      item.classList.toggle("is-selected", value === tempHumidity);
    });
  }

  function scrollPickerTo(value, behavior = "smooth") {
    if (!els.humidityPickerScroll) return;
    const top = clampHumidity(value) * PICKER_ITEM_HEIGHT;
    els.humidityPickerScroll.scrollTo({ top, behavior });
  }

  function buildHumidityPicker() {
    if (!els.humidityPickerScroll) return;
    const fragment = document.createDocumentFragment();

    const topSpacer = document.createElement("div");
    topSpacer.className = "picker-spacer";
    fragment.appendChild(topSpacer);

    for (let i = 0; i <= PICKER_MAX_VALUE; i += 1) {
      const row = document.createElement("div");
      row.className = "picker-item";
      row.dataset.value = String(i);

      const text = document.createElement("span");
      text.className = "picker-item-value";
      text.textContent = String(i);

      row.appendChild(text);
      fragment.appendChild(row);
    }

    const bottomSpacer = document.createElement("div");
    bottomSpacer.className = "picker-spacer";
    fragment.appendChild(bottomSpacer);

    els.humidityPickerScroll.innerHTML = "";
    els.humidityPickerScroll.appendChild(fragment);
    pickerItems = Array.from(els.humidityPickerScroll.querySelectorAll(".picker-item"));
  }

  function lockPageScroll() {
    if (document.body.classList.contains("picker-lock")) return;
    lockedScrollY = window.scrollY || window.pageYOffset || 0;
    document.body.style.top = `-${lockedScrollY}px`;
    document.body.classList.add("picker-lock");
  }

  function unlockPageScroll() {
    if (!document.body.classList.contains("picker-lock")) return;
    document.body.classList.remove("picker-lock");
    document.body.style.top = "";
    window.scrollTo(0, lockedScrollY);
  }

  function alignPickerHotZoneToDisplay() {
    if (!els.humidityPicker || !els.targetDisplayBtn) return;
    const rect = els.targetDisplayBtn.getBoundingClientRect();
    const centerY = rect.top + rect.height / 2;
    const clampedY = Math.max(
      PICKER_HALF_HEIGHT + PICKER_EDGE_PADDING,
      Math.min(window.innerHeight - PICKER_HALF_HEIGHT - PICKER_EDGE_PADDING, centerY)
    );
    els.humidityPicker.style.top = `${clampedY}px`;
  }

  function closePicker() {
    if (!els.humidityPicker || !els.pickerBackdrop) return;
    if (!pickerOpen && els.humidityPicker.hidden) return;

    pickerOpen = false;
    if (els.targetDisplayBtn) {
      els.targetDisplayBtn.setAttribute("aria-expanded", "false");
    }
    if (els.targetHumidityClip) {
      els.targetHumidityClip.classList.remove("picker-open");
    }

    els.pickerBackdrop.classList.remove("show");
    els.humidityPicker.classList.remove("show");
    els.humidityPicker.classList.add("hiding");

    if (pickerHideTimer) clearTimeout(pickerHideTimer);
    pickerHideTimer = window.setTimeout(() => {
      if (!els.humidityPicker || !els.pickerBackdrop) return;
      els.pickerBackdrop.hidden = true;
      els.humidityPicker.hidden = true;
      els.humidityPicker.classList.remove("hiding");
      unlockPageScroll();
    }, PICKER_HIDE_MS);
  }

  function openPicker() {
    if (!autoModeEnabled) return;
    if (!els.humidityPicker || !els.pickerBackdrop) return;
    if (pickerOpen) return;

    pickerOpen = true;
    tempHumidity = targetHumidity;
    updatePickerSelection();

    if (els.targetDisplayBtn) {
      els.targetDisplayBtn.setAttribute("aria-expanded", "true");
    }
    if (els.targetHumidityClip) {
      els.targetHumidityClip.classList.add("picker-open");
    }

    if (pickerHideTimer) clearTimeout(pickerHideTimer);
    lockPageScroll();
    alignPickerHotZoneToDisplay();
    els.pickerBackdrop.hidden = false;
    els.humidityPicker.hidden = false;
    els.humidityPicker.classList.remove("hiding");

    scrollPickerTo(tempHumidity, "auto");

    requestAnimationFrame(() => {
      if (!els.pickerBackdrop || !els.humidityPicker) return;
      els.pickerBackdrop.classList.add("show");
      els.humidityPicker.classList.add("show");
    });
  }

  async function confirmTargetHumidity() {
    setTargetHumidityValue(tempHumidity, true);
    closePicker();
    await sendCommand(`/set_target?value=${encodeURIComponent(String(targetHumidity))}`);
  }

  function setAutoModeUI(enabled) {
    autoModeEnabled = enabled;
    setToggleButton(els.autoToggleBtn, enabled);

    if (els.targetHumidityWrap) {
      els.targetHumidityWrap.classList.toggle("is-open", enabled);
      els.targetHumidityWrap.setAttribute("aria-hidden", enabled ? "false" : "true");
    }

    if (!enabled) closePicker();
  }

  function setManualChangeBusy(busy) {
    manualChangeBusy = busy;
    if (!els.manualChangeBtn) return;
    els.manualChangeBtn.classList.toggle("is-busy", busy);
    els.manualChangeBtn.disabled = busy;
    els.manualChangeBtn.setAttribute("aria-busy", busy ? "true" : "false");
  }

  function isRunningStatus(status) {
    return String(status || "").toLowerCase() === "running";
  }

  function clearStartButtonFocus() {
    if (els.startBtn && document.activeElement === els.startBtn) {
      els.startBtn.blur();
    }
  }

  function temperatureComfortText(value) {
    if (!Number.isFinite(value)) return "Ideal";
    if (value < 18) return "Chilly";
    if (value <= 26) return "Ideal";
    return "Warm";
  }

  function humidityComfortText(value) {
    if (!Number.isFinite(value)) return "Ideal";
    if (value < 40) return "Dry";
    if (value <= 60) return "Ideal";
    return "Humid";
  }

  function iaqComfortText(value) {
    const text = String(value ?? "").trim().toLowerCase();
    if (text === "preheating") return "Waiting";

    const numericValue = Number(value);
    if (!Number.isFinite(numericValue)) return "Waiting";
    if (numericValue <= 100) return "Fresh";
    if (numericValue <= 200) return "Comfortable";
    return "Stale";
  }

  function waterLevelStatusText(value) {
    const normalized = String(value || "").trim().toLowerCase();
    if (normalized === "empty") return "Need Water";
    if (normalized === "normal") return "Sufficient";
    if (normalized === "full" || normalized === "fulll") return "Perfect";
    if (normalized === "error") return "Check Sensor";
    return "Sufficient";
  }

  function resolveSystemStatus(state) {
    const reportedStatus = normalizeSystemStatus(state?.status);
    if (reportedStatus) return reportedStatus;
    // Keep the last known mode so browser refresh does not reset page state.
    return lastKnownSystemStatus;
  }

  function bindSecondaryCardFlip() {
    const cards = Array.from(document.querySelectorAll(".secondary-grid .secondary-card"));
    cards.forEach((card) => {
      let longPressTimer = null;

      card.addEventListener("pointerdown", (event) => {
        event.preventDefault();
        clearStartButtonFocus();
        longPressTimer = window.setTimeout(() => {
          card.classList.add("is-flipped");
        }, SECONDARY_CARD_LONG_PRESS_MS);
      });

      card.addEventListener("pointerup", () => {
        clearStartButtonFocus();
        if (longPressTimer) {
          clearTimeout(longPressTimer);
          longPressTimer = null;
        }
        card.classList.remove("is-flipped");
      });

      card.addEventListener("pointerleave", () => {
        clearStartButtonFocus();
        if (longPressTimer) {
          clearTimeout(longPressTimer);
          longPressTimer = null;
        }
      });

      card.addEventListener("pointercancel", () => {
        clearStartButtonFocus();
        if (longPressTimer) {
          clearTimeout(longPressTimer);
          longPressTimer = null;
        }
        card.classList.remove("is-flipped");
      });

      card.addEventListener("contextmenu", (event) => {
        event.preventDefault();
        clearStartButtonFocus();
      });
    });
  }

  function renderState(state) {
    if (!state || typeof state !== "object") return;

    let systemStatus = resolveSystemStatus(state);
    if (pendingStatus) {
      if (systemStatus === pendingStatus) {
        clearPendingStatus();
      } else {
        // Optimistic switch: keep requested state until device confirms or grace timeout.
        systemStatus = pendingStatus;
      }
    }
    if (systemStatus) {
      persistSystemStatus(systemStatus);
    }

    if (els.onlineBadge) {
      els.onlineBadge.textContent = systemStatus || "syncing";
      els.onlineBadge.dataset.state = systemStatus || "offline";
    }

    const t = Number(state.temperature);
    const tempText = Number.isFinite(t) ? t.toFixed(1) : "0.0";
    if (els.tempNumber) {
      els.tempNumber.textContent = tempText;
    }
    if (els.secondaryTempValue) {
      els.secondaryTempValue.textContent = tempText;
    }
    if (els.secondaryTempStatusText) {
      els.secondaryTempStatusText.textContent = temperatureComfortText(t);
    }
    if (els.tempUnit) {
      els.tempUnit.textContent = "°C";
    }

    const h = Number(state.humidity);
    const humidityText = Number.isFinite(h) ? h.toFixed(1) : "0.0";
    if (els.humidityNumber) {
      els.humidityNumber.textContent = humidityText;
    }
    if (els.secondaryHumidityValue) {
      els.secondaryHumidityValue.textContent = humidityText;
    }
    if (els.secondaryHumidityStatusText) {
      els.secondaryHumidityStatusText.textContent = humidityComfortText(h);
    }
    if (els.humidityUnit) {
      els.humidityUnit.textContent = "%";
    }

    const iaqText = String(state.iaq ?? 0);
    const iaqNormalized = iaqText.trim().toLowerCase();
    const iaqIsNumeric = /^-?\d+(\.\d+)?$/.test(iaqText.trim());
    const iaqDisplayText = iaqNormalized === "preheating" ? "preheat\ning" : iaqText;
    if (els.iaqValue) {
      els.iaqValue.textContent = iaqDisplayText;
    }
    if (els.secondaryIaqValue) {
      els.secondaryIaqValue.textContent = iaqDisplayText;
      els.secondaryIaqValue.classList.toggle("is-text", !iaqIsNumeric);
    }
    if (els.secondaryIaqStatusText) {
      els.secondaryIaqStatusText.textContent = iaqComfortText(state.iaq);
    }

    const waterText = waterLevelText(String(state.waterLevel || ""));
    if (els.waterLevelValue) {
      els.waterLevelValue.textContent = waterText;
    }
    if (els.secondaryWaterLevelValue) {
      els.secondaryWaterLevelValue.textContent = waterText;
    }
    if (els.secondaryWaterStatusText) {
      els.secondaryWaterStatusText.textContent = waterLevelStatusText(state.waterLevel);
    }

    const uvOn = String(state.uv || "off").toLowerCase() === "on";
    setToggleButton(els.uvToggleBtn, uvOn);

    const autoOn = String(state.autoMode || "off").toLowerCase() === "on";
    setAutoModeUI(autoOn);

    const p = Number(state.powerLevel);
    setPowerLevelUI(Number.isFinite(p) ? p : 0);

    if (Object.prototype.hasOwnProperty.call(state, "targetHumidity")) {
      const nextTarget = clampHumidity(state.targetHumidity);
      setTargetHumidityValue(nextTarget, false);
      if (!pickerOpen) {
        tempHumidity = nextTarget;
      }
    }

    if (els.lastUpdateText) {
      const sec = Number(state.secondsSinceUpdate);
      const shown = Number.isFinite(sec) && sec >= 0 ? Math.floor(sec) : 0;
      els.lastUpdateText.textContent = `LAST UPDATE: ${shown}S AGO`;
    }

    const normalizedSystemStatus = normalizeSystemStatus(systemStatus);
    if (normalizedSystemStatus) {
      const shouldShowPrimary = isRunningStatus(normalizedSystemStatus);
      if (shouldShowPrimary !== showingPrimaryPage && !pageTransitionInFlight) {
        setActivePage(shouldShowPrimary);
      }
    }

  }

  async function fetchState() {
    if (!stateEndpointAvailable) return;

    try {
      const response = await fetch("/state", { cache: "no-store" });
      if (response.status === 404) {
        stateEndpointAvailable = false;
        return;
      }
      if (!response.ok) return;
      const state = await response.json();
      renderState(state);
    } catch (err) {
      // Keep UI usable even if state endpoint is unavailable.
      console.warn("State polling failed:", err);
    }
  }

  function bindActions() {
    setActivePage(showingPrimaryPage, true);
    setToggleButton(els.uvToggleBtn, false);
    setAutoModeUI(false);
    setManualChangeBusy(false);
    buildHumidityPicker();
    bindSecondaryCardFlip();
    setTargetHumidityValue(targetHumidity, false);
    tempHumidity = targetHumidity;
    updatePickerSelection();

    if (els.uvToggleBtn) {
      els.uvToggleBtn.addEventListener("click", async () => {
        const on = els.uvToggleBtn.dataset.state === "on";
        await sendCommand(on ? "/uv_off" : "/uv_on");
        setToggleButton(els.uvToggleBtn, !on);
      });
    }

    if (els.autoToggleBtn) {
      els.autoToggleBtn.addEventListener("click", async () => {
        const on = els.autoToggleBtn.dataset.state === "on";
        await sendCommand(on ? "/auto_off" : "/auto_on");
        setAutoModeUI(!on);
      });
    }

    if (els.manualChangeBtn) {
      els.manualChangeBtn.addEventListener("click", async () => {
        if (manualChangeBusy) return;
        setManualChangeBusy(true);
        try {
          await Promise.all([
            sendCommand("/manual_change"),
            wait(MANUAL_CHANGE_MIN_ANIMATION_MS),
          ]);
        } finally {
          setManualChangeBusy(false);
        }
      });
    }

    setPowerLevelUI(0);

    powerButtons.forEach((button) => {
      button.addEventListener("click", async () => {
        const level = Number(button.getAttribute("data-level"));
        if (!Number.isFinite(level)) return;
        if (autoModeEnabled) {
          setAutoModeUI(false);
          await sendCommand("/auto_off");
        }
        await sendCommand(`/set_strength?level=${encodeURIComponent(String(level))}`);
        setPowerLevelUI(level);
      });
    });

    if (els.targetDisplayBtn) {
      els.targetDisplayBtn.addEventListener("click", () => {
        if (!autoModeEnabled) return;
        openPicker();
      });
    }

    if (els.pickerBackdrop) {
      els.pickerBackdrop.addEventListener("click", () => {
        closePicker();
      });
    }

    if (els.humidityPickerScroll) {
      els.humidityPickerScroll.addEventListener("scroll", () => {
        if (pickerScrollRaf) return;
        pickerScrollRaf = window.requestAnimationFrame(() => {
          pickerScrollRaf = null;
          if (!els.humidityPickerScroll) return;
          tempHumidity = clampHumidity(Math.round(els.humidityPickerScroll.scrollTop / PICKER_ITEM_HEIGHT));
          updatePickerSelection();
        });
      });
    }

    if (els.humidityPickerScroll) {
      els.humidityPickerScroll.addEventListener("click", async (event) => {
        const row = event.target instanceof Element ? event.target.closest(".picker-item") : null;
        if (!row) return;
        const picked = clampHumidity(row.getAttribute("data-value"));
        if (picked !== tempHumidity) {
          tempHumidity = picked;
          updatePickerSelection();
          scrollPickerTo(picked, "smooth");
          return;
        }
        await confirmTargetHumidity();
      });
    }

    document.addEventListener("keydown", (event) => {
      if (event.key === "Escape" && pickerOpen) closePicker();
    });

    window.addEventListener("resize", () => {
      if (pickerOpen) alignPickerHotZoneToDisplay();
    });

    if (els.backBtn) {
      els.backBtn.addEventListener("click", async () => {
        if (pageTransitionInFlight) return;
        if (!showingPrimaryPage) return;
        setPendingStatus("standby");
        setActivePage(false);
        try {
          await sendCommand("/power_standby");
          await fetchState();
        } catch (err) {
          clearPendingStatus();
          await fetchState();
          console.warn("Power standby request failed:", err);
        }
      });
    }

    if (els.startBtn) {
      els.startBtn.addEventListener("click", async () => {
        clearStartButtonFocus();
        if (pageTransitionInFlight) return;
        if (showingPrimaryPage) return;
        setPendingStatus("running");
        setActivePage(true);
        try {
          await sendCommand("/power_running");
          await fetchState();
        } catch (err) {
          clearPendingStatus();
          await fetchState();
          console.warn("Power running request failed:", err);
        }
      });
    }
  }

  function bootstrap() {
    const restoredSystemStatus = restoreSystemStatus();
    if (restoredSystemStatus) {
      persistSystemStatus(restoredSystemStatus);
      showingPrimaryPage = isRunningStatus(restoredSystemStatus);
      if (els.onlineBadge) {
        els.onlineBadge.textContent = restoredSystemStatus;
        els.onlineBadge.dataset.state = restoredSystemStatus;
      }
    } else if (els.onlineBadge) {
      els.onlineBadge.textContent = "syncing";
      els.onlineBadge.dataset.state = "offline";
    }
    bindActions();
    if (restoredSystemStatus) {
      setActivePage(showingPrimaryPage, true);
    }
    fetchState();
    setInterval(fetchState, POLL_INTERVAL_MS);
  }

  document.addEventListener("DOMContentLoaded", bootstrap);
})();
