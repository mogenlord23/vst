// MUTAFORM v2 UI — pop-art 9:16. Vanilla JS. Talks to C++ through JUCE relays + native functions.
import * as Juce from "./juce/index.js";
import { CharacterView } from "./character.js";

const NL = 5;
const $ = (s, r = document) => r.querySelector(s);
const $$ = (s, r = document) => [...r.querySelectorAll(s)];
const el = (tag, cls, html) => { const e = document.createElement(tag); if (cls) e.className = cls; if (html != null) e.innerHTML = html; return e; };
const fnCache = {};
const call = (name, ...a) => (fnCache[name] ??= Juce.getNativeFunction(name))(...a);
const lid = (i, s) => `l${i + 1}_${s}`;
const state = { model: null, sel: 0, scale: 1, meters: null, peaks: {}, dragging: false, lastNote: -1, isFx: false };

// ---------------------------------------------------------------- relays
const sliders = {}, toggles = {}, combos = {};
const slider = (id) => (sliders[id] ??= Juce.getSliderState(id));
const toggle = (id) => (toggles[id] ??= Juce.getToggleState(id));
const combo = (id) => (combos[id] ??= Juce.getComboBoxState(id));
const onS = (id, f) => { const s = slider(id); s.valueChangedEvent.addListener(f); s.propertiesChangedEvent.addListener(f); };
const onT = (id, f) => { const s = toggle(id); s.valueChangedEvent.addListener(f); s.propertiesChangedEvent.addListener(f); };
const onC = (id, f) => { const s = combo(id); s.valueChangedEvent.addListener(f); s.propertiesChangedEvent.addListener(f); };
function toNorm(st, v) { const p = st.properties; if (p.end === p.start) return 0; return Math.pow(Math.min(1, Math.max(0, (v - p.start) / (p.end - p.start))), p.skew); }
function setScaled(id, v) { const st = slider(id); st.sliderDragStarted(); st.setNormalisedValue(toNorm(st, v)); st.sliderDragEnded(); }
const fmt = {
  pct: (v) => `${Math.round(v * 100)}%`, pct100: (v) => `${Math.round(v)}%`,
  db: (v) => (v <= -47.9 ? "-INF" : `${v > 0 ? "+" : ""}${v.toFixed(1)}DB`),
  ms: (v) => (v >= 1000 ? `${(v / 1000).toFixed(2)}S` : `${Math.round(v)}MS`),
  hz: (v) => (v >= 1000 ? `${(v / 1000).toFixed(1)}K` : `${Math.round(v)}HZ`),
  pan: (v) => (Math.abs(v) < 0.01 ? "C" : v < 0 ? `L${Math.round(-v * 100)}` : `R${Math.round(v * 100)}`),
};

// ---------------------------------------------------------------- icons (CSS masks)
const ICON = {
  load: '<path d="M4 18h16M12 3v11m-5-5 5 5 5-5" stroke="#000" stroke-width="3" fill="none" stroke-linecap="round" stroke-linejoin="round"/>',
  layers: '<path d="M12 3 2 8l10 5 10-5zm-10 9 10 5 10-5M2 16l10 5 10-5" stroke="#000" stroke-width="2.6" fill="none" stroke-linejoin="round"/>',
  star: '<path d="M12 2l3 7 7 .6-5.4 4.7 1.7 7.2L12 17.8 5.7 21.5l1.7-7.2L2 9.6 9 9z"/>',
  disk: '<path d="M3 3h15l3 3v15H3zM7 3v6h9V3M7 21v-7h10v7" stroke="#000" stroke-width="2.6" fill="none" stroke-linejoin="round"/>',
  dice: '<rect x="3" y="3" width="18" height="18" rx="4" stroke="#000" stroke-width="2.6" fill="none"/><circle cx="8" cy="8" r="1.8"/><circle cx="16" cy="16" r="1.8"/><circle cx="16" cy="8" r="1.8"/><circle cx="8" cy="16" r="1.8"/><circle cx="12" cy="12" r="1.8"/>',
  face: '<circle cx="12" cy="12" r="9" stroke="#000" stroke-width="2.6" fill="none"/><circle cx="9" cy="10" r="1.6"/><circle cx="15" cy="10" r="1.6"/><path d="M8 14.5q4 4 8 0" stroke="#000" stroke-width="2.4" fill="none" stroke-linecap="round"/>',
};
function installIcons() {
  let css = "";
  for (const [k, v] of Object.entries(ICON)) {
    const url = `url("data:image/svg+xml;utf8,${encodeURIComponent(`<svg xmlns='http://www.w3.org/2000/svg' viewBox='0 0 24 24'>${v}</svg>`)}")`;
    css += `.ti-${k}{-webkit-mask-image:${url};mask-image:${url}}`;
  }
  document.head.appendChild(el("style", null, css));
}

// ---------------------------------------------------------------- glossy pop knob (SVG)
const KCOL = { lime: ["#E8FF8A", "#B8F400", "#7FA800"], blue: ["#8FC4FF", "#1E7BFF", "#0B4BB0"], orange: ["#FFD08A", "#FF8A00", "#B85E00"],
  pink: ["#FF9AD8", "#FF1FA8", "#A80068"], yellow: ["#FFF6A0", "#FFE600", "#C9A800"] };
let kid = 0;
function dialSvg(color) {
  const [a, b, c] = KCOL[color] || KCOL.pink; const g = "kg" + (kid++);
  return `<svg viewBox="0 0 100 100"><defs><radialGradient id="${g}" cx="45%" cy="35%" r="70%"><stop offset="0" stop-color="${a}"/><stop offset=".55" stop-color="${b}"/><stop offset="1" stop-color="${c}"/></radialGradient></defs>
    <circle cx="50" cy="50" r="46" fill="url(#${g})" stroke="#141014" stroke-width="6"/>
    <path class="arc" fill="none" stroke="#141014" stroke-width="5" stroke-linecap="round" opacity=".35"/>
    <circle cx="50" cy="50" r="27" fill="#fff" stroke="#141014" stroke-width="5"/>
    <g class="ptr"><rect x="46" y="10" width="8" height="26" rx="4" fill="#141014"/></g>
    <ellipse cx="36" cy="24" rx="14" ry="7" fill="#fff" opacity=".55"/></svg>`;
}
function arcPath(a0, a1, r = 40) {
  const p = (a) => [50 + r * Math.cos((a - 90) * Math.PI / 180), 50 + r * Math.sin((a - 90) * Math.PI / 180)];
  const [x0, y0] = p(a0), [x1, y1] = p(a1);
  return `M${x0} ${y0} A${r} ${r} 0 ${a1 - a0 > 180 ? 1 : 0} 1 ${x1} ${y1}`;
}
/** Knob bound to a slider (float/int) or a combo (choice) parameter. */
function knob(parent, id, label, opts = {}) {
  const isCombo = !!opts.combo;
  const w = el("div", opts.mini ? "mk" : "kn");
  const dial = el("div", "dial", dialSvg(opts.color || "pink"));
  w.append(dial, el("span", "lab", label), el("span", "val"));
  const ptr = dial.querySelector(".ptr"), arc = dial.querySelector(".arc"), val = w.querySelector(".val");
  const norm = () => {
    if (isCombo) { const c = combo(id); const n = Math.max(1, (c.properties.choices || []).length - 1); return c.getChoiceIndex() / n; }
    return slider(id).getNormalisedValue();
  };
  const render = () => {
    const n = norm(), a = -135 + n * 270;
    ptr.setAttribute("transform", `rotate(${a} 50 50)`);
    arc.setAttribute("d", n > 0.002 ? arcPath(-135, a) : "");
    if (isCombo) { const c = combo(id); val.textContent = (c.properties.choices || [])[c.getChoiceIndex()] || ""; }
    else val.textContent = (opts.fmt || fmt.pct)(slider(id).getScaledValue());
  };
  (isCombo ? onC : onS)(id, render); render();
  let y0 = 0, n0 = 0, drag = false;
  dial.addEventListener("pointerdown", (e) => { if (e.button) return; dial.setPointerCapture(e.pointerId); drag = true; y0 = e.clientY; n0 = norm(); if (!isCombo) slider(id).sliderDragStarted(); });
  dial.addEventListener("pointermove", (e) => {
    if (!drag) return;
    const n = Math.min(1, Math.max(0, n0 + (y0 - e.clientY) * (isCombo ? 0.009 : (e.shiftKey ? 0.0015 : 0.006)) / state.scale));
    if (isCombo) { const c = combo(id); const cnt = (c.properties.choices || []).length; const i = Math.round(n * (cnt - 1)); if (i !== c.getChoiceIndex()) c.setChoiceIndex(i); }
    else slider(id).setNormalisedValue(n);
  });
  const end = () => { if (!drag) return; drag = false; if (!isCombo) slider(id).sliderDragEnded(); };
  dial.addEventListener("pointerup", end); dial.addEventListener("pointercancel", end);
  dial.addEventListener("dblclick", () => { if (opts.def != null && !isCombo) setScaled(id, opts.def); });
  dial.addEventListener("wheel", (e) => {
    e.preventDefault();
    if (isCombo) { const c = combo(id); const cnt = (c.properties.choices || []).length; c.setChoiceIndex(Math.max(0, Math.min(cnt - 1, c.getChoiceIndex() - Math.sign(e.deltaY)))); }
    else { const s = slider(id); s.sliderDragStarted(); s.setNormalisedValue(Math.min(1, Math.max(0, s.getNormalisedValue() - Math.sign(e.deltaY) * 0.03))); s.sliderDragEnded(); }
  }, { passive: false });
  parent.appendChild(w);
  return w;
}
function tog(parent, id, label, onChange) {
  const b = el("button", "tog", label);
  const r = () => { const v = !!toggle(id).getValue(); b.classList.toggle("on", v); onChange?.(v); };
  onT(id, r); r(); b.addEventListener("click", () => toggle(id).setValue(!toggle(id).getValue()));
  parent.appendChild(b); return b;
}
function chips(parent, id, labels) {
  const w = el("div", "chips");
  const bs = labels.map((l, k) => { const b = el("button", "", l); b.addEventListener("click", () => combo(id).setChoiceIndex(k)); w.appendChild(b); return b; });
  const r = () => bs.forEach((b, k) => b.classList.toggle("on", combo(id).getChoiceIndex() === k));
  onC(id, r); r(); parent.appendChild(w); return w;
}
function unit(parent, title, onId) {
  const u = el("div", "unit"), h = el("div", "uh", `<span>${title}</span>`), b = el("div", "ub");
  u.append(h, b);
  if (onId) tog(h, onId, "ON", (v) => u.classList.toggle("off", !v));
  parent.appendChild(u); return { u, h, b };
}

// ---------------------------------------------------------------- toast / bubble
let toastT = 0, bubbleT = 0, bubbleSticky = "";
function toast(m) { const t = $("#toast"); t.textContent = String(m).toUpperCase(); t.classList.add("show"); clearTimeout(toastT); toastT = setTimeout(() => t.classList.remove("show"), 2200); }
function say(text, ms = 2400) {
  const b = $("#bubble"); b.textContent = text; b.classList.remove("hide");
  clearTimeout(bubbleT);
  if (ms > 0) bubbleT = setTimeout(() => { if (bubbleSticky) say(bubbleSticky, 0); else b.classList.add("hide"); }, ms);
}
const QUIPS = ["YUMMY!", "SO STRETCHY!", "LA LA LAAA!", "POP!", "WHAT A TUNE!", "MORE SOUNDS!", "I'M A STAR!", "WHEEE!"];

// ---------------------------------------------------------------- scaling
function fit() {
  const s = Math.min(window.innerWidth / 450, window.innerHeight / 800);
  state.scale = s > 0 ? s : 1;
  $("#app").style.transform = `scale(${state.scale})`;
  $("#app").style.left = `${Math.max(0, (window.innerWidth - 450 * state.scale) / 2)}px`;
}

// ---------------------------------------------------------------- character + ribbon animation
let char = null;
const anim = { armL: 20, armR: -20, tl: 20, tr: -20, blinkUntil: 0, nextBlink: 2, mouthUntil: 0, catchT: 0, noteKick: 0 };
function animate(t) {
  const M = state.model;
  const loaded = M?.layers?.some((l) => l.loaded);
  const busy = M?.layers?.some((l) => l.busy) || (state.meters?.geno ?? -1) >= 0;
  const playing = (state.meters?.voices || 0) > 0;
  const note = state.meters?.note ?? -1;
  if (playing && note >= 0 && note !== state.lastNote) { state.lastNote = note; anim.mouthUntil = t + 0.18; anim.noteKick = 1; }
  if (!playing) state.lastNote = -1;
  anim.noteKick = Math.max(0, anim.noteKick - 0.06);

  if (state.dragging) { anim.tl = 165 + Math.sin(t * 9) * 8; anim.tr = -165 - Math.sin(t * 9 + 1) * 8; }      // arms up: "give it to me"
  else if (t < anim.catchT) { anim.tl = 120; anim.tr = -120; }                                                    // catching
  else if (!loaded) { anim.tl = 18 + Math.sin(t * 1.5) * 4; anim.tr = -18 - Math.sin(t * 1.5) * 4; }             // idle, empty hands
  else if (playing) {                                                                                             // stretch the sample
    const pitch = note >= 0 ? (note - 60) / 12 : 0;
    const spread = 72 + 26 * Math.abs(Math.sin(t * 4.2)) + 18 * anim.noteKick;
    const lift = 16 * pitch + Math.sin(t * 6) * 6;
    anim.tl = spread + lift; anim.tr = -spread + lift;
  } else { anim.tl = 62 + Math.sin(t * 2) * 3; anim.tr = -62 - Math.sin(t * 2) * 3; }                              // holding
  anim.armL += (anim.tl - anim.armL) * 0.25;
  anim.armR += (anim.tr - anim.armR) * 0.25;
  if (t > anim.nextBlink) { anim.blinkUntil = t + 0.13; anim.nextBlink = t + 2.5 + Math.random() * 3.5; }
  const mouth = state.dragging ? "wide" : busy ? "open" : t < anim.mouthUntil ? "wide" : playing ? "open" : "closed";
  char.set({ armL: anim.armL, armR: anim.armR, blink: t < anim.blinkUntil || busy, mouth,
    bob: Math.sin(t * (playing ? 8 : 2)) * (playing ? 4 : 2) - anim.noteKick * 4, tilt: busy ? Math.sin(t * 30) * 2 : Math.sin(t * 0.9) * 1.5 });
  drawRibbon(t, loaded && !state.dragging && t >= anim.catchT - 0.1, playing);
}

function drawRibbon(t, holding, playing) {
  const c = $("#ribbon"); const g = c.getContext("2d");
  g.clearRect(0, 0, c.width, c.height);
  if (!holding) return;
  // character box is 300x420 design units drawn bottom-aligned into the 300x400 slot
  const sy = 400 / 420;
  const [hl, hr] = char.hands().map(([x, y]) => [x * sy + (300 - 300 * sy) / 2 + 30, y * sy - 170]);
  const P = state.peaks[state.sel]?.mini;
  const len = Math.hypot(hr[0] - hl[0], hr[1] - hl[1]) || 1;
  const steps = 60, thick = 34 + (playing ? 8 * Math.sin(t * 10) : 0), sag = playing ? 4 : 22;
  let pk = 0.05; if (P) for (const v of P) pk = Math.max(pk, Math.abs(v));
  const pts = [];
  for (let i = 0; i <= steps; i++) {
    const u = i / steps;
    const x = hl[0] + (hr[0] - hl[0]) * u, y = hl[1] + (hr[1] - hl[1]) * u + Math.sin(u * Math.PI) * sag;
    let amp = 0.25;
    if (P) { const k = Math.min(P.length / 2 - 1, Math.floor(u * P.length / 2)); amp = Math.max(Math.abs(P[k * 2]), Math.abs(P[k * 2 + 1])) / pk; }
    const wob = playing ? Math.sin(u * 18 - t * 12) * 0.15 : 0;
    pts.push([x, y, (thick * 0.5) * (0.3 + amp * 0.85 + wob) * (0.55 + 0.45 * Math.sin(u * Math.PI))]);
  }
  const nx = -(hr[1] - hl[1]) / len, ny = (hr[0] - hl[0]) / len;
  g.beginPath();
  pts.forEach(([x, y, h], i) => (i ? g.lineTo(x + nx * h, y + ny * h) : g.moveTo(x + nx * h, y + ny * h)));
  for (let i = pts.length - 1; i >= 0; i--) { const [x, y, h] = pts[i]; g.lineTo(x - nx * h, y - ny * h); }
  g.closePath();
  const grad = g.createLinearGradient(0, Math.min(hl[1], hr[1]) - 20, 0, Math.max(hl[1], hr[1]) + 30);
  grad.addColorStop(0, "#FF8AD0"); grad.addColorStop(0.5, "#FF1FA8"); grad.addColorStop(1, "#C4007A");
  g.fillStyle = grad; g.fill();
  g.lineWidth = 4; g.strokeStyle = "#141014"; g.lineJoin = "round"; g.stroke();
  g.beginPath(); pts.forEach(([x, y, h], i) => (i ? g.lineTo(x + nx * h * 0.45, y + ny * h * 0.45 - 2) : g.moveTo(x + nx * h * 0.45, y + ny * h * 0.45 - 2)));
  g.strokeStyle = "rgba(255,255,255,.65)"; g.lineWidth = 3; g.stroke();
}

// ---------------------------------------------------------------- main controls
function buildKnobs() {
  const k = $("#knobs");
  knob(k, "mel_key", "KEY", { combo: true, color: "lime" });
  knob(k, "mel_scale", "SCALE", { combo: true, color: "blue" });
  knob(k, "mel_mood", "MOOD", { color: "orange", def: 0.4, fmt: (v) => (v < 0.33 ? "CHILL" : v < 0.66 ? "GROOVY" : "WILD") });
  knob(k, "mel_density", "DENSITY", { color: "pink", def: 0.5, fmt: fmt.pct });
}
const NOTE_NAMES = ["C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"];
function noteName(m) { const n = Math.round(m); return NOTE_NAMES[((n % 12) + 12) % 12] + (Math.floor(n / 12) - 2); }
function renderStatus() {
  const M = state.model; if (!M) return;
  const kc = combo("mel_key"), sc = combo("mel_scale");
  const keyName = (kc.properties.choices || [])[kc.getChoiceIndex()] || "", scaleName = (sc.properties.choices || [])[sc.getChoiceIndex()] || "";
  const L = M.layers[state.sel];
  const pitch = L?.loaded && L.pitchConf >= 0.5 ? ` · SOUND ${noteName(L.pitch)}` : "";
  $("#status").innerHTML = `<b>${keyName} ${scaleName.toUpperCase()}</b> · LAYER ${state.sel + 1}${pitch} · LEVEL ${M.level}`;
}
async function doEvolve() {
  if (!state.model?.layers?.some((l) => l.loaded)) { say("FEED ME A SOUND FIRST!"); return; }
  if (!(await call("evolveAll", 0x1f))) toast("Busy...");
}

// ---------------------------------------------------------------- layers overlay
const rows = [];
function buildLayers() {
  const body = $("#layersBody");
  for (let i = 0; i < NL; i++) {
    const r = el("div", "lrow empty");
    r.innerHTML = `<div class="num">${i + 1}</div><div class="lw"><span class="ln">EMPTY</span><canvas></canvas></div>
      <div class="lc">
        <button data-a="load">LOAD</button><button data-a="loop">LOOP</button><button data-a="reverse">REV</button>
        <button data-a="mute" class="red">M</button><button data-a="solo">S</button>
        <button data-a="octd">OCT-</button><button data-a="octu">OCT+</button>
        <button data-a="lock">LOCK</button><button data-a="geno">GENO</button><button data-a="export">WAV</button><button data-a="clear">X</button>
      </div><div class="lc hist"></div>`;
    r.addEventListener("click", (e) => {
      selectLayer(i);
      const b = e.target.closest("button"); if (!b) return;
      const a = b.dataset.a, t = (s) => toggle(lid(i, s)).setValue(!toggle(lid(i, s)).getValue());
      if (a === "load") call("browseSample", i);
      else if (a === "loop") t("loop"); else if (a === "reverse") t("reverse"); else if (a === "mute") t("mute"); else if (a === "solo") t("solo");
      else if (a === "octd" || a === "octu") { const id = lid(i, "semi"); setScaled(id, Math.max(-24, Math.min(24, Math.round(slider(id).getScaledValue()) + (a === "octu" ? 12 : -12)))); }
      else if (a === "lock") call("setLocked", i, !state.model.layers[i].locked);
      else if (a === "geno") startGeno(i);
      else if (a === "export") call("exportLayer", i);
      else if (a === "clear") call("clearLayer", i);
      else if (b.dataset.k != null) call("jumpHistory", i, +b.dataset.k);
    });
    r.addEventListener("dragenter", (e) => { e.preventDefault(); dropLayer = i; });
    body.appendChild(r);
    rows.push(r);
    ["loop", "reverse", "mute", "solo"].forEach((s) => onT(lid(i, s), () => renderRow(i)));
  }
}
function renderRow(i) {
  const r = rows[i]; if (!r) return;
  const L = state.model?.layers?.[i];
  r.classList.toggle("empty", !L?.loaded); r.classList.toggle("sel", i === state.sel);
  r.querySelector(".ln").textContent = L?.loaded ? `${L.name} · ${L.seconds.toFixed(2)}S${L.pitchConf >= 0.5 ? " · " + noteName(L.pitch) : ""}${L.index > 0 ? " · VAR " + L.index : ""}` : "EMPTY — DROP A SOUND";
  const on = (a, v) => r.querySelector(`[data-a=${a}]`).classList.toggle("on", !!v);
  on("loop", toggle(lid(i, "loop")).getValue()); on("reverse", toggle(lid(i, "reverse")).getValue());
  on("mute", toggle(lid(i, "mute")).getValue()); on("solo", toggle(lid(i, "solo")).getValue()); on("lock", L?.locked);
  const h = r.querySelector(".hist"); h.innerHTML = "";
  if (L?.loaded) L.history.forEach((e, k) => { const b = el("button", k === L.index ? "cur" : "", k === 0 ? "ORIG" : (e.label.startsWith("GENOFORM") ? "G" : "") + k); b.dataset.k = k; b.title = `${e.name}\n${e.label}`; h.appendChild(b); });
  const c = r.querySelector("canvas"); const rc = c.getBoundingClientRect();
  if (!rc.width) return;
  c.width = Math.max(1, Math.round(rc.width / state.scale)); c.height = Math.max(1, Math.round(rc.height / state.scale));
  const g = c.getContext("2d"); g.clearRect(0, 0, c.width, c.height);
  const P = state.peaks[i]?.mini;
  if (P) {
    let pk = 0.05; P.forEach((v) => (pk = Math.max(pk, Math.abs(v)))); const n = P.length / 2;
    g.fillStyle = toggle(lid(i, "mute")).getValue() ? "#bbb" : "#FF1FA8";
    for (let x = 0; x < c.width; x++) { const k = Math.min(n - 1, Math.floor(x / c.width * n)); const a = Math.max(Math.abs(P[k * 2]), Math.abs(P[k * 2 + 1])) / pk; const hh = Math.max(1, a * c.height * 0.42); g.fillRect(x, c.height / 2 - hh, 1, hh * 2); }
  }
}
function selectLayer(i) { state.sel = i; call("selectLayer", i); rows.forEach((_, k) => renderRow(k)); $$(".strip").forEach((s, k) => (s.style.background = k === i ? "#FFF2B0" : "")); renderStatus(); }

// ---------------------------------------------------------------- FX / MIX overlays
function buildFx() {
  const b = $("#fxBody");
  let u = unit(b, "FILTER", "flt_on"); chips(u.h, "flt_type", ["LP", "HP", "BP"]);
  knob(u.b, "flt_cutoff", "CUTOFF", { mini: true, color: "yellow", fmt: fmt.hz, def: 8000 }); knob(u.b, "flt_res", "RESO", { mini: true, color: "yellow", def: 0.2 }); knob(u.b, "flt_mix", "MIX", { mini: true, color: "yellow", def: 1 });
  u = unit(b, "DRIVE", "dist_on");
  knob(u.b, "dist_drive", "DRIVE", { mini: true, color: "orange", def: 0.3 }); knob(u.b, "dist_mix", "MIX", { mini: true, color: "orange", def: 1 });
  u = unit(b, "CHORUS", "cho_on");
  knob(u.b, "cho_rate", "RATE", { mini: true, color: "lime", fmt: fmt.hz, def: 0.6 }); knob(u.b, "cho_depth", "DEPTH", { mini: true, color: "lime", def: 0.35 }); knob(u.b, "cho_mix", "MIX", { mini: true, color: "lime", def: 0.5 });
  u = unit(b, "DELAY", "dly_on"); tog(u.h, "dly_sync", "SYNC");
  knob(u.b, "dly_div", "NOTE", { mini: true, combo: true, color: "blue" }); knob(u.b, "dly_time", "TIME", { mini: true, color: "blue", fmt: fmt.ms, def: 350 });
  knob(u.b, "dly_fb", "FEEDBK", { mini: true, color: "blue", def: 0.35 }); knob(u.b, "dly_mix", "MIX", { mini: true, color: "blue", def: 0.25 });
  u = unit(b, "REVERB", "rev_on");
  knob(u.b, "rev_size", "SIZE", { mini: true, color: "pink", def: 0.6 }); knob(u.b, "rev_damp", "DAMP", { mini: true, color: "pink", def: 0.4 });
  knob(u.b, "rev_width", "WIDTH", { mini: true, color: "pink", def: 1 }); knob(u.b, "rev_mix", "MIX", { mini: true, color: "pink", def: 0.25 });
  u = unit(b, "MUTATION POOL (EVOLVE)");
  [["evo_pitch", "PITCH"], ["evo_grain", "GRAINS"], ["evo_filter", "FILTER"], ["evo_sat", "SATURATE"], ["evo_stretch", "STRETCH"], ["evo_ring", "RING"], ["evo_revtail", "REV TAIL"], ["evo_trans", "PUNCH"]]
    .forEach(([id, l]) => tog(u.b, id, l));
  knob(u.b, "chaos", "CHAOS", { mini: true, color: "pink", fmt: fmt.pct100, def: 40 });
}
function fader(parent, id) {
  const st = slider(id);
  const f = el("div", "fader", `<div class="tr"></div><div class="fl"></div><div class="cap"></div>`), v = el("span", "fv");
  const fl = f.querySelector(".fl"), cap = f.querySelector(".cap");
  const r = () => { const n = st.getNormalisedValue(); fl.style.height = `${n * 100}%`; cap.style.bottom = `${n * 100}%`; v.textContent = fmt.db(st.getScaledValue()); };
  st.valueChangedEvent.addListener(r); st.propertiesChangedEvent.addListener(r);
  let d = false; const set = (e) => { const b = f.getBoundingClientRect(); st.setNormalisedValue(Math.min(1, Math.max(0, 1 - (e.clientY - b.top) / b.height))); };
  f.addEventListener("pointerdown", (e) => { f.setPointerCapture(e.pointerId); d = true; st.sliderDragStarted(); set(e); });
  f.addEventListener("pointermove", (e) => d && set(e));
  const end = () => { if (d) { d = false; st.sliderDragEnded(); } };
  f.addEventListener("pointerup", end); f.addEventListener("pointercancel", end);
  f.addEventListener("dblclick", () => setScaled(id, 0));
  parent.append(f, v); r();
}
function buildMix() {
  const b = $("#mixBody");
  const s = el("div", "strips"); b.appendChild(s);
  for (let i = 0; i < NL; i++) {
    const st = el("div", "strip"); st.appendChild(el("span", "sn", `L${i + 1}`)); fader(st, lid(i, "gain"));
    knob(st, lid(i, "pan"), "PAN", { mini: true, color: "blue", fmt: fmt.pan, def: 0 });
    st.addEventListener("pointerdown", () => selectLayer(i)); s.appendChild(st);
  }
  let u = unit(b, "MELODY");
  tog(u.h, "auto_tune", "AUTO-TUNE"); tog(u.h, "tonalize", "TONALIZE");
  knob(u.b, "mel_bars", "BARS", { mini: true, combo: true, color: "lime" });
  knob(u.b, "mel_octave", "OCTAVE", { mini: true, color: "lime", fmt: (v) => `${v > 0 ? "+" : ""}${Math.round(v)}`, def: 0 });
  knob(u.b, "glide", "GLIDE", { mini: true, color: "lime", fmt: fmt.ms, def: 0 }); knob(u.b, "vibrato", "VIBRATO", { mini: true, color: "lime", def: 0 });
  u = unit(b, "AMP");
  knob(u.b, "amp_attack", "ATTACK", { mini: true, color: "orange", fmt: fmt.ms, def: 1 }); knob(u.b, "amp_decay", "DECAY", { mini: true, color: "orange", fmt: fmt.ms, def: 400 });
  knob(u.b, "amp_sustain", "SUSTAIN", { mini: true, color: "orange", def: 1 }); knob(u.b, "amp_release", "RELEASE", { mini: true, color: "orange", fmt: fmt.ms, def: 250 });
  knob(u.b, "vel_sens", "VELO", { mini: true, color: "orange", def: 0.8 });
  u = unit(b, "OUTPUT", "limiter_on");
  knob(u.b, "master_gain", "MASTER", { mini: true, color: "pink", fmt: fmt.db, def: -3 }); knob(u.b, "limiter_ceiling", "CEILING", { mini: true, color: "pink", fmt: fmt.db, def: -0.3 });
  if (state.isFx) knob(u.b, "fx_dry", "DRY", { mini: true, color: "pink", def: 1 });
  const gr = el("div", "gr", "<div id='grBar'></div>"); gr.style.width = "120px"; u.b.appendChild(gr);
}

// ---------------------------------------------------------------- presets
async function refreshPresets() {
  let list; try { list = await call("listPresets"); } catch { return; }
  if (!Array.isArray(list)) return;
  state.presets = list;
  const cur = state.model?.preset || "";
  $("#presetName").textContent = (cur || "UNTITLED").toUpperCase();
  const box = $("#presetList"); box.innerHTML = ""; const grid = el("div", "plist"); box.appendChild(grid);
  let sec = null;
  list.forEach((p) => {
    const s = p.factory ? "FACTORY" : "YOUR PRESETS"; if (s !== sec) { grid.appendChild(el("div", "sec", s)); sec = s; }
    const b = el("button", p.name === cur ? "cur" : "", p.name.toUpperCase()); b.addEventListener("click", () => loadPreset(p)); grid.appendChild(b);
  });
}
async function loadPreset(p) { const r = await call("loadPreset", p.id); if (!r?.ok) toast(r?.error || "Could not load"); else { closeOverlays(); say(`HI! I'M ${p.name.toUpperCase()}!`); } refreshPresets(); }
function stepPreset(d) { const l = state.presets || []; if (!l.length) return; const k = l.findIndex((p) => p.name === state.model?.preset); loadPreset(l[((k < 0 ? (d > 0 ? -1 : 0) : k) + d + l.length) % l.length]); }

// ---------------------------------------------------------------- overlays
function closeOverlays() { $$(".overlay").forEach((o) => o.classList.remove("on")); $("#bFx").classList.remove("lit"); $("#bMix").classList.remove("lit"); }
function openOverlay(id, btn) {
  const o = $(id), was = o.classList.contains("on"); closeOverlays();
  if (!was) { o.classList.add("on"); btn?.classList.add("lit"); if (id === "#ovLayers") requestAnimationFrame(() => rows.forEach((_, k) => renderRow(k))); }
}

// ---------------------------------------------------------------- drag & drop (into the character's hands)
let dropLayer = 0, dragDepth = 0;
const AUDIO_RE = /\.(wav|wave|aif|aiff|flac|mp3|ogg)$/i;
function b64(buf) { const u = new Uint8Array(buf); let s = ""; for (let i = 0; i < u.length; i += 0x8000) s += String.fromCharCode.apply(null, u.subarray(i, i + 0x8000)); return btoa(s); }
function firstEmpty() { const i = state.model?.layers?.findIndex((l) => !l.loaded) ?? -1; return i < 0 ? state.sel : i; }
async function loadFileInto(layer, f) {
  if (!AUDIO_RE.test(f.name)) { toast(`Unsupported: ${f.name}`); return false; }
  if (f.size > 200 * 1024 * 1024) { toast("File too large"); return false; }
  const r = await call("loadSampleData", layer, f.name, b64(await f.arrayBuffer()));
  if (!r?.ok) { toast(r?.error || "Could not load"); return false; }
  return true;
}
function setupDrop() {
  window.addEventListener("dragenter", (e) => {
    e.preventDefault();
    if (++dragDepth === 1) { state.dragging = true; dropLayer = $("#ovLayers").classList.contains("on") ? state.sel : firstEmpty(); $("#charSlot").classList.add("dragging"); say("GIMME GIMME!", 0); }
  });
  window.addEventListener("dragover", (e) => { e.preventDefault(); if (e.dataTransfer) e.dataTransfer.dropEffect = "copy"; });
  window.addEventListener("dragleave", (e) => { e.preventDefault(); dragDepth = Math.max(0, dragDepth - 1); if (!dragDepth) { state.dragging = false; $("#charSlot").classList.remove("dragging"); say(bubbleSticky || "AWW...", 1200); } });
  window.addEventListener("drop", async (e) => {
    e.preventDefault(); dragDepth = 0; state.dragging = false; $("#charSlot").classList.remove("dragging");
    const files = [...(e.dataTransfer?.files || [])]; let layer = dropLayer, ok = false;
    for (const f of files.slice(0, NL)) { ok = (await loadFileInto(layer, f)) || ok; layer = (layer + 1) % NL; }
    if (ok) { anim.catchT = performance.now() / 1000 + 0.6; say("GOT IT! LET'S STRETCH!"); selectLayer(dropLayer); }
  });
}

// ---------------------------------------------------------------- computer keyboard
function setupKeys() {
  const MAP = "awsedftgyhujkolp;'"; const held = new Map(); let oct = 0;
  window.addEventListener("keydown", (e) => {
    if (e.target.tagName === "INPUT" || e.repeat || e.ctrlKey || e.metaKey) return;
    const k = e.key.toLowerCase();
    if (k === "z") { oct = Math.max(-3, oct - 1); toast(`OCTAVE ${oct}`); return; }
    if (k === "x") { oct = Math.min(3, oct + 1); toast(`OCTAVE ${oct}`); return; }
    if (k === " ") { e.preventDefault(); toggle("mel_on").setValue(!toggle("mel_on").getValue()); return; }
    const i = MAP.indexOf(k); if (i < 0) return;
    const n = 60 + i + oct * 12; held.set(k, n); call("noteOn", n, 0.85);
  });
  window.addEventListener("keyup", (e) => { const k = e.key.toLowerCase(); if (held.has(k)) { call("noteOff", held.get(k)); held.delete(k); } });
}

// ---------------------------------------------------------------- GENOFORM
async function startGeno(i) {
  if (!state.model?.layers?.[i]?.loaded) { say("GIVE ME A SOUND TO GROW FROM!"); return; }
  if (!(await call("genoform", i))) toast("Already growing...");
  else say("GROWING A SYNTH...", 0);
}
let wasGeno = false;
function renderGeno(m) {
  const p = m?.geno ?? -1; const box = $("#genoMeter");
  box.classList.toggle("hidden", p < 0);
  $("#sGeno").classList.toggle("busy", p >= 0);
  if (wasGeno && p < 0) say(`GENOFORM DONE!`, 2000);
  wasGeno = p >= 0;
  if (p < 0) return;
  const dots = $("#gmDots"); if (!dots.children.length) for (let i = 0; i < 10; i++) dots.appendChild(el("i"));
  [...dots.children].forEach((d, k) => d.classList.toggle("on", k < Math.round(p * 10)));
  $("#gmText").textContent = `GROWING ${Math.round(p * 100)}% · MATCH ${Math.round((m.genoScore || 0) * 100)}%`;
}

// ---------------------------------------------------------------- model
async function fetchPeaks(i) {
  const L = state.model?.layers?.[i];
  if (!L?.loaded) { delete state.peaks[i]; return; }
  if (state.peaks[i]?.rev === L.revision) return;
  state.peaks[i] = { rev: L.revision, mini: await call("getPeaks", i, 200) };
}
let prevBusy = false;
async function onModel(m) {
  state.model = m;
  await Promise.all(m.layers.map((_, i) => fetchPeaks(i)));
  $("#presetName").textContent = (m.preset || "UNTITLED").toUpperCase();
  const loaded = m.layers.some((l) => l.loaded), busy = m.layers.some((l) => l.busy);
  bubbleSticky = !loaded ? (state.isFx ? "PRESS FEED OR DROP A SOUND!" : "DROP A SOUND IN MY HANDS!") : "";
  if (bubbleSticky && !state.dragging) say(bubbleSticky, 0);
  else if (!state.dragging && $("#bubble").textContent.startsWith("DROP")) $("#bubble").classList.add("hide");
  if (prevBusy && !busy && loaded) say(QUIPS[Math.floor(Math.random() * QUIPS.length)]);
  prevBusy = busy;
  $("#sEvolve").classList.toggle("busy", busy);
  $("#sUndo").disabled = !m.canPrevLevel;
  $("#sFeed").classList.toggle("on", !!m.capturing);
  if (!m.capturing) $("#sFeed").textContent = "FEED";
  rows.forEach((_, i) => renderRow(i));
  renderStatus();
  const errs = m.layers.map((l) => l.error).filter(Boolean); if (errs.length) toast(errs[0]);
}

// ---------------------------------------------------------------- loop
let lastMeter = 0;
function tick(tms) {
  requestAnimationFrame(tick);
  animate(tms / 1000);
  if (tms - lastMeter > 50) {
    lastMeter = tms;
    call("getMeters").then((m) => {
      state.meters = m;
      const gr = $("#grBar"); if (gr) gr.style.width = `${Math.min(100, (1 - m.limiter) * 400)}%`;
      renderGeno(m);
      if (m.capturing) $("#sFeed").textContent = `STOP ${m.captureSec.toFixed(1)}S`;
    }).catch(() => {});
  }
}

// ---------------------------------------------------------------- character packs
async function loadCharacterPack() {
  try { const pack = await call("getCharacterPack"); if (pack && pack.base) char.loadPack(pack); else char.renderDefault(); } catch { char.renderDefault(); }
}

// ---------------------------------------------------------------- boot
window.addEventListener("error", (e) => { try { call("log", `${e.message} @${e.lineno}`); } catch {} });
window.addEventListener("unhandledrejection", (e) => { try { call("log", `rejection: ${e.reason}`); } catch {} });

async function boot() {
  installIcons();
  char = new CharacterView($("#char"));
  try { state.isFx = !!(await call("isEffect")); } catch { state.isFx = false; }
  if (state.isFx) $("#sFeed").classList.remove("hidden");
  buildKnobs(); buildLayers(); buildFx(); buildMix(); setupDrop(); setupKeys();

  $("#bFx").addEventListener("click", () => openOverlay("#ovFx", $("#bFx")));
  $("#bMix").addEventListener("click", () => openOverlay("#ovMix", $("#bMix")));
  const melBtn = $("#bMelody");
  onT("mel_on", () => { const on = !!toggle("mel_on").getValue(); melBtn.classList.toggle("on", on); });
  melBtn.addEventListener("click", () => {
    if (!state.model?.layers?.some((l) => l.loaded)) { say("DROP A SOUND FIRST!"); return; }
    const on = !toggle("mel_on").getValue();
    toggle("mel_on").setValue(on);
    if (on) say("LA LA LAAA!", 1500);
  });
  ["mel_key", "mel_scale"].forEach((id) => onC(id, renderStatus));
  $("#sTune").addEventListener("click", async () => {
    await call("newMelody"); say("NEW TUNE!", 1500);
    if (!toggle("mel_on").getValue() && state.model?.layers?.some((l) => l.loaded)) toggle("mel_on").setValue(true);
  });
  $("#sEvolve").addEventListener("click", doEvolve);
  $("#sGeno").addEventListener("click", () => startGeno(state.sel));
  $("#sUndo").addEventListener("click", () => call("stepLevel", -1));
  $("#sFeed").addEventListener("click", async () => {
    if (state.model?.capturing) { call("stopCapture"); say("NOM NOM!"); }
    else if (await call("startCapture", firstEmpty())) say("I'M LISTENING... PLAY SOMETHING!", 0);
    else toast("No audio input");
  });
  $$("[data-close]").forEach((b) => b.addEventListener("click", closeOverlays));
  $("#tLoad").addEventListener("click", () => call("browseSample", firstEmpty()));
  $("#tLayers").addEventListener("click", () => openOverlay("#ovLayers"));
  $("#tPresets").addEventListener("click", () => { refreshPresets(); openOverlay("#ovPresets"); });
  $("#tSave").addEventListener("click", () => { openOverlay("#ovSave"); const n = $("#saveName"); n.value = state.model?.preset || ""; setTimeout(() => n.focus(), 30); });
  $("#saveGo").addEventListener("click", async () => {
    const name = $("#saveName").value.trim(); if (!name) { toast("Enter a name"); return; }
    const r = await call("savePreset", name); if (r?.ok) { closeOverlays(); toast(`Saved ${name}`); refreshPresets(); } else toast(r?.error || "Save failed");
  });
  $("#saveName").addEventListener("keydown", (e) => { if (e.key === "Enter") $("#saveGo").click(); });
  $("#tDice").addEventListener("click", async () => {
    if (!state.model?.layers?.some((l) => l.loaded)) { say("NOTHING TO ROLL!"); return; }
    await call("rollDice"); await call("newMelody"); say("SURPRISE!");
  });
  $("#tChar").addEventListener("click", () => openOverlay("#ovChar"));
  $("#charPick").addEventListener("click", () => call("pickCharacterFolder"));
  $("#charReset").addEventListener("click", async () => { await call("resetCharacter"); char.renderDefault(); closeOverlays(); });
  $("#pPrev").addEventListener("click", () => stepPreset(-1));
  $("#pNext").addEventListener("click", () => stepPreset(1));
  $("#charSlot").addEventListener("click", () => {
    if (!state.model?.layers?.some((l) => l.loaded)) call("browseSample", firstEmpty());
    else { call("noteOn", 60, 0.9); setTimeout(() => call("noteOff", 60), 350); anim.noteKick = 1; anim.mouthUntil = performance.now() / 1000 + 0.3; }
  });

  window.__JUCE__.backend.addEventListener("modelChanged", onModel);
  window.__JUCE__.backend.addEventListener("notify", (msg) => toast(String(msg)));
  window.__JUCE__.backend.addEventListener("characterChanged", () => { loadCharacterPack(); closeOverlays(); say("NEW LOOK!"); });
  window.addEventListener("resize", fit);
  fit();
  selectLayer(0);
  call("getModel").then(onModel);
  refreshPresets();
  loadCharacterPack();
  requestAnimationFrame(tick);
}
boot();
