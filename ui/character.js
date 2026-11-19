// MUTAFORM default mascot "POPSY" — an original pop-art character (not based on any real person).
// Drawn as layered SVG so it can blink, sing and move its arms. A user character pack
// (PNG layers) can replace every layer; see CharacterView.loadPack().

const OUT = 'stroke="#141014" stroke-width="5" stroke-linejoin="round" stroke-linecap="round"';

const BASE = `
<defs>
  <pattern id="dots" width="7" height="7" patternUnits="userSpaceOnUse"><circle cx="3.5" cy="3.5" r="1.6" fill="#E0559E"/></pattern>
  <pattern id="dotsY" width="8" height="8" patternUnits="userSpaceOnUse"><circle cx="4" cy="4" r="1.8" fill="#F0A800"/></pattern>
  <clipPath id="faceClip"><ellipse cx="150" cy="200" rx="74" ry="88"/></clipPath>
</defs>
<!-- jacket -->
<path d="M40 400 C 46 330, 90 300, 150 300 C 210 300, 254 330, 260 400 Z" fill="#141014" ${OUT}/>
<path d="M118 300 L150 352 L182 300 L204 312 L150 380 L96 312 Z" fill="#B8F400" ${OUT}/>
<!-- neck -->
<path d="M126 262 L126 306 Q150 320 174 306 L174 262 Z" fill="#FF9ED2" ${OUT}/>
<!-- back hair volume -->
<path d="M64 210 C 40 150, 60 70, 150 64 C 240 70, 262 150, 236 214 C 250 250, 226 282, 206 262 L 94 262 C 74 282, 50 250, 64 210 Z" fill="#FFE600" ${OUT}/>
<!-- ears -->
<ellipse cx="78" cy="212" rx="12" ry="18" fill="#FF9ED2" ${OUT}/>
<ellipse cx="222" cy="212" rx="12" ry="18" fill="#FF9ED2" ${OUT}/>
<circle cx="78" cy="232" r="5" fill="#2BB8FF" ${OUT.replace('5"', '3"')}/>
<circle cx="222" cy="232" r="5" fill="#2BB8FF" ${OUT.replace('5"', '3"')}/>
<!-- face -->
<ellipse cx="150" cy="200" rx="74" ry="88" fill="#FF9ED2" ${OUT}/>
<g clip-path="url(#faceClip)"><rect x="168" y="110" width="70" height="190" fill="url(#dots)" opacity=".8"/></g>
<!-- cheeks -->
<ellipse cx="104" cy="236" rx="14" ry="8" fill="#FF5FB0" opacity=".8"/>
<ellipse cx="196" cy="236" rx="14" ry="8" fill="#FF5FB0" opacity=".8"/>
<!-- quiff: a tall swirl on top (front hair) -->
<path d="M86 150 C 70 96, 104 40, 156 30 C 200 22, 236 52, 222 92 C 212 120, 178 118, 176 98 C 174 82, 192 78, 196 88
         C 200 66, 172 58, 156 72 C 138 88, 150 126, 186 140 C 150 150, 112 140, 98 160 Z" fill="#FFE600" ${OUT}/>
<path d="M98 150 C 92 112, 116 72, 150 66" fill="none" stroke="#F0A800" stroke-width="6" stroke-linecap="round"/>
<path d="M96 120 C 104 96, 124 86, 140 84" fill="none" stroke="#FFF6A0" stroke-width="5" stroke-linecap="round"/>
<g opacity=".9"><path d="M182 134 C 196 120, 216 112, 230 96 L 236 140 C 220 150, 200 150, 182 134 Z" fill="url(#dotsY)"/></g>
<!-- brows -->
<path d="M100 166 Q 118 152 138 162" fill="none" ${OUT}/>
<path d="M162 162 Q 182 152 200 166" fill="none" ${OUT}/>
<!-- nose -->
<path d="M150 204 Q 144 222 152 226" fill="none" stroke="#141014" stroke-width="4" stroke-linecap="round"/>
<!-- beauty star -->
<path d="M190 254 l3 6 7 1 -5 5 1 7 -6 -3 -6 3 1 -7 -5 -5 7 -1 z" fill="#141014"/>
`;

const EYES_OPEN = `
<path d="M100 192 Q 120 172 140 192 Q 120 206 100 192 Z" fill="#fff" ${OUT.replace('5"', '4"')}/>
<path d="M160 192 Q 180 172 200 192 Q 180 206 160 192 Z" fill="#fff" ${OUT.replace('5"', '4"')}/>
<circle class="iris" cx="121" cy="191" r="8" fill="#141014"/>
<circle class="iris" cx="181" cy="191" r="8" fill="#141014"/>
<circle cx="124" cy="188" r="2.6" fill="#fff"/><circle cx="184" cy="188" r="2.6" fill="#fff"/>
<path d="M98 190 Q 120 160 142 190 Q 120 176 98 190 Z" fill="#2BB8FF" ${OUT.replace('5"', '3"')}/>
<path d="M158 190 Q 180 160 202 190 Q 180 176 158 190 Z" fill="#2BB8FF" ${OUT.replace('5"', '3"')}/>
<path d="M98 190 l-8 -6 M104 182 l-6 -8 M196 182 l6 -8 M202 190 l8 -6" stroke="#141014" stroke-width="4" stroke-linecap="round"/>
`;
const EYES_CLOSED = `
<path d="M98 190 Q 120 178 142 190 Q 120 174 98 190 Z" fill="#2BB8FF" ${OUT.replace('5"', '3"')}/>
<path d="M158 190 Q 180 178 202 190 Q 180 174 158 190 Z" fill="#2BB8FF" ${OUT.replace('5"', '3"')}/>
<path d="M100 192 Q 120 202 140 192 M160 192 Q 180 202 200 192" fill="none" ${OUT.replace('5"', '4"')}/>
<path d="M98 192 l-8 4 M202 192 l8 4" stroke="#141014" stroke-width="4" stroke-linecap="round"/>
`;
const MOUTH = {
  closed: `<path d="M124 250 Q 137 240 150 247 Q 163 240 176 250 Q 163 266 150 266 Q 137 266 124 250 Z" fill="#E8102A" ${OUT.replace('5"', '4"')}/>
           <path d="M126 250 Q 150 256 174 250" fill="none" stroke="#7A0010" stroke-width="3"/><ellipse cx="160" cy="258" rx="6" ry="2.5" fill="#fff" opacity=".7"/>`,
  open: `<path d="M122 248 Q 136 236 150 244 Q 164 236 178 248 Q 170 276 150 278 Q 130 276 122 248 Z" fill="#E8102A" ${OUT.replace('5"', '4"')}/>
         <path d="M132 252 Q 150 258 168 252 Q 162 268 150 268 Q 138 268 132 252 Z" fill="#3A0010"/><path d="M134 252 Q 150 256 166 252 L164 256 Q 150 259 136 256 Z" fill="#fff"/>`,
  wide: `<path d="M118 246 Q 134 230 150 240 Q 166 230 182 246 Q 176 290 150 292 Q 124 290 118 246 Z" fill="#E8102A" ${OUT.replace('5"', '4"')}/>
         <ellipse cx="150" cy="264" rx="20" ry="18" fill="#3A0010"/><path d="M134 252 Q 150 256 166 252 L163 257 Q 150 260 137 257 Z" fill="#fff"/><ellipse cx="150" cy="276" rx="10" ry="5" fill="#FF5F7A"/>`,
};
// arms drawn pointing straight down from the shoulder pivot; rotated by JS
const ARM = (side) => {
  const x = side < 0 ? 84 : 216;
  return `<g class="arm" data-px="${x}" data-py="318">
    <path d="M${x - 16} 318 L${x - 13} 400 Q ${x} 410 ${x + 13} 400 L${x + 16} 318 Z" fill="#1E7BFF" ${OUT}/>
    <path d="M${x - 6} 330 L${x - 5} 372" stroke="#8FC4FF" stroke-width="5" stroke-linecap="round"/>
    <path d="M${x - 13} 384 L ${x + 13} 384" stroke="#B8F400" stroke-width="6"/>
    <circle cx="${x}" cy="412" r="15" fill="#FF9ED2" ${OUT}/>
    <path d="M${x - 8} 404 l-6 -10 M${x} 400 l0 -12 M${x + 8} 404 l6 -10" stroke="#141014" stroke-width="4" stroke-linecap="round"/>
  </g>`;
};
export const ARM_LEN = 94; // shoulder -> hand centre (design units)
export const SHOULDERS = [[84, 318], [216, 318]];

export class CharacterView {
  constructor(host) {
    this.host = host;
    this.pack = null;
    this.state = { armL: 20, armR: -20, blink: false, mouth: "closed", bob: 0, tilt: 0 };
    this.renderDefault();
  }

  renderDefault() {
    this.pack = null;
    this.host.innerHTML = `<svg viewBox="0 0 300 420" preserveAspectRatio="xMidYMax meet" overflow="visible">
      <g class="armL">${ARM(-1)}</g><g class="armR">${ARM(1)}</g>
      <g class="headGroup"><g>${BASE}</g><g class="eyesOpen">${EYES_OPEN}</g><g class="eyesClosed" style="display:none">${EYES_CLOSED}</g>
      <g class="m-closed">${MOUTH.closed}</g><g class="m-open" style="display:none">${MOUTH.open}</g><g class="m-wide" style="display:none">${MOUTH.wide}</g></g>
    </svg>`;
    const q = (s) => this.host.querySelector(s);
    this.el = { armL: q(".armL"), armR: q(".armR"), head: q(".headGroup"), eo: q(".eyesOpen"), ec: q(".eyesClosed"),
      mc: q(".m-closed"), mo: q(".m-open"), mw: q(".m-wide") };
    this.apply(true);
  }

  /** pack = { base, eyes_closed, mouth_open, mouth_wide, arm_left, arm_right } data-URLs */
  loadPack(pack) {
    if (!pack?.base) { this.renderDefault(); return; }
    this.pack = pack;
    const img = (k, cls, hidden) => pack[k] ? `<img class="${cls}" src="${pack[k]}" style="${hidden ? "display:none" : ""}">` : "";
    this.host.innerHTML = `<div class="armL pk">${img("arm_left", "")}</div><div class="armR pk">${img("arm_right", "")}</div>
      <div class="headGroup pk">${img("base", "")}${img("eyes_closed", "ec", true)}${img("mouth_open", "mo", true)}${img("mouth_wide", "mw", true)}</div>`;
    this.host.querySelectorAll(".pk").forEach((d) => { d.style.position = "absolute"; d.style.inset = "0"; d.style.transformOrigin = "50% 50%"; });
    const q = (s) => this.host.querySelector(s);
    this.el = { armL: q(".armL"), armR: q(".armR"), head: q(".headGroup"), eo: null, ec: q(".ec"), mc: null, mo: q(".mo"), mw: q(".mw") };
    // pivots for packs: shoulders at 28 % / 72 % width, 76 % height
    this.el.armL.style.transformOrigin = "28% 76%";
    this.el.armR.style.transformOrigin = "72% 76%";
    this.apply(true);
  }

  set(st) { Object.assign(this.state, st); this.apply(); }

  apply() {
    const s = this.state, e = this.el; if (!e) return;
    if (this.pack) {
      e.armL.style.transform = `rotate(${s.armL}deg)`;
      e.armR.style.transform = `rotate(${s.armR}deg)`;
    } else {
      e.armL.setAttribute("transform", `rotate(${s.armL} ${SHOULDERS[0][0]} ${SHOULDERS[0][1]})`);
      e.armR.setAttribute("transform", `rotate(${s.armR} ${SHOULDERS[1][0]} ${SHOULDERS[1][1]})`);
    }
    const head = `translate(0 ${s.bob.toFixed(2)}) rotate(${s.tilt.toFixed(2)} 150 300)`;
    if (this.pack) e.head.style.transform = `translateY(${s.bob}px) rotate(${s.tilt}deg)`; else e.head.setAttribute("transform", head);
    if (e.eo) e.eo.style.display = s.blink ? "none" : "";
    if (e.ec) e.ec.style.display = s.blink ? "" : "none";
    if (e.mc) e.mc.style.display = s.mouth === "closed" ? "" : "none";
    if (e.mo) e.mo.style.display = s.mouth === "open" ? "" : "none";
    if (e.mw) e.mw.style.display = s.mouth === "wide" ? "" : "none";
  }

  /** Hand positions in design units (300 x 420 box) for the current arm angles. */
  hands() {
    const rot = (p, a) => { const r = a * Math.PI / 180; return [p[0] - Math.sin(r) * ARM_LEN, p[1] + Math.cos(r) * ARM_LEN]; };
    return [rot(SHOULDERS[0], this.state.armL), rot(SHOULDERS[1], this.state.armR)];
  }
}
