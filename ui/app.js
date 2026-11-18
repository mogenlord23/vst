// MUTAFORM UI — retro handheld. Vanilla JS, no bundler. Talks to C++ via JUCE relays + native functions.
import * as Juce from "./juce/index.js";
import { Creature3D, POP_COLORS } from "./creature3d.js";

const NL = 5;
const $ = (s, r = document) => r.querySelector(s);
const $$ = (s, r = document) => [...r.querySelectorAll(s)];
const el = (tag, cls, html) => { const e = document.createElement(tag); if (cls) e.className = cls; if (html != null) e.innerHTML = html; return e; };
const fnCache = {};
const call = (name, ...a) => (fnCache[name] ??= Juce.getNativeFunction(name))(...a);
const lid = (i, s) => `l${i + 1}_${s}`;

const state = {
  model: null, view: "creature", sub: "generated", sel: 0, target: -1, // target -1 = ALL layers
  peaks: {}, meters: null, scale: 1, libCat: "all", lastLevel: null, octave: 0, skin: "pink",
};

// ================================================================ relays
const sliders = {}, toggles = {}, combos = {};
const slider = (id) => (sliders[id] ??= Juce.getSliderState(id));
const toggle = (id) => (toggles[id] ??= Juce.getToggleState(id));
const combo = (id) => (combos[id] ??= Juce.getComboBoxState(id));
const onSlider = (id, fn) => { const s = slider(id); s.valueChangedEvent.addListener(fn); s.propertiesChangedEvent.addListener(fn); };
const onToggle = (id, fn) => { const s = toggle(id); s.valueChangedEvent.addListener(fn); s.propertiesChangedEvent.addListener(fn); };
const onCombo = (id, fn) => { const s = combo(id); s.valueChangedEvent.addListener(fn); s.propertiesChangedEvent.addListener(fn); };
function scaledToNorm(st, v) {
  const p = st.properties;
  if (p.end === p.start) return 0;
  return Math.pow(Math.min(1, Math.max(0, (v - p.start) / (p.end - p.start))), p.skew);
}
function setScaled(id, v) { const st = slider(id); st.sliderDragStarted(); st.setNormalisedValue(scaledToNorm(st, v)); st.sliderDragEnded(); }

const fmt = {
  pct: (v) => `${Math.round(v * 100)}%`, pct100: (v) => `${Math.round(v)}%`,
  db: (v) => (v <= -47.9 ? "-INF" : `${v > 0 ? "+" : ""}${v.toFixed(1)}DB`),
  ms: (v) => (v >= 1000 ? `${(v / 1000).toFixed(2)}S` : `${Math.round(v)}MS`),
  hz: (v) => (v >= 1000 ? `${(v / 1000).toFixed(1)}K` : `${v < 10 ? v.toFixed(2) : Math.round(v)}HZ`),
  st: (v) => `${v > 0 ? "+" : ""}${Math.round(v)}ST`, ct: (v) => `${v > 0 ? "+" : ""}${Math.round(v)} CT`,
  pan: (v) => (Math.abs(v) < 0.01 ? "C" : v < 0 ? `L${Math.round(-v * 100)}` : `R${Math.round(v * 100)}`),
};

// ================================================================ pixel icons
const ICONS = {
  folder: ["............", ".#####......", "#ooooo#.....", "#ooooo######", "#oooooooooo#", "#yyyyyyyyyy#", "#yyyyyyyyyy#", "#yyyyyyyyyy#", "#yyyyyyyyyy#", "#yyyyyyyyyy#", "############", "............"],
  disk: ["############", "#ppwwwwwwpp#", "#ppwwwwwwpp#", "#ppwwwwwwpp#", "#pppppppppp#", "#pppppppppp#", "#pp######pp#", "#pp#oooo#pp#", "#pp#oooo#pp#", "#pp#oooo#pp#", "############", "............"],
  dice: ["############", "#wwwwwwwwww#", "#w##wwww##w#", "#w##wwww##w#", "#wwwwwwwwww#", "#wwww##wwww#", "#wwww##wwww#", "#wwwwwwwwww#", "#w##wwww##w#", "#w##wwww##w#", "#wwwwwwwwww#", "############"],
  undo: ["............", "...#........", "..##........", ".#########..", "..##......#.", "...#.......#", "...........#", "...........#", "..........#.", "..#######...", "............", "............"],
  lockOpen: ["....####....", "...#....#...", "...#....#...", "...#........", "...#........", "..########..", "..#yyyyyy#..", "..#yy##yy#..", "..#yy##yy#..", "..#yyyyyy#..", "..########..", "............"],
  lock: ["....####....", "...#....#...", "...#....#...", "...#....#...", "...#....#...", "..########..", "..#yyyyyy#..", "..#yy##yy#..", "..#yy##yy#..", "..#yyyyyy#..", "..########..", "............"],
  star: [".....##.....", ".....##.....", "....#..#....", "#####..#####", ".#........#.", "..#......#..", "...#....#...", "..#..##..#..", "..#.#..#.#..", ".#.#....#.#.", ".##......##.", "............"],
  starOn: [".....##.....", ".....##.....", "....#yy#....", "#####yy#####", ".#yyyyyyyy#.", "..#yyyyyy#..", "...#yyyy#...", "..#yy##yy#..", "..#y#..#y#..", ".#y#....#y#.", ".##......##.", "............"],
  loop: ["............", "...######...", "..#......##.", ".#.......###", ".#.......#..", ".#..........", "..........#.", "..#.......#.", "###.......#.", ".##......#..", "...######...", "............"],
  multi: ["............", ".#...#...#..", ".##..##..##.", ".#.#.#.#.#.#", ".#...#...#..", ".#...#...#..", ".#...#...#..", ".#...#...#..", ".#...#...#..", ".#...#...#..", ".#...#...#..", "............"],
  reverse: ["............", "............", "....#.......", "...##.......", "..##########", ".###########", "..##########", "...##.......", "....#.......", "............", "............", "............"],
  power: ["............", ".....##.....", "..#..##..#..", ".#...##...#.", "#....##....#", "#....##....#", "#..........#", "#..........#", ".#........#.", "..#......#..", "...######...", "............"],
  lib: ["############", "#wwwwwwwwww#", "#w####wwwww#", "#wwwwwwwwww#", "#w######www#", "#wwwwwwwwww#", "#w#####wwww#", "#wwwwwwwwww#", "#w####wwwww#", "#wwwwwwwwww#", "############", "............"],
  export: [".....##.....", "....####....", "...##.###...", "..#..##..#..", ".....##.....", ".....##.....", "#....##....#", "#....##....#", "#..........#", "#..........#", "############", "............"],
  speaker: ["............", ".....#......", "....##...#..", "...###....#.", "#####.#...#.", "#####..#..#.", "#####..#..#.", "#####.#...#.", "...###....#.", "....##...#..", ".....#......", "............"],
};
const ICON_COL = { "#": "#2A1C2C", o: "#D99A2B", y: "#F7C548", w: "#F3EAD9", p: "#6B4C9A" };
function iconUrl(name, ink) {
  const g = ICONS[name]; let r = "";
  g.forEach((row, y) => [...row].forEach((c, x) => { if (c !== ".") r += `<rect x="${x}" y="${y}" width="1" height="1" fill="${c === "#" && ink ? ink : ICON_COL[c]}"/>`; }));
  return `url("data:image/svg+xml;utf8,${encodeURIComponent(`<svg xmlns='http://www.w3.org/2000/svg' viewBox='0 0 12 12' shape-rendering='crispEdges'>${r}</svg>`)}")`;
}
function installIconCss() {
  let css = "";
  const map = { folder: "folder", disk: "disk", dice: "dice", undo: "undo", "lock-open": "lockOpen", lock: "lock", star: "star", "star-on": "starOn",
    loop: "loop", multi: "multi", reverse: "reverse", power: "power", lib: "lib", export: "export", speaker: "speaker" };
  for (const [cls, name] of Object.entries(map)) css += `.ic-${cls}{background-image:${iconUrl(name)}}`;
  css += `.level-plate .ic-lock-open{background-image:${iconUrl("lockOpen", "#F3EAD9")}}.level-plate .ic-lock{background-image:${iconUrl("lock", "#F3EAD9")}}`;
  css += `.level-plate .ic-star{background-image:${iconUrl("star", "#F3EAD9")}}`;
  document.head.appendChild(el("style", null, css));
}
const ic = (name) => `<i class="ic ic-${name}"></i>`;

// ================================================================ pixel knob
const KNOB_SVG = (a) => {
  // 16x16 pixel knob, pointer rotated by angle a (deg, 0 = up)
  const px = []; const c = 7.5;
  for (let y = 0; y < 16; y++) for (let x = 0; x < 16; x++) {
    const d = Math.hypot(x - c, y - c);
    if (d <= 7.6 && d > 6.4) px.push(`<rect x="${x}" y="${y}" width="1" height="1" fill="#2A1C2C"/>`);
    else if (d <= 6.4) px.push(`<rect x="${x}" y="${y}" width="1" height="1" fill="${x + y < 13 ? "#FFFDF6" : (x + y > 19 ? "#CFC3AE" : "#F3EAD9")}"/>`);
  }
  const rad = (a - 90) * Math.PI / 180;
  for (let t = 1.2; t <= 5.6; t += 0.5) {
    const x = Math.round(c + Math.cos(rad) * t), y = Math.round(c + Math.sin(rad) * t);
    px.push(`<rect x="${x}" y="${y}" width="1" height="1" fill="#2A1C2C"/>`);
  }
  return `<svg viewBox="0 0 16 16">${px.join("")}</svg>`;
};
let tipEl = null;
function knob(parent, id, label, f, opts = {}) {
  const st = slider(id);
  const wrap = opts.compact ? el("div", "kc") : el("div", "ctl" + (opts.horizontal ? " h" : ""));
  const k = el("div", "pknob" + (opts.small ? " sm" : ""));
  const lab = el("span", opts.compact ? "kl" : "lbl", label);
  const val = opts.compact ? el("span", "kv") : null;
  if (opts.horizontal) wrap.append(lab, k); else if (opts.compact) wrap.append(k, lab, val); else wrap.append(k, lab);
  let lastA = null;
  const render = () => {
    const a = Math.round((-135 + st.getNormalisedValue() * 270) / 15) * 15;
    if (a !== lastA) { k.innerHTML = KNOB_SVG(a); lastA = a; }
    if (val) val.textContent = f(st.getScaledValue());
    if (tipEl && dragging) showTip();
    opts.onChange?.(st.getScaledValue());
  };
  const showTip = () => {
    if (!tipEl) { tipEl = el("div", "knob-tip"); $("#screen").appendChild(tipEl); }
    const r = k.getBoundingClientRect(), s = $("#screen").getBoundingClientRect();
    tipEl.style.left = `${(r.left + r.width / 2 - s.left) / state.scale}px`;
    tipEl.style.top = `${(r.top - s.top) / state.scale - 4}px`;
    tipEl.textContent = `${label} ${f(st.getScaledValue())}`;
    tipEl.style.display = "block";
  };
  st.valueChangedEvent.addListener(render); st.propertiesChangedEvent.addListener(render); render();
  let startY = 0, startN = 0, dragging = false;
  k.addEventListener("pointerdown", (e) => { if (e.button) return; k.setPointerCapture(e.pointerId); dragging = true; startY = e.clientY; startN = st.getNormalisedValue(); st.sliderDragStarted(); showTip(); });
  k.addEventListener("pointermove", (e) => { if (!dragging) return; st.setNormalisedValue(Math.min(1, Math.max(0, startN + (startY - e.clientY) * (e.shiftKey ? 0.0015 : 0.007) / state.scale))); });
  const end = () => { if (!dragging) return; dragging = false; st.sliderDragEnded(); if (tipEl) tipEl.style.display = "none"; };
  k.addEventListener("pointerup", end); k.addEventListener("pointercancel", end);
  k.addEventListener("dblclick", () => { if (opts.def != null) setScaled(id, opts.def); });
  k.addEventListener("wheel", (e) => { e.preventDefault(); st.sliderDragStarted(); st.setNormalisedValue(Math.min(1, Math.max(0, st.getNormalisedValue() - Math.sign(e.deltaY) * 0.03))); st.sliderDragEnded(); }, { passive: false });
  k.title = label;
  parent.appendChild(wrap);
  return wrap;
}

// ================================================================ toast / bubble
let toastT = 0;
function toast(msg) { const t = $("#toast"); t.textContent = String(msg).toUpperCase(); t.classList.add("show"); clearTimeout(toastT); toastT = setTimeout(() => t.classList.remove("show"), 2200); }
let bubbleT = 0, bubbleSticky = "";
function say(text, ms = 2600) {
  const b = $("#bubble"); b.textContent = text; b.classList.add("show");
  clearTimeout(bubbleT);
  if (ms > 0) bubbleT = setTimeout(() => { if (bubbleSticky) say(bubbleSticky, 0); else b.classList.remove("show"); }, ms);
}
const QUIPS = ["OOH, THAT TICKLES!", "I FEEL... DIFFERENT.", "MORE! MORE!", "BZZZT!", "WHAT AM I NOW?", "SPICY!", "I LIKE THIS ONE.", "WOBBLE WOBBLE.", "NEW ME, WHO DIS?", "CRUNCHY!"];

// ================================================================ stages
const STAGES = [[0, "SPORE"], [1, "SPROUT"], [10, "CUB"], [30, "SCAMP"], [60, "YOUTH"], [120, "ADULT"], [300, "ELDER"], [1000, "ANCIENT"]];
function stageOf(level) {
  let i = 0; while (i + 1 < STAGES.length && level >= STAGES[i + 1][0]) i++;
  const lo = STAGES[i][0], hi = i + 1 < STAGES.length ? STAGES[i + 1][0] : lo * 2;
  return { index: i, name: STAGES[i][1], progress: hi > lo ? (level - lo) / (hi - lo) : 1 };
}

// ================================================================ scaling
function fit() {
  const s = Math.min(window.innerWidth / 600, window.innerHeight / 800);
  state.scale = s > 0 ? s : 1;
  const d = $("#device");
  d.style.transform = `scale(${state.scale})`;
  d.style.left = `${Math.max(0, (window.innerWidth - 600 * state.scale) / 2)}px`;
  resizeWaves();
}

// ================================================================ scene (pixel art, 168x181 grid, x3)
const W = 168, H = 181;
const scene = { ctx: null, bg: null, t: 0, stars: [], spores: [], bounce: 0, shake: 0, poof: 0, blinkT: 0 };
function mulberry(a) { return () => { a |= 0; a = (a + 0x6D2B79F5) | 0; let t = Math.imul(a ^ (a >>> 15), 1 | a); t = (t + Math.imul(t ^ (t >>> 7), 61 | t)) ^ t; return ((t ^ (t >>> 14)) >>> 0) / 4294967296; }; }

const SKINS = {
  pink:   { stripe: "#FF1FA8", floor: "#FFE600", floor2: "#E8C500", sun: "#FFE600", doodle: "#82064F" },
  lime:   { stripe: "#B8F400", floor: "#FF1FA8", floor2: "#D0128A", sun: "#FFFFFF", doodle: "#557300" },
  blue:   { stripe: "#1E7BFF", floor: "#FF8A00", floor2: "#DD7000", sun: "#FFE600", doodle: "#063483" },
  orange: { stripe: "#FF8A00", floor: "#1E7BFF", floor2: "#1660D8", sun: "#FFE600", doodle: "#864400" },
};
const SKIN_ORDER = ["pink", "lime", "blue", "orange"];

// original doodle tile (lightning, star, flower, cherries, banana-curve, spiral, heart, note)
function doodleTile(color) {
  const st = `fill="none" stroke="${color}" stroke-width="2.2" stroke-linecap="round" stroke-linejoin="round" opacity=".38"`;
  const svg = `<svg xmlns="http://www.w3.org/2000/svg" width="180" height="180" viewBox="0 0 180 180"><g ${st}>
    <path d="M20 14 l-8 16 h9 l-6 15 l16 -20 h-9 l6 -11z"/>
    <path d="M120 20 l4 9 l10 1 l-7 7 l2 10 l-9 -5 l-9 5 l2 -10 l-7 -7 l10 -1z"/>
    <g transform="translate(70 80)"><circle r="4"/><circle cx="0" cy="-9" r="5"/><circle cx="9" cy="0" r="5"/><circle cx="0" cy="9" r="5"/><circle cx="-9" cy="0" r="5"/></g>
    <g transform="translate(150 95)"><circle cx="-6" cy="10" r="6"/><circle cx="8" cy="12" r="6"/><path d="M-6 4 q4 -14 10 -18 q2 10 4 26"/><path d="M4 -14 q8 -2 12 4"/></g>
    <path d="M18 120 q6 22 30 22 q-18 -6 -22 -24 z"/>
    <path d="M110 150 m0 0 a4 4 0 1 1 8 0 a8 8 0 1 1 -16 0 a12 12 0 1 1 24 0"/>
    <path d="M60 165 c-6 -8 -18 -2 -10 8 l10 8 l10 -8 c8 -10 -4 -16 -10 -8z"/>
    <path d="M150 150 v-18 l14 -4 v18"/><circle cx="147" cy="152" r="3.5"/><circle cx="161" cy="148" r="3.5"/>
  </g></svg>`;
  return `url("data:image/svg+xml;utf8,${encodeURIComponent(svg)}")`;
}

function buildBackground() {
  const sk = SKINS[state.skin] || SKINS.pink;
  const c = document.createElement("canvas"); c.width = W; c.height = H;
  const g = c.getContext("2d");
  // pop-art stripes (black / skin colour)
  for (let y = 0; y < 132; y++) { g.fillStyle = Math.floor(y / 6) % 2 ? "#141014" : sk.stripe; g.fillRect(0, y, W, 1); }
  // halftone sun
  const cx = 132, cy = 62;
  g.fillStyle = sk.sun;
  for (let y = -26; y <= 26; y += 3) for (let x = -26; x <= 26; x += 3) {
    const d = Math.hypot(x, y); if (d > 26) continue;
    const rr = d < 15 ? 2 : d < 21 ? 1.4 : 1;
    g.beginPath(); g.arc(cx + x + ((y / 3) % 2 ? 1.5 : 0), cy + y, rr, 0, 6.28); g.fill();
  }
  g.beginPath(); g.arc(cx, cy, 13, 0, 6.28); g.fill();
  // comic burst behind the creature
  g.fillStyle = "rgba(255,255,255,.16)";
  g.beginPath();
  for (let k = 0; k < 28; k++) { const a = k / 28 * Math.PI * 2, r = k % 2 ? 46 : 72; const x = 84 + Math.cos(a) * r, y = 112 + Math.sin(a) * r * 0.75; k ? g.lineTo(x, y) : g.moveTo(x, y); }
  g.closePath(); g.fill();
  // floor with halftone shading
  for (let y = 132; y < H; y++) { g.fillStyle = y < 134 ? "#141014" : sk.floor; g.fillRect(0, y, W, 1); }
  g.fillStyle = sk.floor2;
  for (let y = 136; y < H; y += 3) for (let x = (y % 2) * 2; x < W; x += 4) { const k = (y - 136) / (H - 136); if (k > 0.35) g.fillRect(x, y, k > 0.7 ? 2 : 1, k > 0.7 ? 2 : 1); }
  scene.bg = c;
  const rs = mulberry(5);
  scene.stars = Array.from({ length: 10 }, () => ({ x: 8 + Math.floor(rs() * (W - 16)), y: 10 + Math.floor(rs() * 100), p: rs() * 6.28, b: true }));
  scene.spores = Array.from({ length: 8 }, () => ({ x: rs() * W, y: 60 + rs() * 110, v: 0.05 + rs() * 0.08, p: rs() * 6.28 }));
}

function applySkin(name, persist = true) {
  state.skin = SKINS[name] ? name : "pink";
  document.body.classList.remove(...SKIN_ORDER.map((k) => "skin-" + k));
  document.body.classList.add("skin-" + state.skin);
  document.documentElement.style.setProperty("--doodle", doodleTile(SKINS[state.skin].doodle));
  if (scene.ctx) buildBackground();
  if (persist) call("setSkin", state.skin);
}

// --------------------------------------------------------------- creature
function hsl(h, s, l) { return `hsl(${h},${s}%,${l}%)`; }
function traitsFromModel() {
  const M = state.model;
  const seedHex = M?.creatureSeed || "1";
  let seed = 0; for (const ch of seedHex) seed = (Math.imul(seed, 31) + ch.charCodeAt(0)) | 0;
  const r = mulberry(seed);
  const main = M?.layers?.find((l) => l.loaded) || {};
  const bright = main.brightness ?? 0.5, punch = main.punch ?? 0.4, motion = main.motion ?? 0.4;
  const hue = (bright < 0.5 ? 220 + (0.5 - bright) * 120 : 25 + (1 - bright) * 140) + (r() - 0.5) * 50;
  const P = POP_COLORS;
  const floor = SKINS[state.skin]?.floor;
  const pick = (arr) => { const a = arr.filter((c) => c !== floor); return a[Math.floor(r() * a.length)]; };
  const hexOf = (n) => "#" + n.toString(16).padStart(6, "0");
  const bodyColor = bright >= 0.5 ? pick([P.yellow, P.orange, P.lime, P.pink].map(hexOf)) : pick([P.blue, P.violet, P.cyan, P.pink].map(hexOf));
  const accentColor = pick([P.pink, P.cyan, P.yellow, P.lime, P.orange].map(hexOf).filter((c) => c !== bodyColor));
  return {
    seed, motion, bodyColor: parseInt(bodyColor.slice(1), 16), accentColor: parseInt(accentColor.slice(1), 16),
    lidColor: P.cyan, footColor: bright >= 0.5 ? P.blue : P.yellow, hair: r() < 0.75, colorKey: state.skin,
    hue: (hue + 360) % 360, sat: 52 + Math.floor(r() * 20), body: Math.floor(r() * 5), eyes: r() < 0.25 ? 1 : 2, eyeBig: r() < 0.5,
    spikes: punch > 0.45 ? 3 + Math.floor(r() * 3) : 0, pattern: Math.floor(r() * 3), antenna: r() < 0.6, wobble: 0.6 + motion * 1.6,
    horns: r() < 0.5, ears: r() < 0.4, accentHue: (hue + 140 + r() * 80) % 360, r,
  };
}
let traits = null;

function drawCreature(g, t) {
  const M = state.model;
  const level = M?.level ?? 0;
  const st = stageOf(level);
  const anyLoaded = M?.layers?.some((l) => l.loaded);
  const tr = traits || (traits = traitsFromModel());
  const cx = 84, ground = 146;
  const playing = (state.meters?.voices || 0) > 0;
  const busy = M?.layers?.some((l) => l.busy);
  scene.bounce = playing ? Math.min(1, scene.bounce + 0.25) : Math.max(0, scene.bounce - 0.08);
  const shakeX = busy ? Math.round(Math.sin(t * 40) * 1.5) : 0;

  // shadow
  const sizes = [20, 22, 28, 34, 40, 46, 50, 54];
  const size = sizes[st.index];
  g.fillStyle = "rgba(120,48,70,.45)";
  for (let x = -size / 2 - 2; x <= size / 2 + 2; x++) { const hh = Math.round(3 * Math.sqrt(Math.max(0, 1 - (x / (size / 2 + 3)) ** 2))); g.fillRect(cx + x, ground - hh + 1, 1, hh * 2); }

  if (!anyLoaded || st.index === 0) { drawSpore(g, cx + shakeX, ground, t, anyLoaded); return; }

  const bob = Math.round(Math.sin(t * 2.2 * tr.wobble) * 1.2);
  const squash = scene.bounce * 0.12 * (0.5 + 0.5 * Math.sin(t * 18));
  const w = Math.round(size * (tr.body === 3 ? 1.25 : tr.body === 1 ? 0.8 : 1) * (1 + squash)), h = Math.round(size * (tr.body === 1 ? 1.2 : tr.body === 3 ? 0.8 : 1) * (1 - squash));
  const x0 = cx - Math.floor(w / 2) + shakeX, y0 = ground - h - 2 + bob - (st.index >= 2 ? 3 : 0);
  const base = tr.hue, s = tr.sat;
  const cLight = hsl(base, s, 70), cMid = hsl(base, s, 56), cDark = hsl(base, s - 5, 40), cLine = "#2A1C2C", cAcc = hsl(tr.accentHue, 60, 62);

  // feet / legs
  if (st.index >= 2) {
    g.fillStyle = cDark;
    const fw = Math.max(3, Math.round(w / 7)), legH = st.index >= 4 ? 4 : 3;
    const step = playing ? Math.round(Math.sin(t * 14)) : 0;
    g.fillRect(cx - Math.round(w / 4) - fw / 2 + shakeX, ground - legH - 1 + step, fw, legH + 1);
    g.fillRect(cx + Math.round(w / 4) - fw / 2 + shakeX, ground - legH - 1 - step, fw, legH + 1);
    g.fillStyle = cLine;
    g.fillRect(cx - Math.round(w / 4) - fw / 2 + shakeX, ground + step, fw, 1);
    g.fillRect(cx + Math.round(w / 4) - fw / 2 + shakeX, ground - step, fw, 1);
  }

  // body raster with outline + 3-tone shading + pattern
  const inside = (u, v) => {
    switch (tr.body) {
      case 2: return Math.pow(Math.abs(u / (0.7 + 0.3 * (v + 1) / 2)), 2) + v * v <= 1;          // pear
      case 4: return Math.pow(Math.abs(u), 3.2) + Math.pow(Math.abs(v), 3.2) <= 1;              // boxy
      default: return u * u + v * v <= 1;
    }
  };
  const ins = (x, y) => inside((x + 0.5 - w / 2) / (w / 2), (y + 0.5 - h / 2) / (h / 2));
  for (let y = 0; y < h; y++) for (let x = 0; x < w; x++) {
    if (!ins(x, y)) continue;
    const edge = !ins(x - 1, y) || !ins(x + 1, y) || !ins(x, y - 1) || !ins(x, y + 1);
    const u = (x - w / 2) / (w / 2), v = (y - h / 2) / (h / 2);
    let col = edge ? cLine : (u + v < -0.7 ? cLight : (u + v > 0.55 ? cDark : cMid));
    if (!edge && tr.pattern === 1 && st.index >= 4 && ((x * 7 + y * 13) % 23 === 0 || (x * 5 + y * 11) % 29 === 0)) col = cAcc;
    if (!edge && tr.pattern === 2 && st.index >= 4 && Math.floor((y + x * 0.3) / 4) % 3 === 0 && v > -0.2) col = hsl(base, s, 48);
    if (!edge && v > 0.25 && Math.abs(u) < 0.45 && st.index >= 3) col = hsl(base, s - 15, 76); // belly
    g.fillStyle = col; g.fillRect(x0 + x, y0 + y, 1, 1);
  }
  // highlight
  g.fillStyle = "rgba(255,255,255,.75)"; g.fillRect(x0 + Math.round(w * 0.27), y0 + Math.round(h * 0.18), 2, 1); g.fillRect(x0 + Math.round(w * 0.22), y0 + Math.round(h * 0.24), 1, 2);

  // spikes on top (plucky sounds)
  if (tr.spikes && st.index >= 3) {
    for (let k = 0; k < tr.spikes; k++) {
      const sx = x0 + Math.round(w * (0.3 + 0.4 * (k / Math.max(1, tr.spikes - 1))));
      const sy = y0 + Math.round(h * 0.08) + Math.abs(k - (tr.spikes - 1) / 2);
      g.fillStyle = cLine; g.fillRect(sx - 1, sy - 4, 3, 1); g.fillRect(sx, sy - 6, 1, 2);
      g.fillStyle = cAcc; g.fillRect(sx, sy - 4, 1, 3); g.fillRect(sx - 1, sy - 2, 3, 2);
    }
  }
  // sprout leaf / antenna
  if (st.index === 1) { g.fillStyle = "#3F9E6E"; g.fillRect(cx + shakeX, y0 - 4, 1, 4); g.fillRect(cx + 1 + shakeX, y0 - 5, 3, 2); g.fillRect(cx - 3 + shakeX, y0 - 4, 3, 1); }
  if (tr.antenna && st.index >= 3) {
    const ax = x0 + Math.round(w * 0.62), ay = y0 + 2;
    const sway = Math.round(Math.sin(t * 3) * 1.5);
    g.fillStyle = cLine; for (let i = 0; i < 6; i++) g.fillRect(ax + Math.round(sway * i / 6), ay - i, 1, 1);
    g.fillStyle = cAcc; g.fillRect(ax + sway - 1, ay - 8, 3, 3);
  }
  if (tr.horns && st.index >= 5) {
    g.fillStyle = "#FFF1D6";
    [[0.22, -1], [0.78, 1]].forEach(([p, dir]) => { const hx = x0 + Math.round(w * p), hy = y0 + 3; for (let i = 0; i < 5; i++) g.fillRect(hx + dir * Math.floor(i / 2), hy - i, 2, 1); });
  }
  if (tr.ears && st.index >= 2 && !tr.horns) {
    g.fillStyle = cDark; g.fillRect(x0 + 1, y0 + Math.round(h * 0.25), 2, 4); g.fillRect(x0 + w - 3, y0 + Math.round(h * 0.25), 2, 4);
  }
  // arms
  if (st.index >= 3) {
    const ay = y0 + Math.round(h * 0.55), wave = playing ? Math.round(Math.sin(t * 16) * 2) : 0;
    g.fillStyle = cDark; g.fillRect(x0 - 3, ay - wave, 4, 3); g.fillRect(x0 + w - 1, ay + wave, 4, 3);
    g.fillStyle = cLine; g.fillRect(x0 - 4, ay - wave, 1, 3); g.fillRect(x0 + w + 3, ay + wave, 1, 3);
  }

  // face
  scene.blinkT -= 1 / 12;
  if (scene.blinkT < -0.15) scene.blinkT = 2 + Math.random() * 4;
  const blink = scene.blinkT < 0;
  const ey = y0 + Math.round(h * (tr.body === 1 ? 0.36 : 0.42));
  const eyePos = tr.eyes === 1 ? [cx + shakeX] : [cx - Math.round(w * 0.17) + shakeX, cx + Math.round(w * 0.17) + shakeX];
  const es = tr.eyeBig && st.index >= 2 ? 4 : 3;
  eyePos.forEach((ex) => {
    if (blink || busy) { g.fillStyle = cLine; g.fillRect(ex - Math.floor(es / 2), ey + 1, es, 1); return; }
    g.fillStyle = "#FFFFFF"; g.fillRect(ex - Math.floor(es / 2), ey - 1, es, es + 1);
    g.fillStyle = cLine; g.fillRect(ex - Math.floor(es / 2) + 1, ey, es - 1, es - 1);
    g.fillStyle = "#FFFFFF"; g.fillRect(ex - Math.floor(es / 2) + 1, ey, 1, 1);
  });
  // cheeks + mouth
  g.fillStyle = "rgba(255,120,150,.8)";
  if (st.index >= 2) { g.fillRect(eyePos[0] - 4, ey + es + 1, 2, 1); g.fillRect((eyePos[1] ?? eyePos[0]) + 3, ey + es + 1, 2, 1); }
  g.fillStyle = cLine;
  const my = ey + es + 2;
  if (playing) { g.fillRect(cx - 1 + shakeX, my, 3, 1); g.fillRect(cx - 2 + shakeX, my + 1, 1, 2); g.fillRect(cx + 2 + shakeX, my + 1, 1, 2); g.fillRect(cx - 1 + shakeX, my + 3, 3, 1); }
  else { g.fillRect(cx - 2 + shakeX, my, 1, 1); g.fillRect(cx - 1 + shakeX, my + 1, 3, 1); g.fillRect(cx + 2 + shakeX, my, 1, 1); }

  // elder / ancient extras
  if (st.index >= 6) {
    g.fillStyle = "#FFF6E6";
    for (let i = -2; i <= 2; i++) g.fillRect(cx + i * 2 + shakeX, my + 5 + Math.abs(i), 1, 3 - Math.abs(i) % 2); // whiskers/beard
    for (let k = 0; k < 3; k++) { // orbiting orbs
      const a = t * 1.4 + k * 2.09;
      g.fillStyle = k === 0 ? "#FFE48A" : cAcc;
      g.fillRect(Math.round(cx + Math.cos(a) * (w / 2 + 9)) + shakeX, Math.round(y0 + h / 2 + Math.sin(a) * 6), 2, 2);
    }
  }
  if (st.index >= 7) {
    const cy = y0 - 6;
    g.fillStyle = "#8FF0E0"; [[-6, 3], [-2, 5], [2, 4], [6, 3]].forEach(([dx, hh]) => g.fillRect(cx + dx + shakeX, cy - hh + 3, 2, hh));
    g.fillStyle = "rgba(143,240,224,.18)";
    const rr = w / 2 + 6 + Math.round(Math.sin(t * 3));
    for (let a = 0; a < 6.28; a += 0.08) g.fillRect(Math.round(cx + Math.cos(a) * rr), Math.round(y0 + h / 2 + Math.sin(a) * (h / 2 + 6)), 1, 1);
  }
}

function drawSpore(g, cx, ground, t, fed) {
  const wob = Math.round(Math.sin(t * (fed ? 6 : 1.6)) * (fed ? 1.5 : 0.6));
  const w = 18, h = 22, x0 = cx - 9 + wob, y0 = ground - h - 1;
  for (let y = 0; y < h; y++) for (let x = 0; x < w; x++) {
    const u = (x + 0.5 - w / 2) / (w / 2), v = (y + 0.5 - h / 2) / (h / 2);
    const vv = v < 0 ? v * 1.1 : v;
    if (u * u + vv * vv > 1) continue;
    const e = (u * 1.1) ** 2 + vv ** 2 > 0.78;
    const swirl = Math.sin(Math.atan2(v, u) * 3 + Math.hypot(u, v) * 7) > 0.55;
    g.fillStyle = e ? "#2A1C2C" : swirl ? "#B79CE6" : (u + v < -0.5 ? "#FFF4E4" : u + v > 0.6 ? "#D9C6B0" : "#F3E6D3");
    g.fillRect(x0 + x, y0 + y, 1, 1);
  }
  g.fillStyle = "#FFFFFF"; g.fillRect(x0 + 5, y0 + 5, 2, 1); g.fillRect(x0 + 4, y0 + 6, 1, 2);
  if (fed) { g.fillStyle = "#2A1C2C"; g.fillRect(x0 + 8, y0 + 3, 1, 4); g.fillRect(x0 + 9, y0 + 6, 2, 1); g.fillRect(x0 + 10, y0 + 7, 1, 3); }
}

function drawScene(t) {
  const g = scene.ctx;
  g.drawImage(scene.bg, 0, 0);
  // twinkling stars
  scene.stars.forEach((s) => {
    const on = Math.sin(t * 2 + s.p) > -0.4;
    if (!on) return;
    g.fillStyle = s.b ? "#FFF6D8" : "rgba(255,240,220,.65)"; g.fillRect(s.x, s.y, 1, 1);
    if (s.b && Math.sin(t * 2 + s.p) > 0.8) { g.fillRect(s.x - 1, s.y, 3, 1); g.fillRect(s.x, s.y - 1, 1, 3); }
  });
  // floating spores
  scene.spores.forEach((p) => {
    p.y -= p.v; p.x += Math.sin(t + p.p) * 0.08; if (p.y < 40) { p.y = 170; p.x = Math.random() * W; }
    g.fillStyle = "rgba(255,230,210,.55)"; g.fillRect(Math.round(p.x), Math.round(p.y), 1, 1);
  });
  if (state.view === "creature" && !(c3d && c3d.ok)) drawCreature(g, t);
  // mutation poof
  if (scene.poof > 0) {
    const r = mulberry(Math.floor(scene.poof * 100));
    for (let i = 0; i < 40; i++) {
      const a = r() * 6.28, d = (1 - scene.poof) * 50 * r();
      g.fillStyle = i % 3 ? "#FFF1D6" : "#FF6F91";
      g.fillRect(Math.round(84 + Math.cos(a) * d), Math.round(118 + Math.sin(a) * d * 0.7), 2, 2);
    }
    scene.poof = Math.max(0, scene.poof - 0.06);
  }
}

// ================================================================ 3D creature
let c3d = null, lastT = 0;
function render3D(t) {
  if (!c3d || !c3d.ok) return;
  const cv = $("#creature3d");
  cv.style.display = state.view === "creature" ? "" : "none";
  if (state.view !== "creature") return;
  const M = state.model;
  const st = stageOf(M?.level ?? 0);
  const anyLoaded = M?.layers?.some((l) => l.loaded);
  const tr = traits || (traits = traitsFromModel());
  c3d.build(tr, anyLoaded ? st.index : 0);
  const dt = Math.min(0.1, Math.max(0.001, t - lastT)); lastT = t;
  c3d.render(t, dt, { playing: (state.meters?.voices || 0) > 0, busy: !!M?.layers?.some((l) => l.busy) });
}

// ================================================================ main view
function updateCreatureView() {
  const M = state.model; if (!M) return;
  const st = stageOf(M.level);
  $("#levelText").textContent = `LEVEL ${M.level} - ${st.name}`;
  $("#levelFill").style.width = `${Math.round(Math.min(1, st.progress) * 100)}%`;
  $("#lvPrev").disabled = !M.canPrevLevel;
  $("#lvNext").disabled = !M.canNextLevel;
  $("#btnUndoDice").classList.toggle("hidden", !M.canUndoDice);
  const tgt = targetLayers();
  const allLocked = tgt.length > 0 && tgt.every((i) => M.layers[i].locked);
  $("#lvLock").innerHTML = ic(allLocked ? "lock" : "lock-open");
  const fav = tgt.length > 0 && tgt.every((i) => !M.layers[i].loaded || M.layers[i].history[M.layers[i].index]?.fav);
  $("#lvStar").innerHTML = ic(fav && tgt.some((i) => M.layers[i].loaded && M.layers[i].index > 0) ? "star-on" : "star");
  const label = state.target < 0 ? "ALL" : `${state.target + 1}`;
  $("#layerBox").textContent = label; $("#layerBox2").textContent = label;
  const anyLoaded = M.layers.some((l) => l.loaded);
  const busy = M.layers.some((l) => l.busy);
  $("#btnEvolve").classList.toggle("busy", busy);
  bubbleSticky = !anyLoaded ? "DROP A ONE-SHOT ON ME. I'M HUNGRY!" : busy ? "MUTATING..." : "";
  if (bubbleSticky) say(bubbleSticky, 0);
  else if ($("#bubble").classList.contains("show") && ($("#bubble").textContent.startsWith("DROP") || $("#bubble").textContent === "MUTATING...")) $("#bubble").classList.remove("show");
  if (M.level !== state.lastLevel) {
    const before = stageOf(state.lastLevel), after = stageOf(M.level);
    if (after.index > before.index && state.lastLevel !== null) say(`I GREW INTO A ${after.name}!`, 3200);
    else if (M.level > state.lastLevel) say(QUIPS[Math.floor(Math.random() * QUIPS.length)]);
    if (M.level !== state.lastLevel) scene.poof = 1;
    state.lastLevel = M.level;
  }
}

function targetLayers() { return state.target < 0 ? [0, 1, 2, 3, 4] : [state.target]; }
function cycleTarget() {
  const loaded = state.model?.layers?.map((l, i) => (l.loaded ? i : -1)).filter((i) => i >= 0) || [];
  const order = [-1, ...loaded];
  const k = order.indexOf(state.target);
  state.target = order[(k + 1) % order.length];
  if (state.target >= 0) selectLayer(state.target);
  updateCreatureView();
}

async function doEvolve() {
  const M = state.model;
  if (!M?.layers?.some((l) => l.loaded)) { say("FEED ME FIRST! DROP A SOUND.", 2400); return; }
  if (state.target < 0) {
    const n = await call("evolveAll", 0x1f);
    if (!n) toast(M.layers.every((l) => !l.loaded || l.locked) ? "All layers are locked" : "Busy...");
  } else {
    const L = M.layers[state.target];
    if (!L.loaded) { toast("Layer is empty"); return; }
    if (L.locked) { toast("Layer is locked"); return; }
    await call("evolve", state.target, "");
  }
}

// ================================================================ one-shots rows
const rows = [];
function buildToolbar() {
  const tb = $("#toolbar");
  const tbtn = (label, id) => {
    const b = el("button", "tbtn", label);
    const r = () => b.classList.toggle("on", !!toggle(lid(state.sel, id)).getValue());
    b.addEventListener("click", () => { const t = toggle(lid(state.sel, id)); t.setValue(!t.getValue()); });
    b._render = r; tb.appendChild(b); return b;
  };
  const loopB = tbtn("LOOP", "loop"), revB = tbtn("REVERSE", "reverse");
  const multiB = el("button", "tbtn", "MULTIPOINT");
  multiB._render = () => multiB.classList.toggle("on", combo(lid(state.sel, "mpMode")).getChoiceIndex() > 0);
  multiB.addEventListener("click", () => { const c = combo(lid(state.sel, "mpMode")); c.setChoiceIndex((c.getChoiceIndex() + 1) % 4); toast(["MULTIPOINT OFF", "MULTIPOINT: BY KEY", "MULTIPOINT: BY VELOCITY", "MULTIPOINT: CYCLE"][c.getChoiceIndex()]); });
  tb.insertBefore(multiB, revB);
  const gain = el("div", "tbtn", `GAIN <span class="ar" data-d="-1">&#9664;</span><span class="tb-val" id="tbGain"></span><span class="ar" data-d="1">&#9654;</span>`);
  gain.addEventListener("click", (e) => { const d = +e.target.dataset.d; if (!d) return; const id = lid(state.sel, "gain"); setScaled(id, Math.max(-48, Math.min(12, Math.round(slider(id).getScaledValue()) + d))); });
  const oct = el("div", "tbtn", `OCT <span class="ar" data-d="-1">&#9664;</span><span class="tb-val" id="tbOct"></span><span class="ar" data-d="1">&#9654;</span>`);
  oct.addEventListener("click", (e) => { const d = +e.target.dataset.d; if (!d) return; shiftOct(state.sel, d); });
  const ex = el("button", "tbtn icon", ic("export")); ex.title = "Export this layer as WAV";
  ex.addEventListener("click", () => call("exportLayer", state.sel));
  tb.append(gain, oct, ex);
  state.toolbar = { loopB, revB, multiB };
  const renderTb = () => {
    loopB._render(); revB._render(); multiB._render();
    $("#tbGain").textContent = `${Math.round(slider(lid(state.sel, "gain")).getScaledValue())}DB`;
    $("#tbOct").textContent = `${Math.round(slider(lid(state.sel, "semi")).getScaledValue() / 12)}`;
  };
  state.renderToolbar = renderTb;
  for (let i = 0; i < NL; i++) { onToggle(lid(i, "loop"), renderTb); onToggle(lid(i, "reverse"), renderTb); onCombo(lid(i, "mpMode"), renderTb); onSlider(lid(i, "gain"), renderTb); onSlider(lid(i, "semi"), renderTb); }
  renderTb();
}
function shiftOct(i, d) {
  const id = lid(i, "semi");
  const cur = Math.round(slider(id).getScaledValue());
  setScaled(id, Math.max(-24, Math.min(24, (Math.round(cur / 12) + d) * 12 + (cur % 12))));
}

function buildRows() {
  const box = $("#rows");
  for (let i = 0; i < NL; i++) {
    const r = el("div", "row empty");
    r.innerHTML = `<div class="tile">L${i + 1}</div><div class="wave"><canvas></canvas><div class="empty-txt">DROP A ONE-SHOT</div></div>
      <div class="icons">
        <button class="ib" data-a="loop" title="Loop">${ic("loop")}</button>
        <button class="ib" data-a="multi" title="Multipoint (cycle mode)">${ic("multi")}</button>
        <button class="ib" data-a="reverse" title="Reverse">${ic("reverse")}</button>
        <button class="ib" data-a="reset" title="Back to the original sound">${ic("power")}</button>
        <button class="ib" data-a="lib" title="Variation library">${ic("lib")}</button>
        <button class="ib" data-a="mute" title="Mute">M</button>
        <button class="ib" data-a="solo" title="Solo">S</button>
        <button class="ib oct" data-a="oct" title="Octave"><span data-d="-1">&#9664;</span><b class="ov">0</b><span data-d="1">&#9654;</span></button>
        <button class="ib" data-a="lock" title="Lock (EVOLVE / dice skip this layer)">${ic("lock-open")}</button>
        <button class="ib" data-a="export" title="Export WAV">${ic("export")}</button>
      </div>`;
    const canvas = r.querySelector("canvas");
    r.querySelector(".tile").addEventListener("click", () => { selectLayer(i); if (!state.model?.layers?.[i]?.loaded) call("browseSample", i); });
    r.querySelector(".icons").addEventListener("click", (e) => {
      const b = e.target.closest(".ib"); if (!b) return;
      selectLayer(i);
      const a = b.dataset.a;
      const tgl = (s) => { const t = toggle(lid(i, s)); t.setValue(!t.getValue()); };
      if (a === "loop") tgl("loop");
      else if (a === "reverse") tgl("reverse");
      else if (a === "mute") tgl("mute");
      else if (a === "solo") tgl("solo");
      else if (a === "multi") { const c = combo(lid(i, "mpMode")); c.setChoiceIndex(c.getChoiceIndex() > 0 ? 0 : 1); if (c.getChoiceIndex() > 0 && !(state.model.layers[i].multipoints || []).length) addEvenPoints(i); }
      else if (a === "reset") call("resetLayer", i);
      else if (a === "lib") { openSub("library"); }
      else if (a === "lock") call("setLocked", i, !state.model.layers[i].locked);
      else if (a === "export") call("exportLayer", i);
      else if (a === "oct") { const d = +e.target.dataset.d; if (d) shiftOct(i, d); }
    });
    r.addEventListener("dragenter", (e) => { e.preventDefault(); r.classList.add("drag"); dropLayer = i; $("#dropTarget").textContent = `LAYER ${i + 1}`; });
    r.addEventListener("dragleave", () => r.classList.remove("drag"));
    box.appendChild(r);
    rows.push({ root: r, canvas, ib: (a) => r.querySelector(`[data-a=${a}]`) });
    setupWaveDrag(i, canvas);
    const rr = () => renderRow(i);
    ["loop", "reverse", "mute", "solo"].forEach((s) => onToggle(lid(i, s), rr));
    ["start", "end", "loopStart", "loopEnd", "semi"].forEach((s) => onSlider(lid(i, s), rr));
    onCombo(lid(i, "mpMode"), rr);
  }
}

function addEvenPoints(i) {
  const s = slider(lid(i, "start")).getScaledValue(), e = slider(lid(i, "end")).getScaledValue();
  call("setMultipoints", i, [0, 1, 2, 3].map((k) => s + (e - s) * k / 4));
}

function renderRow(i) {
  const R = rows[i]; if (!R) return;
  const L = state.model?.layers?.[i];
  R.root.classList.toggle("empty", !L?.loaded);
  R.root.classList.toggle("busy", !!L?.busy);
  R.root.classList.toggle("sel", i === state.sel);
  R.root.querySelector(".empty-txt").style.display = L?.loaded ? "none" : "";
  const on = (a, v, cls = "") => { const b = R.ib(a); b.classList.toggle("on", !!v); if (cls) b.classList.toggle(cls, !!v); };
  on("loop", toggle(lid(i, "loop")).getValue());
  on("reverse", toggle(lid(i, "reverse")).getValue());
  on("mute", toggle(lid(i, "mute")).getValue(), "red");
  on("solo", toggle(lid(i, "solo")).getValue());
  on("multi", combo(lid(i, "mpMode")).getChoiceIndex() > 0);
  on("lock", L?.locked, "mint");
  R.ib("lock").innerHTML = ic(L?.locked ? "lock" : "lock-open");
  R.root.querySelector(".ov").textContent = Math.round(slider(lid(i, "semi")).getScaledValue() / 12);
  drawRowWave(i);
}

function resizeWaves() {
  rows.forEach((R, i) => {
    const r = R.canvas.getBoundingClientRect();
    const w = Math.max(1, Math.round(r.width / state.scale)), h = Math.max(1, Math.round(r.height / state.scale));
    R.canvas.width = w; R.canvas.height = h; drawRowWave(i);
  });
}

function drawRowWave(i) {
  const R = rows[i]; const c = R?.canvas; if (!c || !c.width) return;
  const g = c.getContext("2d"); const w = c.width, h = c.height;
  g.clearRect(0, 0, w, h);
  const L = state.model?.layers?.[i]; if (!L?.loaded) return;
  const P = state.peaks[i]?.mini;
  const s = slider(lid(i, "start")).getScaledValue(), e = slider(lid(i, "end")).getScaledValue();
  const loop = !!toggle(lid(i, "loop")).getValue();
  const muted = !!toggle(lid(i, "mute")).getValue();
  if (loop) { g.fillStyle = "rgba(247,181,72,.28)"; const a = slider(lid(i, "loopStart")).getScaledValue(), b = slider(lid(i, "loopEnd")).getScaledValue(); g.fillRect(Math.round(a * w), 0, Math.round((b - a) * w), h); }
  if (P) {
    const n = P.length / 2, mid = h / 2;
    let pk = 0.05; for (let k = 0; k < P.length; k++) pk = Math.max(pk, Math.abs(P[k]));
    for (let x = 0; x < w; x += 1) {
      const k = Math.min(n - 1, Math.floor(x / w * n));
      const mx = Math.max(Math.abs(P[k * 2]), Math.abs(P[k * 2 + 1])) / pk;
      const hh = Math.max(1, Math.round(mx * mid * 0.92));
      const inside = x / w >= s && x / w <= e;
      g.fillStyle = muted ? "#B9ADA0" : inside ? "#6B4C9A" : "#C9B9DA";
      g.fillRect(x, Math.round(mid - hh), 1, hh * 2);
    }
  }
  const mark = (x, col, top) => { const xx = Math.min(w - 2, Math.max(0, Math.round(x * w))); g.fillStyle = col; g.fillRect(xx, 0, 2, h); g.fillRect(top ? xx : xx - 3, 0, 5, 4); };
  mark(s, "#2FA37E", true); mark(e, "#E8505B", false);
  if (loop) { const a = slider(lid(i, "loopStart")).getScaledValue(), b = slider(lid(i, "loopEnd")).getScaledValue(); g.fillStyle = "#D99A2B"; g.fillRect(Math.round(a * w), h - 4, 2, 4); g.fillRect(Math.round(b * w) - 2, h - 4, 2, 4); }
  if (combo(lid(i, "mpMode")).getChoiceIndex() > 0) (L.multipoints || []).forEach((p) => { g.fillStyle = "#FF6F91"; g.fillRect(Math.round(p * w), h - 7, 1, 7); g.fillRect(Math.round(p * w) - 1, h - 7, 3, 2); });
  const ph = state.meters?.playheads?.[i];
  if (ph != null && ph >= 0) { g.fillStyle = "#2A1C2C"; g.fillRect(Math.round(ph * w), 0, 1, h); }
}

function setupWaveDrag(i, c) {
  let drag = null;
  const pos = (e) => { const r = c.getBoundingClientRect(); return Math.min(1, Math.max(0, (e.clientX - r.left) / r.width)); };
  const hit = (x) => {
    const tol = 6 / (c.width || 300);
    const cands = [["end", slider(lid(i, "end")).getScaledValue()], ["start", slider(lid(i, "start")).getScaledValue()]];
    if (toggle(lid(i, "loop")).getValue()) cands.unshift(["loopEnd", slider(lid(i, "loopEnd")).getScaledValue()], ["loopStart", slider(lid(i, "loopStart")).getScaledValue()]);
    if (combo(lid(i, "mpMode")).getChoiceIndex() > 0) (state.model?.layers?.[i]?.multipoints || []).forEach((p, k) => cands.unshift(["mp" + k, p]));
    let best = null, bd = tol; for (const [k, v] of cands) { const d = Math.abs(v - x); if (d <= bd) { bd = d; best = k; } }
    return best;
  };
  c.addEventListener("pointerdown", (e) => {
    selectLayer(i);
    if (!state.model?.layers?.[i]?.loaded) { call("browseSample", i); return; }
    const x = pos(e), h = hit(x);
    if (e.button === 2) { if (h?.startsWith("mp")) { const pts = [...state.model.layers[i].multipoints]; pts.splice(+h.slice(2), 1); call("setMultipoints", i, pts); } return; }
    if (!h) { call("noteOn", 60, 0.85); setTimeout(() => call("noteOff", 60), 350); return; } // click = audition
    c.setPointerCapture(e.pointerId); drag = h;
    if (!h.startsWith("mp")) slider(lid(i, h)).sliderDragStarted();
  });
  c.addEventListener("pointermove", (e) => {
    const x = pos(e);
    if (!drag) { c.style.cursor = hit(x) ? "ew-resize" : "pointer"; return; }
    if (drag.startsWith("mp")) { const pts = [...state.model.layers[i].multipoints]; pts[+drag.slice(2)] = x; state.model.layers[i].multipoints = pts; drawRowWave(i); }
    else slider(lid(i, drag)).setNormalisedValue(x);
  });
  const end = () => { if (!drag) return; if (drag.startsWith("mp")) call("setMultipoints", i, state.model.layers[i].multipoints); else slider(lid(i, drag)).sliderDragEnded(); drag = null; };
  c.addEventListener("pointerup", end); c.addEventListener("pointercancel", end);
  c.addEventListener("contextmenu", (e) => e.preventDefault());
  c.addEventListener("dblclick", (e) => {
    const L = state.model?.layers?.[i]; if (!L?.loaded) return;
    const pts = [...(L.multipoints || [])]; if (pts.length >= 8) { toast("Max 8 points"); return; }
    pts.push(pos(e)); call("setMultipoints", i, pts);
    if (combo(lid(i, "mpMode")).getChoiceIndex() === 0) combo(lid(i, "mpMode")).setChoiceIndex(3);
  });
}

function selectLayer(i) {
  state.sel = i; call("selectLayer", i);
  rows.forEach((_, k) => renderRow(k));
  $$(".strip").forEach((s, k) => s.classList.toggle("sel", k === i));
  state.renderToolbar?.();
  renderLibrary();
}

// ================================================================ library
const CATS = [["all", "ALL", null], ["fav", "FAVORITES", "#F7C548"], ["DARK", "DARK", "#4E3577"], ["BRIGHT", "BRIGHT", "#FFF1C2"], ["PLUCKY", "PLUCKY", "#FF8A5B"],
  ["SMOOTH", "SMOOTH", "#4FD1A5"], ["MOVING", "MOVING", "#9BE05A"], ["STEADY", "STEADY", "#B9ADA0"]];
function buildLibrary() {
  const sel = $("#libSel");
  for (let i = 0; i < NL; i++) { const b = el("button", "", `L${i + 1}`); b.addEventListener("click", () => selectLayer(i)); sel.appendChild(b); }
}
function renderLibrary() {
  const M = state.model; if (!M) return;
  $$("#libSel button").forEach((b, k) => b.classList.toggle("on", k === state.sel));
  const L = M.layers[state.sel];
  const cats = $("#libCats"); cats.innerHTML = "";
  const rnd = el("button", "cat random", `${ic("dice").replace("ic ", "ic sw-ic ")} RANDOM`);
  rnd.querySelector(".ic").style.cssText = "width:16px;height:16px;display:inline-block";
  rnd.addEventListener("click", () => {
    const items = filtered(L); const pick = items.filter((x) => x.k !== L.index);
    if (pick.length) call("jumpHistory", state.sel, pick[Math.floor(Math.random() * pick.length)].k);
  });
  cats.appendChild(rnd);
  const hist = L.loaded ? L.history.map((h, k) => ({ ...h, k })).filter((x) => x.k > 0) : [];
  CATS.forEach(([key, label, color]) => {
    const n = key === "all" ? hist.length : key === "fav" ? hist.filter((x) => x.fav).length : hist.filter((x) => (x.tags || "").includes(key)).length;
    const c = el("button", "cat" + (state.libCat === key ? " on" : ""), `${color ? `<span class="sw" style="background:${color}"></span>` : ""}${key === "fav" ? "&#9733; " : ""}${label}<span class="n">${n}</span>`);
    c.addEventListener("click", () => { state.libCat = key; renderLibrary(); });
    cats.appendChild(c);
  });
  const items = $("#libItems"); items.innerHTML = "";
  const list = filtered(L);
  $("#libHead").innerHTML = `<span>${CATS.find((c) => c[0] === state.libCat)[1]} · ${list.length} LEVELS</span><span>LAYER ${state.sel + 1}</span>`;
  if (!L.loaded) { items.appendChild(el("div", "lib-empty", "THIS LAYER IS EMPTY.<br>DROP A ONE-SHOT ON IT FIRST.")); return; }
  const orig = el("div", "li orig" + (L.index === 0 ? " cur" : ""), `<span>#0</span><span>ORIGINAL L${state.sel + 1}</span><span class="t"></span><span>${ic("speaker")}</span>`);
  orig.addEventListener("click", () => call("jumpHistory", state.sel, 0));
  items.appendChild(orig);
  if (!list.length) items.appendChild(el("div", "lib-empty", state.libCat === "fav" ? "NO FAVORITES YET. TAP THE STAR NEXT TO A VARIATION." : "PRESS EVOLVE TO GROW SOME VARIATIONS."));
  list.slice().reverse().forEach((x) => {
    const r = el("div", "li" + (x.k === L.index ? " cur" : ""));
    r.innerHTML = `<span>#${x.k}</span><span>${x.name}</span><span class="t">${(x.tags || "").split(" ").slice(0, 2).join(" ")}</span><button class="st">${ic(x.fav ? "star-on" : "star")}</button>`;
    r.title = `seed ${x.seed} · chaos ${x.chaos}%\n${x.label}`;
    r.addEventListener("click", (e) => { if (e.target.closest(".st")) { call("toggleFavorite", state.sel, x.k); return; } call("jumpHistory", state.sel, x.k); });
    items.appendChild(r);
  });
}
function filtered(L) {
  if (!L?.loaded) return [];
  const hist = L.history.map((h, k) => ({ ...h, k })).filter((x) => x.k > 0);
  if (state.libCat === "all") return hist;
  if (state.libCat === "fav") return hist.filter((x) => x.fav);
  return hist.filter((x) => (x.tags || "").includes(state.libCat));
}

// ================================================================ FX / MIX overlays
function onoff(parent, id, unitEl) {
  const b = el("button", "onoff", "OFF");
  const r = () => { const v = !!toggle(id).getValue(); b.classList.toggle("on", v); b.textContent = v ? "ON" : "OFF"; unitEl?.classList.toggle("off", !v); };
  onToggle(id, r); r();
  b.addEventListener("click", () => toggle(id).setValue(!toggle(id).getValue()));
  parent.appendChild(b); return b;
}
function chips(parent, id, labels) {
  const w = el("div", "chips");
  const bs = labels.map((l, k) => { const b = el("button", "", l); b.addEventListener("click", () => combo(id).setChoiceIndex(k)); w.appendChild(b); return b; });
  const r = () => bs.forEach((b, k) => b.classList.toggle("on", combo(id).getChoiceIndex() === k));
  onCombo(id, r); r(); parent.appendChild(w); return w;
}
function unit(parent, title, onId, wide) {
  const u = el("div", "unit" + (wide ? " wide" : ""));
  const h = el("div", "unit-head", `<span>${title}</span>`);
  u.appendChild(h);
  if (onId) onoff(h, onId, u);
  const body = el("div", "unit-body"); u.appendChild(body);
  parent.appendChild(u); return { u, h, body };
}
function buildFx() {
  const g = $("#fxGrid");
  let x = unit(g, "FILTER", "flt_on"); chips(x.h, "flt_type", ["LP", "HP", "BP"]);
  knob(x.body, "flt_cutoff", "CUTOFF", fmt.hz, { compact: true, def: 8000 }); knob(x.body, "flt_res", "RESO", fmt.pct, { compact: true, def: 0.2 }); knob(x.body, "flt_mix", "MIX", fmt.pct, { compact: true, def: 1 });
  x = unit(g, "DRIVE", "dist_on");
  knob(x.body, "dist_drive", "DRIVE", fmt.pct, { compact: true, def: 0.3 }); knob(x.body, "dist_mix", "MIX", fmt.pct, { compact: true, def: 1 });
  x = unit(g, "CHORUS", "cho_on");
  knob(x.body, "cho_rate", "RATE", fmt.hz, { compact: true, def: 0.6 }); knob(x.body, "cho_depth", "DEPTH", fmt.pct, { compact: true, def: 0.35 }); knob(x.body, "cho_mix", "MIX", fmt.pct, { compact: true, def: 0.5 });
  x = unit(g, "DELAY", "dly_on");
  const syncB = el("button", "onoff", "SYNC"); x.h.insertBefore(syncB, x.h.lastChild);
  const timeK = knob(x.body, "dly_time", "TIME", fmt.ms, { compact: true, def: 350 });
  const divWrap = el("div", "kc"); const divV = el("button", "onoff", "1/8"); divWrap.append(divV, el("span", "kl", "NOTE")); x.body.appendChild(divWrap);
  const divs = () => combo("dly_div").properties.choices || [];
  divV.addEventListener("click", () => { const c = combo("dly_div"); c.setChoiceIndex((c.getChoiceIndex() + 1) % Math.max(1, divs().length)); });
  divV.addEventListener("contextmenu", (e) => { e.preventDefault(); const c = combo("dly_div"); const n = Math.max(1, divs().length); c.setChoiceIndex((c.getChoiceIndex() - 1 + n) % n); });
  const rs = () => { const on = !!toggle("dly_sync").getValue(); syncB.classList.toggle("on", on); timeK.style.display = on ? "none" : ""; divWrap.style.display = on ? "" : "none"; divV.textContent = divs()[combo("dly_div").getChoiceIndex()] || "1/8"; };
  onToggle("dly_sync", rs); onCombo("dly_div", rs); rs();
  syncB.addEventListener("click", () => toggle("dly_sync").setValue(!toggle("dly_sync").getValue()));
  knob(x.body, "dly_fb", "FEEDBK", fmt.pct, { compact: true, def: 0.35 }); knob(x.body, "dly_mix", "MIX", fmt.pct, { compact: true, def: 0.25 });
  x = unit(g, "REVERB", "rev_on", true);
  knob(x.body, "rev_size", "SIZE", fmt.pct, { compact: true, def: 0.6 }); knob(x.body, "rev_damp", "DAMP", fmt.pct, { compact: true, def: 0.4 });
  knob(x.body, "rev_width", "WIDTH", fmt.pct, { compact: true, def: 1 }); knob(x.body, "rev_mix", "MIX", fmt.pct, { compact: true, def: 0.25 });
  x = unit(g, "MUTATION POOL (WHAT EVOLVE MAY USE)", null, true);
  const pool = el("div", "proc-pool"); x.body.appendChild(pool); x.body.style.display = "block";
  [["evo_pitch", "PITCH"], ["evo_grain", "GRAINS"], ["evo_filter", "FILTER"], ["evo_sat", "SATURATE"], ["evo_stretch", "STRETCH"], ["evo_ring", "RING MOD"], ["evo_revtail", "REV TAIL"], ["evo_trans", "TRANSIENT"]]
    .forEach(([id, l]) => { const b = el("button", "", l); const r = () => b.classList.toggle("on", !!toggle(id).getValue()); onToggle(id, r); r(); b.addEventListener("click", () => toggle(id).setValue(!toggle(id).getValue())); pool.appendChild(b); });
}
function fader(parent, id) {
  const st = slider(id);
  const f = el("div", "fader", `<div class="tr"></div><div class="fl"></div><div class="cap"></div>`);
  const fl = f.querySelector(".fl"), cap = f.querySelector(".cap"); const v = el("span", "fv");
  const r = () => { const n = st.getNormalisedValue(); fl.style.height = `${n * 100}%`; cap.style.bottom = `${n * 100}%`; v.textContent = fmt.db(st.getScaledValue()); };
  st.valueChangedEvent.addListener(r); st.propertiesChangedEvent.addListener(r);
  let drag = false;
  const set = (e) => { const b = f.getBoundingClientRect(); st.setNormalisedValue(Math.min(1, Math.max(0, 1 - (e.clientY - b.top) / b.height))); };
  f.addEventListener("pointerdown", (e) => { f.setPointerCapture(e.pointerId); drag = true; st.sliderDragStarted(); set(e); });
  f.addEventListener("pointermove", (e) => drag && set(e));
  const end = () => { if (drag) { drag = false; st.sliderDragEnded(); } };
  f.addEventListener("pointerup", end); f.addEventListener("pointercancel", end);
  f.addEventListener("dblclick", () => setScaled(id, 0));
  parent.append(f, v); r();
}
function buildMix() {
  const body = $("#mixBody");
  const strips = el("div", "strips"); body.appendChild(strips);
  for (let i = 0; i < NL; i++) {
    const s = el("div", "strip" + (i === state.sel ? " sel" : ""));
    s.appendChild(el("span", "sn", `L${i + 1}`));
    fader(s, lid(i, "gain"));
    knob(s, lid(i, "pan"), "PAN", fmt.pan, { compact: true, small: true, def: 0 });
    knob(s, lid(i, "cents"), "FINE", fmt.ct, { compact: true, small: true, def: 0 });
    const ms = el("div", "ms");
    [["mute", "m", "M"], ["solo", "s", "S"]].forEach(([id, c, t]) => { const b = el("button", c, t); const r = () => b.classList.toggle("on", !!toggle(lid(i, id)).getValue()); onToggle(lid(i, id), r); r(); b.addEventListener("click", () => toggle(lid(i, id)).setValue(!toggle(lid(i, id)).getValue())); ms.appendChild(b); });
    s.appendChild(ms);
    s.addEventListener("pointerdown", () => selectLayer(i));
    strips.appendChild(s);
  }
  const low = el("div", "mix-low"); body.appendChild(low);
  let x = unit(low, "AMP ENVELOPE", null);
  knob(x.body, "amp_attack", "ATTACK", fmt.ms, { compact: true, def: 1 }); knob(x.body, "amp_decay", "DECAY", fmt.ms, { compact: true, def: 400 });
  knob(x.body, "amp_sustain", "SUSTAIN", fmt.pct, { compact: true, def: 1 }); knob(x.body, "amp_release", "RELEASE", fmt.ms, { compact: true, def: 250 });
  knob(x.body, "vel_sens", "VELO", fmt.pct, { compact: true, def: 0.8 });
  x = unit(low, "OUTPUT", "limiter_on");
  x.h.firstChild.textContent = "OUT · LIMIT";
  knob(x.body, "master_gain", "MASTER", fmt.db, { compact: true, def: -3 }); knob(x.body, "limiter_ceiling", "CEILING", fmt.db, { compact: true, def: -0.3 });
  const gr = el("div", "gr", "<div id='grBar'></div>"); x.u.appendChild(gr);
  const loopU = unit(body, "SELECTED LAYER LOOP", null, true);
  state.loopUnit = loopU;
}
function buildLoopUnit() {
  const u = state.loopUnit; if (!u) return;
  u.h.firstChild.textContent = `L${state.sel + 1} · LOOP & REGION`;
  u.body.innerHTML = "";
  const i = state.sel;
  knob(u.body, lid(i, "start"), "START", (v) => `${Math.round(v * 100)}%`, { compact: true, def: 0 });
  knob(u.body, lid(i, "end"), "END", (v) => `${Math.round(v * 100)}%`, { compact: true, def: 1 });
  knob(u.body, lid(i, "loopStart"), "LOOP IN", (v) => `${Math.round(v * 100)}%`, { compact: true, def: 0.2 });
  knob(u.body, lid(i, "loopEnd"), "LOOP OUT", (v) => `${Math.round(v * 100)}%`, { compact: true, def: 0.8 });
  knob(u.body, lid(i, "xfade"), "X-FADE", fmt.ms, { compact: true, def: 15 });
  knob(u.body, lid(i, "semi"), "SEMI", fmt.st, { compact: true, def: 0 });
}

// ================================================================ presets
async function refreshPresets() {
  let list; try { list = await call("listPresets"); } catch { return; }
  if (!Array.isArray(list)) return;
  state.presets = list;
  const cur = state.model?.preset || (await call("getCurrentPreset")) || "";
  $("#presetName").textContent = (cur || "UNTITLED").toUpperCase();
  const box = $("#presetList"); box.innerHTML = "";
  let sec = null;
  list.forEach((p) => {
    const s = p.factory ? "FACTORY" : "YOUR PRESETS";
    if (s !== sec) { box.appendChild(el("div", "pl-sec", s)); sec = s; }
    const b = el("button", "pl" + (p.name === cur ? " cur" : ""), p.name.toUpperCase());
    b.addEventListener("click", () => loadPreset(p));
    box.appendChild(b);
  });
}
async function loadPreset(p) {
  const r = await call("loadPreset", p.id);
  if (!r?.ok) toast(r?.error || "Could not load preset"); else { closeOverlays(); say(`HELLO! I'M ${p.name.toUpperCase()}.`, 2400); }
  refreshPresets();
}
function stepPreset(d) {
  const list = state.presets || []; if (!list.length) return;
  const cur = state.model?.preset || "";
  const k = list.findIndex((p) => p.name === cur);
  loadPreset(list[((k < 0 ? (d > 0 ? -1 : 0) : k) + d + list.length) % list.length]);
}

// ================================================================ overlays & views
function closeOverlays() { $$(".overlay").forEach((o) => o.classList.remove("on")); $("#btnFx").classList.remove("lit"); $("#btnMix").classList.remove("lit"); }
function toggleOverlay(id, btn) {
  const o = $(id), was = o.classList.contains("on");
  closeOverlays();
  if (!was) { o.classList.add("on"); btn?.classList.add("lit"); if (id === "#mixPanel") buildLoopUnit(); }
}
function setView(v) {
  state.view = v;
  $$("#mainTabs button").forEach((b) => b.classList.toggle("on", b.dataset.view === v));
  $$(".view").forEach((x) => x.classList.toggle("on", x.dataset.view === v));
  closeOverlays();
  if (v === "shots") requestAnimationFrame(resizeWaves);
}
function openSub(s) {
  setView("shots"); state.sub = s;
  $$(".paper-tabs button").forEach((b) => b.classList.toggle("on", b.dataset.sub === s));
  $$(".sub").forEach((x) => x.classList.toggle("on", x.dataset.sub === s));
  if (s === "generated") requestAnimationFrame(resizeWaves); else renderLibrary();
}

// ================================================================ drag & drop
let dropLayer = 0, dragDepth = 0;
const AUDIO_RE = /\.(wav|wave|aif|aiff|flac|mp3|ogg)$/i;
function b64(buf) { const u = new Uint8Array(buf); let s = ""; for (let i = 0; i < u.length; i += 0x8000) s += String.fromCharCode.apply(null, u.subarray(i, i + 0x8000)); return btoa(s); }
async function loadFileInto(layer, file) {
  if (!AUDIO_RE.test(file.name)) { toast(`Unsupported: ${file.name}`); return; }
  if (file.size > 200 * 1024 * 1024) { toast("File too large"); return; }
  const r = await call("loadSampleData", layer, file.name, b64(await file.arrayBuffer()));
  if (!r?.ok) toast(r?.error || "Could not load file"); else if (state.view === "creature") say("YUM! TASTY SOUND.", 2000);
}
function firstEmpty() { const i = state.model?.layers?.findIndex((l) => !l.loaded) ?? -1; return i < 0 ? state.sel : i; }
function setupDrop() {
  const hint = $("#dropHint");
  window.addEventListener("dragenter", (e) => { e.preventDefault(); if (++dragDepth === 1) { dropLayer = state.view === "creature" ? (state.target >= 0 ? state.target : firstEmpty()) : state.sel; $("#dropTarget").textContent = `LAYER ${dropLayer + 1}`; hint.classList.add("on"); } });
  window.addEventListener("dragover", (e) => { e.preventDefault(); if (e.dataTransfer) e.dataTransfer.dropEffect = "copy"; });
  window.addEventListener("dragleave", (e) => { e.preventDefault(); dragDepth = Math.max(0, dragDepth - 1); if (!dragDepth) { hint.classList.remove("on"); rows.forEach((r) => r.root.classList.remove("drag")); } });
  window.addEventListener("drop", async (e) => {
    e.preventDefault(); dragDepth = 0; hint.classList.remove("on"); rows.forEach((r) => r.root.classList.remove("drag"));
    const files = [...(e.dataTransfer?.files || [])]; let layer = dropLayer;
    for (const f of files.slice(0, NL)) { await loadFileInto(layer, f); layer = (layer + 1) % NL; }
    selectLayer(dropLayer);
  });
}

// ================================================================ keyboard
const KEY_MAP = "awsedftgyhujkolp;'";
function buildKeys() {
  const k = $("#keys"); const keyEls = {};
  for (let n = 48; n <= 72; n++) { const d = el("div", "k" + ([1, 3, 6, 8, 10].includes(n % 12) ? " b" : "") + (n === 60 ? " root" : "")); d.dataset.n = n; k.appendChild(d); keyEls[n] = d; }
  const on = (n) => { call("noteOn", n, 0.85); keyEls[n]?.classList.add("down"); };
  const off = (n) => { call("noteOff", n); keyEls[n]?.classList.remove("down"); };
  let down = null;
  k.addEventListener("pointerdown", (e) => { const n = +e.target.dataset.n; if (!n) return; k.setPointerCapture(e.pointerId); down = n; on(n); });
  k.addEventListener("pointermove", (e) => { if (down == null) return; const t = document.elementFromPoint(e.clientX, e.clientY); const n = t && +t.dataset.n; if (n && n !== down) { off(down); down = n; on(n); } });
  const up = () => { if (down != null) { off(down); down = null; } };
  k.addEventListener("pointerup", up); k.addEventListener("pointercancel", up);
  const held = new Map();
  window.addEventListener("keydown", (e) => {
    if (e.target.tagName === "INPUT" || e.repeat || e.ctrlKey || e.metaKey) return;
    const key = e.key.toLowerCase();
    if (key === "z") { state.octave = Math.max(-3, state.octave - 1); toast(`OCTAVE ${state.octave}`); return; }
    if (key === "x") { state.octave = Math.min(3, state.octave + 1); toast(`OCTAVE ${state.octave}`); return; }
    if (key === " ") { e.preventDefault(); doEvolve(); return; }
    const idx = KEY_MAP.indexOf(key); if (idx < 0) return;
    const n = 60 + idx + state.octave * 12; held.set(key, n); on(n);
  });
  window.addEventListener("keyup", (e) => { const key = e.key.toLowerCase(); if (held.has(key)) { off(held.get(key)); held.delete(key); } });
}

// ================================================================ model
async function fetchPeaks(i) {
  const L = state.model?.layers?.[i];
  if (!L?.loaded) { delete state.peaks[i]; return; }
  if (state.peaks[i]?.rev === L.revision) return;
  state.peaks[i] = { rev: L.revision, mini: await call("getPeaks", i, 400) };
}
async function onModel(m) {
  const prevSeed = state.model?.creatureSeed;
  state.model = m;
  if (m.creatureSeed !== prevSeed) traits = null;
  if (state.lastLevel === undefined || state.lastLevel === null) state.lastLevel = m.level;
  await Promise.all(m.layers.map((_, i) => fetchPeaks(i)));
  if (state.target >= 0 && !m.layers[state.target]?.loaded) state.target = -1;
  $("#presetName").textContent = (m.preset || "UNTITLED").toUpperCase();
  updateCreatureView();
  rows.forEach((_, i) => renderRow(i));
  state.renderToolbar?.();
  renderLibrary();
  const errs = m.layers.map((l) => l.error).filter(Boolean); if (errs.length) toast(errs[0]);
}

// ================================================================ loop
let lastFrame = 0, lastMeter = 0;
function tick(t) {
  requestAnimationFrame(tick);
  if (t - lastFrame >= 1000 / 15) { lastFrame = t; scene.t = t / 1000; drawScene(scene.t); }
  render3D(t / 1000);
  if (t - lastMeter > 50) {
    lastMeter = t;
    call("getMeters").then((m) => {
      const was = (state.meters?.voices || 0) > 0;
      state.meters = m;
      const gr = $("#grBar"); if (gr) gr.style.width = `${Math.min(100, (1 - m.limiter) * 400)}%`;
      if (state.view === "shots" && (was || m.voices > 0)) rows.forEach((_, i) => drawRowWave(i));
    }).catch(() => {});
  }
}

// ================================================================ boot
window.addEventListener("error", (e) => { try { call("log", `${e.message} @${e.lineno}`); } catch {} });
window.addEventListener("unhandledrejection", (e) => { try { call("log", `rejection: ${e.reason}`); } catch {} });

function boot() {
  installIconCss();
  scene.ctx = $("#scene").getContext("2d");
  applySkin("pink", false);
  try { c3d = new Creature3D($("#creature3d"), 252, 271); } catch (e) { c3d = null; call("log", "3D unavailable: " + e); }
  if (!c3d?.ok) $("#creature3d").style.display = "none";
  call("getSkin").then((sk) => { if (sk) applySkin(sk, false); }).catch(() => {});
  $("#btnSkin").addEventListener("click", () => { applySkin(SKIN_ORDER[(SKIN_ORDER.indexOf(state.skin) + 1) % SKIN_ORDER.length]); traits = null; });
  knob($("#kChaos"), "chaos", "CHAOS", fmt.pct100, { def: 40 });
  knob($("#kGlide"), "glide", "GLIDE", fmt.ms, { def: 0 });
  knob($("#kVibrato"), "vibrato", "VIBRATO", fmt.pct, { def: 0 });
  knob($("#kChaos2"), "chaos", "CHAOS", fmt.pct100, { def: 40, horizontal: true, small: true });
  knob($("#kGlide2"), "glide", "GLIDE", fmt.ms, { def: 0, horizontal: true, small: true });
  knob($("#kVibrato2"), "vibrato", "VIBRATO", fmt.pct, { def: 0, horizontal: true, small: true });
  // the wrapper ctl divs from knob() are nested inside the placeholders; unwrap
  ["#kChaos", "#kGlide", "#kVibrato", "#kChaos2", "#kGlide2", "#kVibrato2"].forEach((s) => { const p = $(s); const c = p.firstChild; p.replaceWith(c); });
  buildToolbar(); buildRows(); buildLibrary(); buildFx(); buildMix(); buildKeys(); setupDrop();

  $$("#mainTabs button").forEach((b) => b.addEventListener("click", () => setView(b.dataset.view)));
  $$(".paper-tabs button").forEach((b) => b.addEventListener("click", () => openSub(b.dataset.sub)));
  $("#btnEvolve").addEventListener("click", doEvolve);
  $("#btnFx").addEventListener("click", () => toggleOverlay("#fxPanel", $("#btnFx")));
  $("#btnMix").addEventListener("click", () => toggleOverlay("#mixPanel", $("#btnMix")));
  $$("[data-close]").forEach((b) => b.addEventListener("click", closeOverlays));
  $("#layerBox").addEventListener("click", cycleTarget); $("#layerBox2").addEventListener("click", cycleTarget);
  $("#lvPrev").addEventListener("click", () => call("stepLevel", -1));
  $("#lvNext").addEventListener("click", () => call("stepLevel", 1));
  $("#lvLock").addEventListener("click", () => {
    const tgt = targetLayers().filter((i) => state.model.layers[i].loaded); if (!tgt.length) return;
    const lock = !tgt.every((i) => state.model.layers[i].locked);
    tgt.forEach((i) => call("setLocked", i, lock)); toast(lock ? "Locked" : "Unlocked");
  });
  $("#lvStar").addEventListener("click", () => {
    targetLayers().forEach((i) => { const L = state.model.layers[i]; if (L.loaded && L.index > 0) call("toggleFavorite", i, L.index); });
  });
  $("#btnDice").addEventListener("click", async () => {
    if (!state.model?.layers?.some((l) => l.loaded)) { say("NOTHING TO ROLL. FEED ME A SOUND!", 2200); return; }
    await call("rollDice"); say("ROLLING THE DICE...", 1600);
  });
  $("#btnUndoDice").addEventListener("click", () => call("undoDice"));
  $("#btnPresets").addEventListener("click", () => { refreshPresets(); toggleOverlay("#presetPanel"); });
  $("#btnSave").addEventListener("click", () => { toggleOverlay("#savePanel"); const n = $("#saveName"); n.value = state.model?.preset || ""; setTimeout(() => n.focus(), 30); });
  $("#saveGo").addEventListener("click", async () => {
    const name = $("#saveName").value.trim(); if (!name) { toast("Enter a name"); return; }
    const r = await call("savePreset", name);
    if (r?.ok) { closeOverlays(); toast(`Saved ${name}`); refreshPresets(); } else toast(r?.error || "Save failed");
  });
  $("#saveName").addEventListener("keydown", (e) => { if (e.key === "Enter") $("#saveGo").click(); });
  $("#presetPrev").addEventListener("click", () => stepPreset(-1));
  $("#presetNext").addEventListener("click", () => stepPreset(1));
  // creature: click = play / browse, drag = spin it around
  let dragX = null, moved = false;
  const dc = $("#dropCatch");
  dc.addEventListener("pointerdown", (e) => { dc.setPointerCapture(e.pointerId); dragX = e.clientX; moved = false; });
  dc.addEventListener("pointermove", (e) => { if (dragX == null) return; const dx = e.clientX - dragX; if (Math.abs(dx) > 3) moved = true; if (c3d?.ok) c3d.nudge(dx / state.scale); dragX = e.clientX; });
  dc.addEventListener("pointerup", () => {
    dragX = null; if (moved) return;
    if (!state.model?.layers?.some((l) => l.loaded)) call("browseSample", firstEmpty());
    else { call("noteOn", 60, 0.9); setTimeout(() => call("noteOff", 60), 400); scene.bounce = 1; }
  });

  window.__JUCE__.backend.addEventListener("modelChanged", onModel);
  window.__JUCE__.backend.addEventListener("notify", (msg) => toast(String(msg)));
  window.addEventListener("resize", fit);
  fit();
  selectLayer(0);
  call("getModel").then(onModel);
  refreshPresets();
  requestAnimationFrame(tick);
}
boot();
