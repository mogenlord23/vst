// MUTAFORM UI — vanilla JS, no bundler. Talks to C++ through JUCE relays + native functions.
import * as Juce from "./juce/index.js";

const NL = 5;                       // layers
const $ = (s, r = document) => r.querySelector(s);
const el = (tag, cls, html) => { const e = document.createElement(tag); if (cls) e.className = cls; if (html != null) e.innerHTML = html; return e; };
const fnCache = {};
const nf = (name) => (fnCache[name] ??= Juce.getNativeFunction(name));
const call = (name, ...a) => nf(name)(...a);
const lid = (i, s) => `l${i + 1}_${s}`;

const state = { model: null, sel: 0, tab: "evolve", peaks: {}, meters: null, octave: 0, scale: 1 };

// ---------------------------------------------------------------- param helpers
const sliders = {}, toggles = {}, combos = {};
const slider = (id) => (sliders[id] ??= Juce.getSliderState(id));
const toggle = (id) => (toggles[id] ??= Juce.getToggleState(id));
const combo = (id) => (combos[id] ??= Juce.getComboBoxState(id));

function scaledToNorm(st, v) {
  const p = st.properties;
  if (p.end === p.start) return 0;
  return Math.pow(Math.min(1, Math.max(0, (v - p.start) / (p.end - p.start))), p.skew);
}

const fmt = {
  pct: (v) => `${Math.round(v * 100)}%`,
  pct100: (v) => `${Math.round(v)}%`,
  db: (v) => (v <= -47.9 ? "-inf" : `${v > 0 ? "+" : ""}${v.toFixed(1)} dB`),
  ms: (v) => (v >= 1000 ? `${(v / 1000).toFixed(2)} s` : `${v < 10 ? v.toFixed(1) : Math.round(v)} ms`),
  hz: (v) => (v >= 1000 ? `${(v / 1000).toFixed(v >= 10000 ? 1 : 2)} kHz` : `${v < 10 ? v.toFixed(2) : Math.round(v)} Hz`),
  st: (v) => `${v > 0 ? "+" : ""}${Math.round(v)} st`,
  ct: (v) => `${v > 0 ? "+" : ""}${Math.round(v)} ct`,
  pan: (v) => (Math.abs(v) < 0.01 ? "C" : v < 0 ? `L${Math.round(-v * 100)}` : `R${Math.round(v * 100)}`),
  pos: (v) => `${(v * 100).toFixed(1)}%`,
  num: (v) => v.toFixed(2),
};

// ---------------------------------------------------------------- knob
const ARC = 270, R = 17;
function arcPath(cx, cy, r, a0, a1) {
  const p = (a) => [cx + r * Math.cos((a - 90) * Math.PI / 180), cy + r * Math.sin((a - 90) * Math.PI / 180)];
  const [x0, y0] = p(a0), [x1, y1] = p(a1);
  return `M${x0.toFixed(2)} ${y0.toFixed(2)} A${r} ${r} 0 ${a1 - a0 > 180 ? 1 : 0} 1 ${x1.toFixed(2)} ${y1.toFixed(2)}`;
}

function knob(parent, id, label, f = fmt.num, opts = {}) {
  const st = slider(id);
  const big = !!opts.big;
  const k = el("div", "knob" + (big ? " big" : ""));
  k.title = opts.title || label;
  const vb = 44, c = vb / 2, r = big ? 19 : R;
  k.innerHTML = `<svg viewBox="0 0 ${vb} ${vb}">
      ${big ? `<defs><linearGradient id="cg" x1="0" y1="1" x2="1" y2="0"><stop offset="0" stop-color="#38E0C0"/><stop offset="1" stop-color="#FF5A7A"/></linearGradient></defs>` : ""}
      <path class="trk" d="${arcPath(c, c, r, -ARC / 2, ARC / 2)}" stroke="#283041" stroke-width="${big ? 3 : 3.2}" fill="none" stroke-linecap="round"/>
      <path class="val" stroke="${big ? "url(#cg)" : (opts.color || "#38E0C0")}" stroke-width="${big ? 3.4 : 3.2}" fill="none" stroke-linecap="round"/>
      <circle cx="${c}" cy="${c}" r="${big ? 13 : 11}" fill="#1C2130" stroke="#283041"/>
      <line class="ptr" x1="${c}" y1="${c - (big ? 6 : 5)}" x2="${c}" y2="${c - (big ? 12 : 10)}" stroke="#E7EAF0" stroke-width="2" stroke-linecap="round"/>
      ${big ? `<text class="bigtxt" x="${c}" y="${c + 3.5}" text-anchor="middle" font-size="9" font-weight="700" fill="#E7EAF0" font-family="Cascadia Mono,Consolas,monospace"></text>` : ""}
    </svg><div class="k-label">${label}</div><div class="k-val"></div>`;
  if (big) k.querySelector(".k-val").remove();
  const valPath = k.querySelector(".val"), ptr = k.querySelector(".ptr"), txt = k.querySelector(".k-val"), bigTxt = k.querySelector(".bigtxt");
  const bipolar = opts.bipolar;
  const render = () => {
    const n = st.getNormalisedValue();
    const a = -ARC / 2 + n * ARC;
    const a0 = bipolar ? 0 : -ARC / 2;
    valPath.setAttribute("d", Math.abs(a - a0) < 0.5 ? "" : arcPath(c, c, r, Math.min(a0, a), Math.max(a0, a)));
    ptr.setAttribute("transform", `rotate(${a} ${c} ${c})`);
    const s = f(st.getScaledValue());
    if (txt) txt.textContent = s;
    if (bigTxt) bigTxt.textContent = s;
    opts.onChange?.(st.getScaledValue());
  };
  st.valueChangedEvent.addListener(render);
  st.propertiesChangedEvent.addListener(render);
  render();

  let startY = 0, startN = 0, dragging = false;
  k.addEventListener("pointerdown", (e) => {
    if (e.button !== 0) return;
    k.setPointerCapture(e.pointerId);
    dragging = true; startY = e.clientY; startN = st.getNormalisedValue();
    st.sliderDragStarted();
  });
  k.addEventListener("pointermove", (e) => {
    if (!dragging) return;
    const sens = (e.shiftKey ? 0.0012 : 0.006) / state.scale;
    st.setNormalisedValue(Math.min(1, Math.max(0, startN + (startY - e.clientY) * sens)));
  });
  const end = () => { if (dragging) { dragging = false; st.sliderDragEnded(); } };
  k.addEventListener("pointerup", end);
  k.addEventListener("pointercancel", end);
  k.addEventListener("dblclick", () => {
    if (opts.def == null) return;
    st.sliderDragStarted(); st.setNormalisedValue(scaledToNorm(st, opts.def)); st.sliderDragEnded();
  });
  k.addEventListener("wheel", (e) => {
    e.preventDefault();
    st.sliderDragStarted();
    st.setNormalisedValue(Math.min(1, Math.max(0, st.getNormalisedValue() - Math.sign(e.deltaY) * (e.shiftKey ? 0.005 : 0.02))));
    st.sliderDragEnded();
  }, { passive: false });
  parent.appendChild(k);
  return k;
}

function tg(parent, id, label, cls = "", onChange) {
  const st = toggle(id);
  const b = el("button", "tg " + cls, label);
  const render = () => { b.classList.toggle("on", !!st.getValue()); onChange?.(!!st.getValue()); };
  st.valueChangedEvent.addListener(render);
  st.propertiesChangedEvent.addListener(render);
  b.addEventListener("click", () => st.setValue(!st.getValue()));
  render();
  parent.appendChild(b);
  return b;
}

function seg(parent, id, labels, onChange) {
  const st = combo(id);
  const s = el("div", "seg");
  const btns = labels.map((l, i) => {
    const b = el("button", "", l);
    b.addEventListener("click", () => st.setChoiceIndex(i));
    s.appendChild(b);
    return b;
  });
  const render = () => { const i = st.getChoiceIndex(); btns.forEach((b, j) => b.classList.toggle("on", i === j)); onChange?.(i); };
  st.valueChangedEvent.addListener(render);
  st.propertiesChangedEvent.addListener(render);
  render();
  parent.appendChild(s);
  return s;
}

function group(parent, title, headExtra) {
  const g = el("div", "group");
  const h = el("div", "group-head");
  h.appendChild(el("span", "sec-title", title));
  g.appendChild(h);
  headExtra?.(h, g);
  parent.appendChild(g);
  return g;
}

// ---------------------------------------------------------------- toast
let toastTimer = 0;
function toast(msg) {
  const t = $("#toast");
  t.textContent = msg;
  t.classList.add("show");
  clearTimeout(toastTimer);
  toastTimer = setTimeout(() => t.classList.remove("show"), 2600);
}

// ---------------------------------------------------------------- scaling
function fit() {
  const s = Math.min(window.innerWidth / 900, window.innerHeight / 600);
  state.scale = s > 0 ? s : 1;
  $("#app").style.transform = `scale(${state.scale})`;
  resizeCanvases();
}

// ---------------------------------------------------------------- layer rail
const slots = [];
function buildRail() {
  const rail = $("#rail");
  for (let i = 0; i < NL; i++) {
    const s = el("div", "slot empty");
    s.innerHTML = `<span class="num">${i + 1}</span><span class="nm">empty</span><span class="ms"></span><canvas></canvas><span class="busy-dot"></span>`;
    const ms = s.querySelector(".ms");
    for (const [suffix, letter] of [["mute", "m"], ["solo", "s"]]) {
      const st = toggle(lid(i, suffix));
      const b = el("button", `mini-tg ${letter}`, letter.toUpperCase());
      b.title = suffix === "mute" ? "Mute" : "Solo";
      const r = () => b.classList.toggle("on", !!st.getValue());
      st.valueChangedEvent.addListener(r); st.propertiesChangedEvent.addListener(r); r();
      b.addEventListener("click", (e) => { e.stopPropagation(); st.setValue(!st.getValue()); });
      ms.appendChild(b);
    }
    s.addEventListener("click", () => selectLayer(i));
    s.addEventListener("dragenter", (e) => { e.preventDefault(); s.classList.add("drag"); dropLayer = i; $("#dropTarget").textContent = `L${i + 1}`; });
    s.addEventListener("dragleave", () => s.classList.remove("drag"));
    rail.appendChild(s);
    slots.push({ root: s, canvas: s.querySelector("canvas"), name: s.querySelector(".nm") });
  }
}

function selectLayer(i) {
  state.sel = i;
  call("selectLayer", i);
  slots.forEach((s, j) => s.root.classList.toggle("selected", i === j));
  document.querySelectorAll(".strip").forEach((s, j) => s.classList.toggle("sel", i === j));
  buildLayerPage();
  renderModel();
}

// ---------------------------------------------------------------- waveform
const wave = { canvas: null, ctx: null, w: 0, h: 0, drag: null, hover: null };

function resizeCanvases() {
  const dpr = (window.devicePixelRatio || 1) * state.scale;
  const fitCanvas = (c) => {
    const r = c.getBoundingClientRect();
    const w = Math.max(1, Math.round(r.width / state.scale)), h = Math.max(1, Math.round(r.height / state.scale));
    c.width = Math.round(w * dpr); c.height = Math.round(h * dpr);
    c._w = w; c._h = h; c._dpr = dpr;
  };
  if (wave.canvas) fitCanvas(wave.canvas);
  slots.forEach((s) => fitCanvas(s.canvas));
  drawAll();
}

async function fetchPeaks(i) {
  const L = state.model?.layers?.[i];
  if (!L || !L.loaded) { delete state.peaks[i]; return; }
  const have = state.peaks[i];
  if (have && have.rev === L.revision) return;
  const [main, mini] = await Promise.all([call("getPeaks", i, 1024), call("getPeaks", i, 160)]);
  state.peaks[i] = { rev: L.revision, main, mini, born: performance.now() };
}

function drawPeaks(ctx, peaks, x0, w, h, color, alpha = 1) {
  if (!peaks || !peaks.length) return;
  const n = peaks.length / 2, mid = h / 2;
  ctx.fillStyle = color; ctx.globalAlpha = alpha;
  ctx.beginPath();
  for (let px = 0; px < w; px++) {
    const b = Math.min(n - 1, Math.floor(px / w * n));
    const mn = peaks[b * 2], mx = peaks[b * 2 + 1];
    const y0 = mid - mx * mid * 0.92, y1 = mid - mn * mid * 0.92;
    ctx.rect(x0 + px, y0, 1, Math.max(1, y1 - y0));
  }
  ctx.fill();
  ctx.globalAlpha = 1;
}

function markers(i) {
  const g = (s) => slider(lid(i, s)).getScaledValue();
  return {
    start: g("start"), end: g("end"), ls: g("loopStart"), le: g("loopEnd"),
    loop: !!toggle(lid(i, "loop")).getValue(), rev: !!toggle(lid(i, "reverse")).getValue(),
    mp: state.model?.layers?.[i]?.multipoints || [], mpMode: combo(lid(i, "mpMode")).getChoiceIndex(),
    xfade: g("xfade"), seconds: state.model?.layers?.[i]?.seconds || 0,
  };
}

function drawMain() {
  const c = wave.canvas; if (!c || !c._w) return;
  const ctx = c.getContext("2d");
  const w = c._w, h = c._h;
  ctx.setTransform(c._dpr, 0, 0, c._dpr, 0, 0);
  ctx.clearRect(0, 0, w, h);
  const i = state.sel, L = state.model?.layers?.[i];
  // grid
  ctx.strokeStyle = "#1d2330"; ctx.lineWidth = 1;
  for (let k = 1; k < 8; k++) { const x = Math.round(w * k / 8) + 0.5; ctx.beginPath(); ctx.moveTo(x, 0); ctx.lineTo(x, h); ctx.stroke(); }
  ctx.beginPath(); ctx.moveTo(0, h / 2 + 0.5); ctx.lineTo(w, h / 2 + 0.5); ctx.stroke();
  if (!L || !L.loaded) return;
  const P = state.peaks[i]; const m = markers(i);
  const fade = P ? Math.min(1, (performance.now() - P.born) / 220) : 1;
  const xs = m.start * w, xe = m.end * w;
  // loop region
  if (m.loop) {
    ctx.fillStyle = "rgba(255,184,77,0.10)";
    ctx.fillRect(m.ls * w, 0, (m.le - m.ls) * w, h);
    if (m.seconds > 0 && m.xfade > 0) {
      const xw = Math.min((m.xfade / 1000) / m.seconds, (m.le - m.ls) / 2) * w;
      const g = ctx.createLinearGradient(m.le * w - xw, 0, m.le * w, 0);
      g.addColorStop(0, "rgba(255,184,77,0)"); g.addColorStop(1, "rgba(255,184,77,0.22)");
      ctx.fillStyle = g; ctx.fillRect(m.le * w - xw, 0, xw, h);
    }
  }
  if (P) {
    drawPeaks(ctx, P.main, 0, w, h, "#38E0C0", 0.22 * fade);
    ctx.save(); ctx.beginPath(); ctx.rect(xs, 0, Math.max(0, xe - xs), h); ctx.clip();
    drawPeaks(ctx, P.main, 0, w, h, "#38E0C0", fade);
    ctx.restore();
  }
  // markers
  const vline = (x, col, label, top = true) => {
    ctx.strokeStyle = col; ctx.lineWidth = 1.5;
    ctx.beginPath(); ctx.moveTo(Math.round(x) + 0.5, 0); ctx.lineTo(Math.round(x) + 0.5, h); ctx.stroke();
    ctx.fillStyle = col;
    const y = top ? 0 : h - 14, bw = 12;
    const bx = Math.min(w - bw, Math.max(0, Math.round(x) - bw / 2));
    ctx.fillRect(bx, y, bw, 14);
    ctx.fillStyle = "#0D0F13"; ctx.font = "700 9px Cascadia Mono, Consolas, monospace"; ctx.textAlign = "center";
    ctx.fillText(label, bx + bw / 2, y + 10);
  };
  vline(xs, "#38E0C0", "S"); vline(xe, "#38E0C0", "E");
  if (m.loop) { vline(m.ls * w, "#FFB84D", "L", false); vline(m.le * w, "#FFB84D", "L", false); }
  if (m.mpMode > 0) m.mp.forEach((p, k) => vline(p * w, "#8F7CFF", String(k + 1), false));
  if (m.rev) {
    ctx.fillStyle = "#FFB84D"; ctx.font = "700 10px Segoe UI, system-ui"; ctx.textAlign = "left";
    ctx.fillText("◀ REVERSE", 26, 12);
  }
  // playhead
  const ph = state.meters?.playheads?.[i];
  if (ph != null && ph >= 0) {
    ctx.strokeStyle = "#E7EAF0"; ctx.globalAlpha = 0.85; ctx.lineWidth = 1;
    ctx.beginPath(); ctx.moveTo(ph * w + 0.5, 0); ctx.lineTo(ph * w + 0.5, h); ctx.stroke(); ctx.globalAlpha = 1;
  }
}

function drawMini(i) {
  const s = slots[i], c = s.canvas; if (!c._w) return;
  const ctx = c.getContext("2d");
  ctx.setTransform(c._dpr, 0, 0, c._dpr, 0, 0);
  ctx.clearRect(0, 0, c._w, c._h);
  const L = state.model?.layers?.[i];
  if (!L || !L.loaded) {
    ctx.strokeStyle = "#283041"; ctx.setLineDash([3, 3]);
    ctx.beginPath(); ctx.moveTo(0, c._h / 2 + 0.5); ctx.lineTo(c._w, c._h / 2 + 0.5); ctx.stroke(); ctx.setLineDash([]);
    return;
  }
  const muted = !!toggle(lid(i, "mute")).getValue();
  drawPeaks(ctx, state.peaks[i]?.mini, 0, c._w, c._h, muted ? "#4a5263" : (i === state.sel ? "#38E0C0" : "#7fa9a0"));
  const ph = state.meters?.playheads?.[i];
  if (ph != null && ph >= 0) { ctx.fillStyle = "#E7EAF0"; ctx.fillRect(Math.round(ph * c._w), 0, 1, c._h); }
}

function drawAll() { drawMain(); for (let i = 0; i < NL; i++) drawMini(i); }

function setupWaveInteraction() {
  const c = wave.canvas;
  const pos = (e) => { const r = c.getBoundingClientRect(); return Math.min(1, Math.max(0, (e.clientX - r.left) / r.width)); };
  const hit = (x) => {
    const i = state.sel, m = markers(i), tol = 7 / (c._w || 500);
    const cands = [];
    if (m.mpMode > 0) m.mp.forEach((p, k) => cands.push({ kind: "mp", k, x: p }));
    if (m.loop) { cands.push({ kind: "loopEnd", x: m.le }); cands.push({ kind: "loopStart", x: m.ls }); }
    cands.push({ kind: "end", x: m.end }); cands.push({ kind: "start", x: m.start });
    let best = null, bd = tol;
    for (const cd of cands) { const d = Math.abs(cd.x - x); if (d <= bd) { bd = d; best = cd; } }
    return best;
  };
  c.addEventListener("pointermove", (e) => {
    const x = pos(e);
    if (wave.drag) {
      const d = wave.drag;
      if (d.kind === "mp") {
        const pts = [...(state.model.layers[state.sel].multipoints || [])];
        pts[d.k] = x; state.model.layers[state.sel].multipoints = pts; d.pts = pts;
      } else {
        slider(lid(state.sel, d.kind)).setNormalisedValue(x);
      }
      drawMain();
      return;
    }
    c.style.cursor = hit(x) ? "ew-resize" : "crosshair";
  });
  c.addEventListener("pointerdown", (e) => {
    if (!state.model?.layers?.[state.sel]?.loaded) return;
    const x = pos(e), h = hit(x);
    if (e.button === 2) {
      if (h && h.kind === "mp") {
        const pts = [...state.model.layers[state.sel].multipoints]; pts.splice(h.k, 1);
        call("setMultipoints", state.sel, pts);
      }
      return;
    }
    if (!h) return;
    c.setPointerCapture(e.pointerId);
    wave.drag = h;
    if (h.kind !== "mp") slider(lid(state.sel, h.kind)).sliderDragStarted();
  });
  const end = () => {
    const d = wave.drag; if (!d) return;
    wave.drag = null;
    if (d.kind === "mp") { if (d.pts) call("setMultipoints", state.sel, d.pts); }
    else slider(lid(state.sel, d.kind)).sliderDragEnded();
  };
  c.addEventListener("pointerup", end);
  c.addEventListener("pointercancel", end);
  c.addEventListener("contextmenu", (e) => e.preventDefault());
  c.addEventListener("dblclick", (e) => {
    const L = state.model?.layers?.[state.sel];
    if (!L?.loaded) return;
    const pts = [...(L.multipoints || [])];
    if (pts.length >= 8) { toast("Up to 8 start points"); return; }
    pts.push(pos(e));
    call("setMultipoints", state.sel, pts);
    const mp = combo(lid(state.sel, "mpMode"));
    if (mp.getChoiceIndex() === 0) mp.setChoiceIndex(1);
  });
}

// ---------------------------------------------------------------- keyboard
const KEY_MAP = "awsedftgyhujkolp;'";
const heldKeys = new Set();
function buildKeys() {
  const k = $("#keys");
  const first = 48, count = 25; // C2..C4
  const whites = [];
  for (let n = first; n < first + count; n++) if (![1, 3, 6, 8, 10].includes(n % 12)) whites.push(n);
  const ww = 100 / whites.length;
  const keyEls = {};
  whites.forEach((n) => { const d = el("div", "key" + (n === 60 ? " root" : "")); d.dataset.note = n; k.appendChild(d); keyEls[n] = d; });
  for (let n = first; n < first + count; n++) {
    if (![1, 3, 6, 8, 10].includes(n % 12)) continue;
    const wi = whites.indexOf(n - 1);
    const d = el("div", "key black");
    d.style.left = `calc(6px + (100% - 12px) * ${(wi + 1) * ww / 100} - 6px)`;
    d.style.width = `calc((100% - 12px) * ${ww * 0.62 / 100})`;
    d.style.marginLeft = `calc((100% - 12px) * ${-ww * 0.31 / 100} + 6px)`;
    d.dataset.note = n; k.appendChild(d); keyEls[n] = d;
  }
  let down = null;
  const on = (n) => { call("noteOn", n, 0.85); keyEls[n]?.classList.add("down"); };
  const off = (n) => { call("noteOff", n); keyEls[n]?.classList.remove("down"); };
  k.addEventListener("pointerdown", (e) => { const n = +e.target.dataset.note; if (!n) return; k.setPointerCapture(e.pointerId); down = n; on(n); });
  k.addEventListener("pointermove", (e) => {
    if (down == null) return;
    const t = document.elementFromPoint(e.clientX, e.clientY); const n = t && +t.dataset.note;
    if (n && n !== down) { off(down); down = n; on(n); }
  });
  const up = () => { if (down != null) { off(down); down = null; } };
  k.addEventListener("pointerup", up); k.addEventListener("pointercancel", up);

  window.addEventListener("keydown", (e) => {
    if (e.target.tagName === "INPUT" || e.target.tagName === "SELECT" || e.repeat || e.ctrlKey || e.metaKey) return;
    const key = e.key.toLowerCase();
    if (key === "z") { state.octave = Math.max(-3, state.octave - 1); toast(`Keyboard octave ${state.octave >= 0 ? "+" : ""}${state.octave}`); return; }
    if (key === "x") { state.octave = Math.min(3, state.octave + 1); toast(`Keyboard octave ${state.octave >= 0 ? "+" : ""}${state.octave}`); return; }
    if (key === " ") { e.preventDefault(); doEvolve(); return; }
    const idx = KEY_MAP.indexOf(key);
    if (idx < 0) return;
    const n = 60 + idx + state.octave * 12;
    heldKeys.add(key + ":" + n); on(n);
  });
  window.addEventListener("keyup", (e) => {
    const key = e.key.toLowerCase();
    for (const h of [...heldKeys]) if (h.startsWith(key + ":")) { const n = +h.split(":")[1]; off(n); heldKeys.delete(h); }
  });
}

// ---------------------------------------------------------------- drag & drop
let dropLayer = 0, dragDepth = 0;
const AUDIO_RE = /\.(wav|wave|aif|aiff|flac|mp3|ogg)$/i;

function bufferToBase64(buf) {
  const bytes = new Uint8Array(buf);
  let bin = "";
  for (let i = 0; i < bytes.length; i += 0x8000) bin += String.fromCharCode.apply(null, bytes.subarray(i, i + 0x8000));
  return btoa(bin);
}

async function loadFileInto(layer, file) {
  if (!AUDIO_RE.test(file.name)) { toast(`Unsupported file: ${file.name}`); return; }
  if (file.size > 200 * 1024 * 1024) { toast("File too large"); return; }
  const b64 = bufferToBase64(await file.arrayBuffer());
  const r = await call("loadSampleData", layer, file.name, b64);
  if (!r?.ok) toast(r?.error || "Could not load file");
}

function setupDrop() {
  const overlay = $("#dropOverlay");
  window.addEventListener("dragenter", (e) => {
    e.preventDefault(); dragDepth++;
    if (dragDepth === 1) { dropLayer = state.sel; $("#dropTarget").textContent = `L${dropLayer + 1}`; overlay.classList.add("on"); $("#dropzone").classList.add("drag"); }
  });
  window.addEventListener("dragover", (e) => { e.preventDefault(); if (e.dataTransfer) e.dataTransfer.dropEffect = "copy"; });
  window.addEventListener("dragleave", (e) => {
    e.preventDefault(); dragDepth = Math.max(0, dragDepth - 1);
    if (dragDepth === 0) { overlay.classList.remove("on"); $("#dropzone").classList.remove("drag"); slots.forEach((s) => s.root.classList.remove("drag")); }
  });
  window.addEventListener("drop", async (e) => {
    e.preventDefault(); dragDepth = 0;
    overlay.classList.remove("on"); $("#dropzone").classList.remove("drag"); slots.forEach((s) => s.root.classList.remove("drag"));
    const files = [...(e.dataTransfer?.files || [])];
    let layer = dropLayer;
    for (const f of files.slice(0, NL)) { await loadFileInto(layer, f); layer = (layer + 1) % NL; }
    selectLayer(dropLayer);
  });
}

// ---------------------------------------------------------------- evolve column
function chaosText(c) {
  const lo = Math.min(5, 2 + Math.floor(c * 3)), hi = Math.min(5, 2 + Math.floor(c * 3 + 0.999));
  return { lo, hi, str: lo === hi ? `${lo} processes` : `${lo}–${hi} processes` };
}

function buildEvolveColumn() {
  knob($("#chaosRing"), "chaos", "CHAOS", fmt.pct100, {
    big: true, def: 40,
    onChange: (v) => {
      const t = chaosText(v / 100);
      $("#chaosDesc").textContent = `${t.str} per evolve`;
      const big = $("#chaosBig");
      if (big) big.innerHTML = `CHAOS <b>${Math.round(v)}%</b><br>chain length: <b>${t.str}</b><br>strength: ${(0.08 + v / 100 * 0.55).toFixed(2)} – ${Math.min(1, 0.08 + v / 100).toFixed(2)}`;
    },
  });
  $("#evolveBtn").addEventListener("click", doEvolve);
  $("#undoBtn").addEventListener("click", () => call("undo", state.sel));
  $("#redoBtn").addEventListener("click", () => call("redo", state.sel));
}

async function doEvolve() {
  if ($("#evolveAll").checked) {
    const n = await call("evolveAll");
    if (!n) toast("Load a sample first");
  } else {
    const L = state.model?.layers?.[state.sel];
    if (!L?.loaded) { toast("Load a sample first"); return; }
    if (L.busy) return;
    await call("evolve", state.sel, "");
  }
}

// ---------------------------------------------------------------- tabs
function setupTabs() {
  document.querySelectorAll("#tabs button").forEach((b) => b.addEventListener("click", () => {
    state.tab = b.dataset.tab;
    document.querySelectorAll("#tabs button").forEach((x) => x.classList.toggle("active", x === b));
    document.querySelectorAll(".tab-page").forEach((p) => p.classList.toggle("active", p.dataset.page === state.tab));
  }));
}

// EVOLVE tab -----------------------------------------------------
function renderHistory() {
  const L = state.model?.layers?.[state.sel];
  const h = $("#history");
  h.innerHTML = "";
  if (!L?.loaded) {
    h.appendChild(el("div", "empty-note", "Load a sample into this layer, then press EVOLVE. Every variation lands here — click one to go back."));
    $("#histCount").textContent = ""; $("#histDetail").textContent = "";
    return;
  }
  $("#histCount").textContent = `${L.index + 1} / ${L.history.length}`;
  L.history.forEach((e, k) => {
    const c = el("div", "chip" + (k === L.index ? " cur" : ""));
    c.innerHTML = `<span class="c-n">${k === 0 ? "◆" : k}</span><span class="c-s">${k === 0 ? "original" : e.seed.slice(-8) + " · " + e.chaos + "%"}</span><span class="c-l"></span>`;
    c.querySelector(".c-l").textContent = k === 0 ? (L.name || "") : e.label;
    c.title = k === 0 ? "Original sample" : `seed ${e.seed}\nchaos ${e.chaos}%\n${e.label}`;
    c.addEventListener("click", () => call("jumpHistory", state.sel, k));
    h.appendChild(c);
  });
  const cur = L.history[L.index];
  $("#histDetail").textContent = L.index === 0 ? "Original sample" : `seed ${cur.seed} · chaos ${cur.chaos}% · ${cur.label}`;
  const curChip = h.children[L.index];
  curChip?.scrollIntoView({ block: "nearest", inline: "nearest" });
}

function setupEvolvePage() {
  $("#seedRender").addEventListener("click", async () => {
    const v = $("#seedInput").value.trim();
    if (!/^[0-9a-fA-F]{1,16}$/.test(v) || /^0+$/.test(v)) { toast("Enter a hex seed (1–16 digits)"); return; }
    if (!(await call("evolve", state.sel, v))) toast("Layer is empty or busy");
  });
}

// CHAOS tab ---------------------------------------------------------
const PROCS = [
  ["evo_pitch", "Pitch shift", "tape-style transposition"],
  ["evo_grain", "Grain shuffle", "re-orders tiny slices"],
  ["evo_filter", "Envelope filter", "sweeping resonant SVF"],
  ["evo_sat", "Saturation", "tanh · fold · crush"],
  ["evo_stretch", "Time stretch", "overlap-add lengthen/shorten"],
  ["evo_ring", "Ring mod", "metallic sidebands"],
  ["evo_revtail", "Reverse tail", "reverb swell into the hit"],
  ["evo_trans", "Transient shaper", "punch or soften attack"],
];
function buildChaosPage() {
  const g = $("#procGrid");
  PROCS.forEach(([id, name, desc]) => {
    const st = toggle(id);
    const p = el("div", "proc");
    p.innerHTML = `<span class="p-name">${name}</span><span class="p-desc">${desc}</span><span class="p-state"></span>`;
    const r = () => { const on = !!st.getValue(); p.classList.toggle("on", on); p.querySelector(".p-state").textContent = on ? "IN POOL" : "OFF"; };
    st.valueChangedEvent.addListener(r); st.propertiesChangedEvent.addListener(r); r();
    p.addEventListener("click", () => st.setValue(!st.getValue()));
    g.appendChild(p);
  });
}

// LAYERS tab --------------------------------------------------------
function buildLayerPage() {
  const page = $("#layerPage");
  page.innerHTML = "";
  const i = state.sel;
  const region = group(page, `L${i + 1} REGION`);
  let r = el("div", "row"); region.appendChild(r);
  knob(r, lid(i, "start"), "Start", fmt.pos, { def: 0, onChange: drawAll });
  knob(r, lid(i, "end"), "End", fmt.pos, { def: 1, onChange: drawAll });

  const loop = group(page, "LOOP", (h) => tg(h, lid(i, "loop"), "LOOP", "amber", drawAll));
  r = el("div", "row"); loop.appendChild(r);
  knob(r, lid(i, "loopStart"), "Loop in", fmt.pos, { def: 0.2, color: "#FFB84D", onChange: drawAll });
  knob(r, lid(i, "loopEnd"), "Loop out", fmt.pos, { def: 0.8, color: "#FFB84D", onChange: drawAll });
  knob(r, lid(i, "xfade"), "X-fade", fmt.ms, { def: 15, color: "#FFB84D", onChange: drawAll });

  const play = group(page, "PLAYBACK", (h) => tg(h, lid(i, "reverse"), "REVERSE", "amber", drawAll));
  const mpRow = el("div", "row"); mpRow.style.alignItems = "center"; mpRow.style.gap = "6px"; play.appendChild(mpRow);
  mpRow.appendChild(el("span", "sec-title", "MULTI"));
  seg(mpRow, lid(i, "mpMode"), ["OFF", "KEY", "VEL", "CYCLE"], drawAll);
  const mpRow2 = el("div", "row"); mpRow2.style.gap = "6px"; play.appendChild(mpRow2);
  const add = el("button", "btn small", "+ POINT");
  add.title = "Add an evenly spaced start point (or double-click the waveform)";
  add.addEventListener("click", () => {
    const L = state.model?.layers?.[i]; if (!L?.loaded) return;
    const pts = [...(L.multipoints || [])];
    if (pts.length >= 8) { toast("Up to 8 start points"); return; }
    const s = slider(lid(i, "start")).getScaledValue(), e = slider(lid(i, "end")).getScaledValue();
    pts.push(s + (e - s) * ((pts.length + 1) / 9));
    call("setMultipoints", i, pts);
    if (combo(lid(i, "mpMode")).getChoiceIndex() === 0) combo(lid(i, "mpMode")).setChoiceIndex(1);
  });
  const clr = el("button", "btn small ghost", "CLEAR");
  clr.addEventListener("click", () => call("setMultipoints", i, []));
  mpRow2.append(add, clr);

  const tone = group(page, "TONE", (h) => { tg(h, lid(i, "mute"), "M", "coral", () => drawMini(i)); tg(h, lid(i, "solo"), "S", "amber"); });
  r = el("div", "row"); tone.appendChild(r);
  knob(r, lid(i, "semi"), "Semi", fmt.st, { def: 0, bipolar: true });
  knob(r, lid(i, "cents"), "Cents", fmt.ct, { def: 0, bipolar: true });
  knob(r, lid(i, "gain"), "Level", fmt.db, { def: i === 0 ? 0 : -6 });
  knob(r, lid(i, "pan"), "Pan", fmt.pan, { def: 0, bipolar: true });
}

// FX tab -------------------------------------------------------------
function buildFxPage() {
  const page = $("#fxPage");
  const unit = (title, onId, build) => {
    const g = group(page, title, (h, gg) => tg(h, onId, "ON", "", (on) => gg.classList.toggle("off", !on)));
    const k = el("div", "knobs"); g.appendChild(k); build(k, g);
  };
  unit("FILTER", "flt_on", (k, g) => {
    k.classList.add("three");
    const s = el("div"); s.style.margin = "-2px 0 0"; g.insertBefore(s, k);
    seg(s, "flt_type", ["LP", "HP", "BP"]);
    knob(k, "flt_cutoff", "Cutoff", fmt.hz, { def: 8000 });
    knob(k, "flt_res", "Reso", fmt.pct, { def: 0.2 });
    knob(k, "flt_mix", "Mix", fmt.pct, { def: 1 });
  });
  unit("DRIVE", "dist_on", (k) => {
    knob(k, "dist_drive", "Drive", fmt.pct, { def: 0.3 });
    knob(k, "dist_mix", "Mix", fmt.pct, { def: 1 });
  });
  unit("CHORUS", "cho_on", (k) => {
    knob(k, "cho_rate", "Rate", fmt.hz, { def: 0.6 });
    knob(k, "cho_depth", "Depth", fmt.pct, { def: 0.35 });
    knob(k, "cho_mix", "Mix", fmt.pct, { def: 0.5 });
  });
  unit("DELAY", "dly_on", (k, g) => {
    const s = el("div", "row"); s.style.gap = "6px"; s.style.alignItems = "center"; g.insertBefore(s, k);
    let timeK = null;
    const divWrap = el("div");
    tg(s, "dly_sync", "SYNC", "", (on) => { if (timeK) timeK.style.display = on ? "none" : ""; divWrap.style.display = on ? "" : "none"; });
    s.appendChild(divWrap);
    const sel = el("select", "mono dark-select");
    const cb = combo("dly_div");
    const fill = () => {
      sel.innerHTML = ""; (cb.properties.choices || []).forEach((c, j) => { const o = el("option", "", c); o.value = j; sel.appendChild(o); });
      sel.value = cb.getChoiceIndex();
    };
    cb.propertiesChangedEvent.addListener(fill); cb.valueChangedEvent.addListener(() => (sel.value = cb.getChoiceIndex())); fill();
    sel.addEventListener("change", () => cb.setChoiceIndex(+sel.value));
    divWrap.appendChild(sel);
    timeK = knob(k, "dly_time", "Time", fmt.ms, { def: 350 });
    knob(k, "dly_fb", "Feedback", fmt.pct, { def: 0.35 });
    knob(k, "dly_mix", "Mix", fmt.pct, { def: 0.25 });
    const on = !!toggle("dly_sync").getValue();
    timeK.style.display = on ? "none" : ""; divWrap.style.display = on ? "" : "none";
  });
  unit("REVERB", "rev_on", (k) => {
    knob(k, "rev_size", "Size", fmt.pct, { def: 0.6 });
    knob(k, "rev_damp", "Damp", fmt.pct, { def: 0.4 });
    knob(k, "rev_width", "Width", fmt.pct, { def: 1 });
    knob(k, "rev_mix", "Mix", fmt.pct, { def: 0.25 });
  });
}

// MIX tab -------------------------------------------------------------
function fader(parent, id, label) {
  const st = slider(id);
  const f = el("div", "fader");
  f.innerHTML = `<div class="f-track"></div><div class="f-fill"></div><div class="f-cap"></div>`;
  const fill = f.querySelector(".f-fill"), cap = f.querySelector(".f-cap");
  const val = el("div", "f-val mono");
  const render = () => {
    const n = st.getNormalisedValue();
    fill.style.height = `${n * 100}%`; cap.style.bottom = `${n * 100}%`;
    val.textContent = fmt.db(st.getScaledValue()).replace(" dB", "");
  };
  st.valueChangedEvent.addListener(render); st.propertiesChangedEvent.addListener(render);
  let drag = false;
  const setFrom = (e) => { const r = f.getBoundingClientRect(); st.setNormalisedValue(Math.min(1, Math.max(0, 1 - (e.clientY - r.top) / r.height))); };
  f.addEventListener("pointerdown", (e) => { f.setPointerCapture(e.pointerId); drag = true; st.sliderDragStarted(); setFrom(e); });
  f.addEventListener("pointermove", (e) => drag && setFrom(e));
  const end = () => { if (drag) { drag = false; st.sliderDragEnded(); } };
  f.addEventListener("pointerup", end); f.addEventListener("pointercancel", end);
  f.addEventListener("dblclick", () => { st.sliderDragStarted(); st.setNormalisedValue(scaledToNorm(st, 0)); st.sliderDragEnded(); });
  parent.append(f, val);
  render();
}

function buildMixPage() {
  const page = $("#mixPage");
  const g = group(page, "LAYER MIX");
  const strips = el("div", "strips"); g.appendChild(strips);
  for (let i = 0; i < NL; i++) {
    const s = el("div", "strip" + (i === state.sel ? " sel" : ""));
    s.appendChild(el("span", "s-n", `L${i + 1}`));
    fader(s, lid(i, "gain"));
    const ms = el("div", "ms");
    for (const [suffix, letter] of [["mute", "m"], ["solo", "s"]]) {
      const st = toggle(lid(i, suffix));
      const b = el("button", `mini-tg ${letter}`, letter.toUpperCase());
      const r = () => b.classList.toggle("on", !!st.getValue());
      st.valueChangedEvent.addListener(r); st.propertiesChangedEvent.addListener(r); r();
      b.addEventListener("click", () => st.setValue(!st.getValue()));
      ms.appendChild(b);
    }
    s.appendChild(ms);
    strips.appendChild(s);
  }
  const amp = group(page, "AMP ENVELOPE");
  let r = el("div", "row"); amp.appendChild(r);
  knob(r, "amp_attack", "Attack", fmt.ms, { def: 1 });
  knob(r, "amp_decay", "Decay", fmt.ms, { def: 400 });
  knob(r, "amp_sustain", "Sustain", fmt.pct, { def: 1 });
  knob(r, "amp_release", "Release", fmt.ms, { def: 250 });
  r = el("div", "row"); amp.appendChild(r);
  knob(r, "vel_sens", "Velocity", fmt.pct, { def: 0.8 });

  const out = group(page, "OUTPUT", (h) => tg(h, "limiter_on", "LIMIT", "coral"));
  r = el("div", "row"); out.appendChild(r);
  knob(r, "master_gain", "Master", fmt.db, { def: -3 });
  knob(r, "limiter_ceiling", "Ceiling", fmt.db, { def: -0.3, color: "#FF5A7A" });
  out.appendChild(el("span", "sec-title", "GAIN REDUCTION"));
  const gr = el("div", "gr"); gr.innerHTML = "<div id='grBar'></div>"; out.appendChild(gr);
}

// ---------------------------------------------------------------- presets
async function refreshPresets(selectName) {
  let list;
  try { list = await call("listPresets"); } catch { return; }
  if (!Array.isArray(list)) return;
  const sel = $("#presetSelect");
  sel.innerHTML = "";
  let factoryGroup = null, userGroup = null;
  list.forEach((p, k) => {
    const o = el("option", "", p.name); o.value = k; o.dataset.id = p.id;
    if (p.factory) { factoryGroup ??= Object.assign(el("optgroup"), { label: "Factory" }); factoryGroup.appendChild(o); }
    else { userGroup ??= Object.assign(el("optgroup"), { label: "User" }); userGroup.appendChild(o); }
  });
  if (factoryGroup) sel.appendChild(factoryGroup);
  if (userGroup) sel.appendChild(userGroup);
  state.presets = list;
  const cur = selectName ?? (await call("getCurrentPreset"));
  const idx = list.findIndex((p) => p.name === cur || p.id === cur);
  sel.value = idx >= 0 ? idx : "";
}

function setupPresets() {
  const sel = $("#presetSelect");
  const load = async (k) => {
    const p = state.presets?.[k]; if (!p) return;
    const r = await call("loadPreset", p.id);
    if (!r?.ok) toast(r?.error || "Could not load preset"); else { sel.value = k; toast(`Preset: ${p.name}`); }
  };
  sel.addEventListener("change", () => load(+sel.value));
  $("#presetPrev").addEventListener("click", () => { const n = state.presets?.length || 0; if (n) load(((+sel.value || 0) - 1 + n) % n); });
  $("#presetNext").addEventListener("click", () => { const n = state.presets?.length || 0; if (n) load(((+sel.value || 0) + 1) % n); });
  $("#presetSave").addEventListener("click", async () => {
    const name = prompt("Preset name", "My Mutation");
    if (!name) return;
    const r = await call("savePreset", name);
    if (r?.ok) { toast(`Saved "${name}"`); refreshPresets(name); } else toast(r?.error || "Save failed");
  });
  $("#randomize").addEventListener("click", async () => { await call("randomize"); toast("Randomized"); });
}

// ---------------------------------------------------------------- model rendering
function renderModel() {
  const M = state.model; if (!M) return;
  M.layers.forEach((L, i) => {
    const s = slots[i];
    s.root.classList.toggle("empty", !L.loaded);
    s.root.classList.toggle("busy", !!L.busy);
    s.name.textContent = L.loaded ? L.name || `Layer ${i + 1}` : "empty";
  });
  const L = M.layers[state.sel];
  $("#stageLayer").textContent = `L${state.sel + 1}`;
  $("#stageName").textContent = L.loaded ? L.name : "Empty slot";
  $("#stageInfo").textContent = L.loaded ? `${L.seconds.toFixed(2)} s · ${(L.sampleRate / 1000).toFixed(1)} kHz · ${L.index === 0 ? "original" : "variation " + L.index}` : "";
  $("#dropzone").classList.toggle("hidden", !!L.loaded);
  $("#busyVeil").classList.toggle("on", !!L.busy);
  const anyLoaded = M.layers.some((x) => x.loaded);
  const evolveAll = $("#evolveAll").checked;
  const busy = evolveAll ? M.layers.some((x) => x.busy) : !!L.busy;
  $("#evolveBtn").disabled = evolveAll ? !anyLoaded : !L.loaded;
  $("#evolveBtn").classList.toggle("busy", busy);
  $("#evolveSub").textContent = busy ? "rendering…" : evolveAll ? "all layers" : L.loaded ? `layer ${state.sel + 1}` : "load a sample";
  $("#undoBtn").disabled = !L.canUndo;
  $("#redoBtn").disabled = !L.canRedo;
  const cur = L.loaded ? L.history[L.index] : null;
  $("#seedLine").textContent = cur && cur.seed ? `seed ${cur.seed}` : "seed —";
  if (L.error) toast(L.error);
  renderHistory();
  drawAll();
}

async function onModel(m) {
  state.model = m;
  await Promise.all(m.layers.map((_, i) => fetchPeaks(i)));
  renderModel();
  animateIn();
}

let animUntil = 0;
function animateIn() { animUntil = performance.now() + 260; }

// ---------------------------------------------------------------- meters loop
let lastMeter = 0;
function tick(t) {
  requestAnimationFrame(tick);
  if (t - lastMeter > 33) {
    lastMeter = t;
    call("getMeters").then((m) => {
      const active = (m?.voices || 0) > 0 || (state.meters?.voices || 0) > 0;
      state.meters = m;
      $("#status").textContent = `${m.voices} / 16 voices · C3 root`;
      const gr = $("#grBar"); if (gr) gr.style.width = `${Math.min(100, (1 - m.limiter) * 400)}%`;
      if (active || t < animUntil) drawAll();
    }).catch(() => {});
  } else if (t < animUntil) drawMain();
}

// ---------------------------------------------------------------- boot
window.addEventListener("error", (e) => { try { call("log", `${e.message} @${e.lineno}`); } catch {} });
window.addEventListener("unhandledrejection", (e) => { try { call("log", `rejection: ${e.reason}`); } catch {} });

function boot() {
  wave.canvas = $("#wave");
  buildRail();
  buildEvolveColumn();
  setupTabs();
  setupEvolvePage();
  buildChaosPage();
  buildLayerPage();
  buildFxPage();
  buildMixPage();
  buildKeys();
  setupWaveInteraction();
  setupDrop();
  setupPresets();
  $("#browseBtn").addEventListener("click", () => call("browseSample", state.sel));
  $("#dropzone").addEventListener("click", () => call("browseSample", state.sel));
  $("#clearBtn").addEventListener("click", () => call("clearLayer", state.sel));
  $("#evolveAll").addEventListener("change", renderModel);

  window.__JUCE__.backend.addEventListener("modelChanged", onModel);
  window.__JUCE__.backend.addEventListener("notify", (msg) => toast(String(msg)));
  window.__JUCE__.backend.addEventListener("presetsChanged", (name) => refreshPresets(name));
  window.addEventListener("resize", fit);
  fit();
  selectLayer(0);
  call("getModel").then(onModel);
  refreshPresets();
  requestAnimationFrame(tick);
}

boot();
