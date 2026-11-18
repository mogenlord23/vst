// MUTAFORM 3D mascot — original pop-art blob, built procedurally with three.js.
// Toon shading + black "comic" outline (inverted hull), rendered at low resolution and
// upscaled with nearest-neighbour for a chunky retro-3D look.
import * as THREE from "./three.module.min.js";

const POP = {
  pink: 0xff1fa8, lime: 0xb8f400, blue: 0x1e7bff, orange: 0xff8a00, yellow: 0xffe600,
  red: 0xe8102a, cyan: 0x5fd3ff, black: 0x141014, white: 0xffffff, cream: 0xfff3e0, violet: 0x9b5cff,
};

function rng(seed) {
  let a = seed | 0;
  return () => { a = (a + 0x6d2b79f5) | 0; let t = Math.imul(a ^ (a >>> 15), 1 | a); t = (t + Math.imul(t ^ (t >>> 7), 61 | t)) ^ t; return ((t ^ (t >>> 14)) >>> 0) / 4294967296; };
}

function toonGradient() {
  const data = new Uint8Array([70, 70, 70, 255, 160, 160, 160, 255, 255, 255, 255, 255]);
  const tex = new THREE.DataTexture(data, 3, 1, THREE.RGBAFormat);
  tex.minFilter = THREE.NearestFilter; tex.magFilter = THREE.NearestFilter; tex.needsUpdate = true;
  return tex;
}

export class Creature3D {
  constructor(canvas, width, height) {
    this.ok = false;
    try {
      this.renderer = new THREE.WebGLRenderer({ canvas, alpha: true, antialias: false, preserveDrawingBuffer: false });
    } catch (e) { return; }
    if (!this.renderer.getContext()) return;
    this.renderer.setPixelRatio(1);
    this.renderer.setSize(width, height, false);
    this.renderer.setClearColor(0x000000, 0);
    this.scene = new THREE.Scene();
    this.camera = new THREE.PerspectiveCamera(30, width / height, 0.1, 100);
    this.camera.position.set(0, 1.0, 9.2);
    this.camera.lookAt(0, 1.32, 0);
    this.grad = toonGradient();
    const key = new THREE.DirectionalLight(0xffffff, 2.4); key.position.set(-3, 5, 4); this.scene.add(key);
    const rim = new THREE.DirectionalLight(0xffe0f4, 1.2); rim.position.set(4, 2, -3); this.scene.add(rim);
    this.scene.add(new THREE.AmbientLight(0xffffff, 0.75));
    this.root = new THREE.Group(); this.scene.add(this.root);
    // contact shadow
    const sh = new THREE.Mesh(new THREE.CircleGeometry(1, 32), new THREE.MeshBasicMaterial({ color: 0x000000, transparent: true, opacity: 0.28 }));
    sh.rotation.x = -Math.PI / 2; sh.position.y = 0.01; this.shadow = sh; this.scene.add(sh);
    this.spin = 0; this.spinVel = 0; this.userYaw = 0; this.bounce = 0; this.pop = 0; this.key = "";
    this.ok = true;
  }

  // ---------------------------------------------------------------- materials
  toon(color) { return new THREE.MeshToonMaterial({ color, gradientMap: this.grad }); }
  gloss(color) { return new THREE.MeshPhongMaterial({ color, shininess: 120, specular: 0xffffff }); }
  addOutlined(parent, geo, mat, outline = 0.045) {
    const m = new THREE.Mesh(geo, mat); parent.add(m);
    if (outline > 0) {
      const o = new THREE.Mesh(geo, new THREE.MeshBasicMaterial({ color: POP.black, side: THREE.BackSide }));
      o.scale.setScalar(1 + outline); m.add(o);
    }
    return m;
  }

  // ---------------------------------------------------------------- build
  build(traits, stage) {
    const key = `${traits.seed}|${stage}|${traits.colorKey}`;
    if (key === this.key) return;
    this.key = key;
    this.pop = 1;
    this.spinVel = 9; // celebrate a new form with a spin
    while (this.root.children.length) {
      const c = this.root.children.pop();
      c.traverse((n) => { n.geometry?.dispose(); if (n.material) [].concat(n.material).forEach((m) => m.dispose()); });
    }
    const r = rng(traits.seed);
    const g = new THREE.Group(); this.root.add(g); this.body = g;

    if (stage === 0) { this.buildPod(g, traits, r); return; }

    const size = [0.6, 0.62, 0.72, 0.82, 0.92, 1.0, 1.06, 1.12][stage];
    const sx = size * (0.9 + r() * 0.3), sy = size * (0.85 + r() * 0.45), sz = size * (0.85 + r() * 0.2);
    // body: sphere with seeded low-frequency displacement (lumpy, organic)
    const geo = new THREE.SphereGeometry(1, 48, 32);
    const pos = geo.attributes.position, v = new THREE.Vector3();
    const f1 = 1.5 + r() * 2, f2 = 2 + r() * 3, amp = 0.05 + traits.motion * 0.08, ph = r() * 6.28;
    const pear = traits.body === 2 ? 0.25 : 0;
    for (let i = 0; i < pos.count; i++) {
      v.fromBufferAttribute(pos, i);
      const n = Math.sin(v.x * f1 + ph) * Math.sin(v.y * f2 + ph * 0.7) * Math.sin(v.z * f1 + 1.3);
      const widen = 1 + pear * (-v.y);
      v.set(v.x * widen, v.y, v.z * widen).multiplyScalar(1 + n * amp);
      pos.setXYZ(i, v.x, v.y, v.z);
    }
    geo.computeVertexNormals();
    const bodyColor = traits.bodyColor, accent = traits.accentColor;
    const body = this.addOutlined(g, geo, this.toon(bodyColor));
    body.scale.set(sx, sy, sz); body.position.y = sy + (stage >= 2 ? 0.22 : 0.02);
    this.bodyMesh = body;
    const top = body.position.y + sy, cy = body.position.y, front = sz;

    // polka-dot / stripe pattern (pop-art)
    if (stage >= 3) {
      const dotMat = this.toon(accent);
      const n = 6 + Math.floor(r() * 6);
      for (let i = 0; i < n; i++) {
        const a = r() * Math.PI * 2, b = (r() - 0.5) * 1.6;
        const dir = new THREE.Vector3(Math.cos(a) * Math.cos(b), Math.sin(b), Math.sin(a) * Math.cos(b));
        if (dir.z > 0.55 && Math.abs(dir.y) < 0.5) continue; // keep the face clear
        const d = new THREE.Mesh(new THREE.SphereGeometry(0.12 + r() * 0.06, 12, 8), dotMat);
        d.scale.set(1, 1, 0.3);
        d.position.set(dir.x * sx, cy + dir.y * sy, dir.z * sz);
        d.lookAt(d.position.clone().multiplyScalar(2).setY(cy + dir.y * sy * 2));
        g.add(d);
      }
    }
    // feet
    if (stage >= 2) {
      const footMat = this.toon(traits.footColor);
      [-1, 1].forEach((s) => {
        const ft = this.addOutlined(g, new THREE.SphereGeometry(0.22, 16, 10), footMat);
        ft.scale.set(1.15, 0.55, 1.4); ft.position.set(s * sx * 0.45, 0.13, 0.12);
        (s < 0 ? (this.footL = ft) : (this.footR = ft));
      });
    }
    // arms
    if (stage >= 3) {
      const armMat = this.toon(bodyColor);
      [-1, 1].forEach((s) => {
        const arm = this.addOutlined(g, new THREE.CapsuleGeometry(0.11, 0.32, 4, 10), armMat);
        arm.position.set(s * (sx + 0.05), cy - 0.05, 0.1); arm.rotation.z = s * 0.9;
        (s < 0 ? (this.armL = arm) : (this.armR = arm));
      });
    }
    // eyes with pop eyeshadow lids
    const eyeCount = traits.eyes;
    const eyeY = cy + sy * 0.22, eyeSep = sx * 0.36, eyeR = 0.2 * (traits.eyeBig ? 1.25 : 1) * Math.max(0.8, size);
    const xs = eyeCount === 1 ? [0] : [-eyeSep, eyeSep];
    this.eyes = [];
    xs.forEach((x) => {
      const zf = front * Math.sqrt(Math.max(0.05, 1 - (x / sx) ** 2 - ((eyeY - cy) / sy) ** 2)) - 0.02;
      const eye = this.addOutlined(g, new THREE.SphereGeometry(eyeR, 20, 14), this.gloss(POP.white), 0.06);
      eye.position.set(x, eyeY, zf); eye.scale.z = 0.6;
      const pupil = new THREE.Mesh(new THREE.SphereGeometry(eyeR * 0.55, 16, 10), this.gloss(POP.black));
      pupil.position.set(0, -eyeR * 0.05, eyeR * 0.62); eye.add(pupil);
      const glint = new THREE.Mesh(new THREE.SphereGeometry(eyeR * 0.18, 8, 6), new THREE.MeshBasicMaterial({ color: POP.white }));
      glint.position.set(-eyeR * 0.22, eyeR * 0.22, eyeR * 0.95); eye.add(glint);
      if (stage >= 2) { // eyeshadow lid
        const lid = new THREE.Mesh(new THREE.SphereGeometry(eyeR * 1.06, 20, 10, 0, Math.PI * 2, 0, Math.PI * 0.42), this.toon(traits.lidColor));
        lid.rotation.x = 0.35; eye.add(lid);
        this.lids = (this.lids || []); this.lids.push(lid);
      }
      this.eyes.push(eye);
    });
    // glossy lips
    const mouthY = cy - sy * 0.12;
    const mz = front * Math.sqrt(Math.max(0.05, 1 - ((mouthY - cy) / sy) ** 2)) - 0.01;
    const lipMat = this.gloss(POP.red), ls = Math.max(0.8, size);
    const mouth = new THREE.Group(); mouth.position.set(0, mouthY, mz); g.add(mouth);
    const upper = new THREE.Group(); mouth.add(upper);
    [-1, 1].forEach((sd) => { const u = this.addOutlined(upper, new THREE.SphereGeometry(0.07 * ls, 14, 10), lipMat, 0.1); u.scale.set(1.25, 0.6, 0.6); u.position.set(sd * 0.06 * ls, 0.025 * ls, 0); });
    const lower = this.addOutlined(mouth, new THREE.SphereGeometry(0.1 * ls, 16, 10), lipMat, 0.1);
    lower.scale.set(1.45, 0.55, 0.6); lower.position.y = -0.035 * ls;
    const inner = new THREE.Mesh(new THREE.SphereGeometry(0.07 * ls, 12, 8), new THREE.MeshBasicMaterial({ color: 0x3a0010 }));
    inner.scale.set(1.4, 0.1, 0.3); inner.position.set(0, 0, -0.02); mouth.add(inner);
    this.mouth = [upper, lower, inner];
    // cheeks
    if (stage >= 2) [-1, 1].forEach((s) => {
      const ch = new THREE.Mesh(new THREE.CircleGeometry(0.09, 16), new THREE.MeshBasicMaterial({ color: 0xff6fa8, transparent: true, opacity: 0.85 }));
      const x = s * sx * 0.55, y = mouthY + 0.08;
      const z = front * Math.sqrt(Math.max(0.05, 1 - (x / sx) ** 2 - ((y - cy) / sy) ** 2)) + 0.01;
      ch.position.set(x, y, z); ch.lookAt(x * 2, y, z + 2); g.add(ch);
    });
    // sprout leaf
    if (stage === 1) {
      const leaf = this.addOutlined(g, new THREE.SphereGeometry(0.16, 12, 8), this.toon(POP.lime));
      leaf.scale.set(1.4, 0.35, 0.8); leaf.position.set(0.12, top + 0.12, 0); leaf.rotation.z = 0.5;
      const stem = this.addOutlined(g, new THREE.CylinderGeometry(0.03, 0.03, 0.2, 6), this.toon(POP.lime));
      stem.position.set(0, top + 0.05, 0);
    }
    // antenna
    if (traits.antenna && stage >= 3) {
      const ant = new THREE.Group(); ant.position.set(sx * 0.3, top - 0.05, 0); g.add(ant);
      const st = this.addOutlined(ant, new THREE.CylinderGeometry(0.025, 0.03, 0.45, 6), this.toon(POP.black), 0);
      st.position.y = 0.22;
      const ball = this.addOutlined(ant, new THREE.SphereGeometry(0.1, 12, 8), this.gloss(accent));
      ball.position.y = 0.48; this.antenna = ant;
    }
    // pop "swirl" hair tuft
    if (stage >= 4 && traits.hair) {
      const hairMat = this.toon(POP.yellow);
      const curve = new THREE.CatmullRomCurve3([...Array(9)].map((_, i) => {
        const a = i / 8 * Math.PI * 1.7;
        const rr = 0.28 * (1 - i / 10);
        return new THREE.Vector3(Math.cos(a) * rr - 0.05, top - 0.05 + i * 0.045 + Math.sin(a) * rr * 0.6, Math.sin(a) * rr * 0.4 + 0.1);
      }));
      this.addOutlined(g, new THREE.TubeGeometry(curve, 40, 0.09, 10, false), hairMat, 0.08);
    }
    // spikes for plucky sounds
    if (traits.spikes && stage >= 3) {
      const spikeMat = this.toon(accent);
      for (let i = 0; i < traits.spikes; i++) {
        const a = -0.6 + 1.2 * (i / Math.max(1, traits.spikes - 1));
        const sp = this.addOutlined(g, new THREE.ConeGeometry(0.1, 0.32, 8), spikeMat);
        sp.position.set(Math.sin(a) * sx * 0.75, cy + Math.cos(a) * sy * 0.95, -sz * 0.25);
        sp.rotation.z = -a;
      }
    }
    // horns
    if (stage >= 5 && traits.horns) [-1, 1].forEach((s) => {
      const h = this.addOutlined(g, new THREE.ConeGeometry(0.09, 0.38, 10), this.toon(POP.cream));
      h.position.set(s * sx * 0.55, top - 0.12, 0); h.rotation.z = -s * 0.5;
    });
    // elder: star sunglasses
    if (stage >= 6) {
      const sg = this.gloss(POP.pink);
      xs.forEach((x) => {
        const shape = new THREE.Shape();
        for (let k = 0; k < 10; k++) { const rad = k % 2 ? 0.13 : 0.3; const a = k / 10 * Math.PI * 2 + Math.PI / 2; const px = Math.cos(a) * rad, py = Math.sin(a) * rad; k ? shape.lineTo(px, py) : shape.moveTo(px, py); }
        const star = this.addOutlined(g, new THREE.ExtrudeGeometry(shape, { depth: 0.05, bevelEnabled: false }), sg, 0.04);
        star.position.set(x, eyeY, this.eyes[0].position.z + eyeR * 0.55); star.scale.setScalar(Math.max(0.8, size) * (eyeCount === 1 ? 1.3 : 1));
      });
    }
    // ancient: crown + orbiting stars
    this.orbits = [];
    if (stage >= 7) {
      const crown = new THREE.Group(); crown.position.set(0, top - 0.04, 0); g.add(crown);
      for (let k = 0; k < 5; k++) {
        const c = this.addOutlined(crown, new THREE.ConeGeometry(0.07, 0.26, 6), this.gloss(POP.yellow));
        const a = k / 5 * Math.PI * 2; c.position.set(Math.cos(a) * 0.22, 0.12, Math.sin(a) * 0.22);
      }
      this.addOutlined(crown, new THREE.TorusGeometry(0.24, 0.05, 8, 20), this.gloss(POP.yellow)).rotation.x = Math.PI / 2;
    }
    if (stage >= 6) for (let k = 0; k < 3; k++) {
      const o = new THREE.Mesh(new THREE.OctahedronGeometry(0.08), this.gloss([POP.yellow, POP.cyan, POP.pink][k]));
      g.add(o); this.orbits.push({ m: o, a: k * 2.09, r: sx + 0.5, y: cy });
    }
    this.shadow.scale.set(sx * 1.2, sz * 1.0, 1);
  }

  buildPod(g, traits, r) {
    const geo = new THREE.SphereGeometry(0.55, 40, 28);
    const pos = geo.attributes.position;
    for (let i = 0; i < pos.count; i++) { const y = pos.getY(i); pos.setY(i, y * (y > 0 ? 1.35 : 1.05)); }
    geo.computeVertexNormals();
    const pod = this.addOutlined(g, geo, this.gloss(POP.cream));
    pod.position.y = 0.62; this.bodyMesh = pod;
    const dotMat = this.toon(traits.accentColor);
    for (let i = 0; i < 11; i++) {
      const a = r() * Math.PI * 2, b = (r() - 0.3) * 1.3;
      const d = new THREE.Mesh(new THREE.SphereGeometry(0.1, 10, 8), dotMat);
      d.scale.set(1, 1, 0.3);
      const dir = new THREE.Vector3(Math.cos(a) * Math.cos(b), Math.sin(b) * 1.2, Math.sin(a) * Math.cos(b)).normalize();
      d.position.copy(dir.clone().multiply(new THREE.Vector3(0.55, 0.7, 0.55))).add(new THREE.Vector3(0, 0.62, 0));
      d.lookAt(d.position.clone().add(dir));
      g.add(d);
    }
    this.eyes = []; this.mouth = []; this.orbits = []; this.footL = this.footR = this.armL = this.armR = this.antenna = null; this.lids = [];
    this.shadow.scale.set(0.65, 0.6, 1);
  }

  nudge(dx) { this.spinVel += dx * 0.05; }

  // ---------------------------------------------------------------- frame
  render(t, dt, s) {
    if (!this.ok) return;
    const playing = s.playing, busy = s.busy;
    this.bounce = playing ? Math.min(1, this.bounce + dt * 6) : Math.max(0, this.bounce - dt * 3);
    this.pop = Math.max(0, this.pop - dt * 2.5);
    // turntable: gentle sway + inertia from spins / user drags, occasional twirl
    if (busy) this.spinVel = Math.max(this.spinVel, 6);
    if (!this.nextTwirl || t > this.nextTwirl) { this.nextTwirl = t + 7 + Math.random() * 6; if (!busy && !playing) this.spinVel += 7.5; }
    this.spin += this.spinVel * dt;
    this.spinVel *= Math.pow(0.35, dt);
    if (Math.abs(this.spinVel) < 0.05) { // ease back to facing the viewer
      const target = Math.round(this.spin / (Math.PI * 2)) * Math.PI * 2;
      this.spin += (target - this.spin) * Math.min(1, dt * 2.5);
    }
    const sway = Math.sin(t * 0.9) * 0.45;
    this.root.rotation.y = this.spin + sway;
    const g = this.body; if (!g) return;
    const hop = playing ? Math.abs(Math.sin(t * 9)) * 0.18 * this.bounce : Math.sin(t * 2) * 0.03;
    const squash = 1 - 0.1 * this.bounce * Math.abs(Math.cos(t * 9));
    const popS = 1 + Math.sin(this.pop * Math.PI) * 0.25;
    g.position.y = hop;
    g.scale.set(popS / Math.sqrt(squash), popS * squash, popS / Math.sqrt(squash));
    if (busy) g.rotation.z = Math.sin(t * 30) * 0.06; else g.rotation.z *= 0.8;
    this.shadow.material.opacity = 0.28 - hop * 0.6;
    if (this.armL) { this.armL.rotation.z = -0.9 - (playing ? Math.sin(t * 10) * 0.6 : Math.sin(t * 2) * 0.1); this.armR.rotation.z = 0.9 + (playing ? Math.sin(t * 10 + 1) * 0.6 : Math.sin(t * 2 + 1) * 0.1); }
    if (this.footL) { this.footL.position.y = 0.13 + (playing ? Math.max(0, Math.sin(t * 9)) * 0.06 : 0); this.footR.position.y = 0.13 + (playing ? Math.max(0, -Math.sin(t * 9)) * 0.06 : 0); }
    if (this.antenna) this.antenna.rotation.z = Math.sin(t * 3) * 0.25 + (playing ? Math.sin(t * 14) * 0.2 : 0);
    // blink
    const blink = (t % 4.3) < 0.12 || busy;
    this.eyes.forEach((e) => { e.scale.y = blink ? 0.12 : 1; });
    // mouth opens while playing
    if (this.mouth.length) {
      const open = playing ? Math.abs(Math.sin(t * 9)) : 0;
      this.mouth[0].position.y = open * 0.05; this.mouth[1].position.y = -0.035 - open * 0.05; this.mouth[2].scale.y = 0.1 + open * 1.2;
    }
    this.orbits.forEach((o, k) => { const a = t * 1.3 + o.a; o.m.position.set(Math.cos(a) * o.r, o.y + Math.sin(a * 1.7) * 0.25, Math.sin(a) * o.r); o.m.rotation.y = t * 3; });
    this.renderer.render(this.scene, this.camera);
  }
}

export const POP_COLORS = POP;
