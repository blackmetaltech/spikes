// Spike site: header menu, the inked Golgi cells behind the hero, clips that play while on screen, the lightbox, and the latest release.
// No dependencies. Everything degrades: without JS the clips have native controls, the zoom buttons do nothing, the download links go to Releases.
(() => {
  const REPO = "ixaxaar/spikes";
  const reduce = matchMedia("(prefers-reduced-motion: reduce)");

  // ---- header menu (phones) ----
  const btn = document.getElementById("menubtn"), links = document.getElementById("navlinks");
  if (btn && links) {
    const set = (o) => { links.classList.toggle("open", o); btn.setAttribute("aria-expanded", String(o)); };
    btn.addEventListener("click", () => set(!links.classList.contains("open")));
    links.addEventListener("click", (e) => { if (e.target.closest("a")) set(false); });
    addEventListener("keydown", (e) => { if (e.key === "Escape") set(false); });
  }

  // ---- Golgi-stained cells, after the title screen: an ink pyramidal cell, a sienna excitatory cell, a blue basket cell ----
  const cv = document.getElementById("cells");
  if (cv && cv.getContext) {
    const ctx = cv.getContext("2d");
    let segs = [], W = 0, H = 0, raf = 0;
    const rng = (seed) => () => ((seed = (seed * 1664525 + 1013904223) >>> 0) / 4294967296);
    function grow(r, out, x, y, ang, len, wid, depth, t, col, spread) {
      const n = 4, step = len / n;
      let px = x, py = y;
      for (let i = 0; i < n; i++) {
        ang += (r() - 0.5) * 0.45;
        const nx = px + Math.cos(ang) * step, ny = py + Math.sin(ang) * step;
        out.push({ x0: px, y0: py, x1: nx, y1: ny, w: Math.max(0.35, wid * (1 - i / (n * 2.2))), t: t + i, col });
        px = nx; py = ny;
      }
      if (depth <= 0) return;
      const k = r() < 0.25 ? 3 : 2;
      for (let j = 0; j < k; j++) {
        const da = (j - (k - 1) / 2) * spread * (0.7 + r() * 0.6);
        grow(r, out, px, py, ang + da, len * (0.62 + r() * 0.18), wid * 0.66, depth - 1, t + n, col, spread * 0.95);
      }
    }
    function cell(out, kind, x, y, s, seed) {
      const r = rng(seed);
      if (kind === "pyramid") {
        grow(r, out, x, y, -Math.PI / 2, 120 * s, 3.4 * s, 6, 0, "ink", 0.55);                 // apical dendrite
        for (let b = 0; b < 5; b++) grow(r, out, x, y + 6 * s, Math.PI * (0.15 + b * 0.175), 46 * s, 2.2 * s, 4, 0, "ink", 0.7); // basal tree
        grow(r, out, x, y + 8 * s, Math.PI / 2 + 0.05, 150 * s, 1.1 * s, 2, 0, "ink", 0.35);   // axon
        out.push({ soma: "tri", x, y, s, col: "ink", t: 0 });
      } else {
        const n = kind === "basket" ? 7 : 5;
        for (let b = 0; b < n; b++) grow(r, out, x, y, (b / n) * Math.PI * 2 + r() * 0.5, (kind === "basket" ? 40 : 54) * s, 1.8 * s, kind === "basket" ? 4 : 5, 0, kind === "basket" ? "inh" : "exc", 0.75);
        out.push({ soma: "dot", x, y, s, col: kind === "basket" ? "inh" : "exc", t: 0 });
      }
    }
    const COL = { ink: "rgba(35,28,23,", exc: "rgba(184,70,27,", inh: "rgba(30,79,143," };
    function layout() {
      const dpr = Math.min(devicePixelRatio || 1, 2);
      W = cv.clientWidth; H = cv.clientHeight;
      cv.width = Math.round(W * dpr); cv.height = Math.round(H * dpr); ctx.setTransform(dpr, 0, 0, dpr, 0, 0);
      segs = [];
      const wide = W > 980, s = wide ? Math.min(1.25, W / 1300) : Math.max(0.55, W / 900);
      if (wide) {
        cell(segs, "pyramid", W * 0.965, H * 0.62, s * 1.1, 7);
        cell(segs, "exc", W * 0.47, H * 0.2, s * 0.8, 11);
        cell(segs, "basket", W * 0.43, H * 0.83, s * 0.85, 5);
      } else {
        cell(segs, "pyramid", W * 0.93, H * 0.3, s, 7);
        cell(segs, "basket", W * 0.82, H * 0.07, s * 0.8, 5);
      }
    }
    function draw(upto) {
      ctx.clearRect(0, 0, W, H);
      ctx.lineCap = "round";
      for (const g of segs) {
        if (g.t > upto) continue;
        const a = g.col === "ink" ? 0.30 : 0.36;
        if (g.soma) {
          ctx.fillStyle = COL[g.col] + (a + 0.25) + ")";
          ctx.beginPath();
          if (g.soma === "tri") { ctx.moveTo(g.x - 9 * g.s, g.y + 9 * g.s); ctx.lineTo(g.x, g.y - 13 * g.s); ctx.lineTo(g.x + 9 * g.s, g.y + 9 * g.s); ctx.closePath(); }
          else ctx.arc(g.x, g.y, 6 * g.s, 0, Math.PI * 2);
          ctx.fill();
          continue;
        }
        ctx.strokeStyle = COL[g.col] + a + ")"; ctx.lineWidth = g.w;
        ctx.beginPath(); ctx.moveTo(g.x0, g.y0); ctx.lineTo(g.x1, g.y1); ctx.stroke();
      }
    }
    const maxT = () => segs.reduce((m, g) => Math.max(m, g.t), 0);
    function render(animate) {
      cancelAnimationFrame(raf); layout();
      if (!animate) { draw(Infinity); return; }
      const T = maxT(), t0 = performance.now(), dur = 2200;
      const tick = (now) => { const p = Math.min(1, (now - t0) / dur); draw(T * (1 - Math.pow(1 - p, 2.2))); if (p < 1) raf = requestAnimationFrame(tick); };
      raf = requestAnimationFrame(tick);
    }
    render(!reduce.matches);
    let rt = 0;
    addEventListener("resize", () => { clearTimeout(rt); rt = setTimeout(() => render(false), 150); });
  }

  // ---- clips: play while on screen; with reduced motion, only when asked ----
  const clips = [...document.querySelectorAll("[data-clip]")].map((box) => ({ box, v: box.querySelector("video"), user: false }));
  const pause = (c) => { c.v.pause(); c.box.classList.add("paused"); };
  const play = (c) => { c.v.preload = "auto"; const p = c.v.play(); c.box.classList.remove("paused"); if (p && p.catch) p.catch(() => c.box.classList.add("paused")); };
  for (const c of clips) {
    if (!c.v) continue;
    c.v.removeAttribute("controls");
    c.box.classList.add("paused");
    const toggle = () => { c.user = true; if (c.v.paused) play(c); else pause(c); };
    c.box.querySelector(".play")?.addEventListener("click", toggle);
    c.v.addEventListener("click", toggle);
  }
  if ("IntersectionObserver" in window) {
    const io = new IntersectionObserver((es) => {
      for (const e of es) {
        const c = clips.find((x) => x.box === e.target);
        if (!c || !c.v) continue;
        if (e.isIntersecting && !reduce.matches && !c.user) play(c);
        else if (!e.isIntersecting && !c.v.paused) pause(c);
      }
    }, { threshold: 0.35 });
    clips.forEach((c) => io.observe(c.box));
  }
  reduce.addEventListener?.("change", () => { if (reduce.matches) clips.forEach((c) => c.v && pause(c)); });

  // ---- lightbox over every .zoom button ----
  const lb = document.getElementById("lb"), zooms = [...document.querySelectorAll(".zoom[data-zoom]")];
  if (lb && typeof lb.showModal === "function" && zooms.length) {
    const img = document.getElementById("lb-img"), cap = document.getElementById("lb-cap");
    let i = 0, opener = null;
    const show = (k) => {
      i = (k + zooms.length) % zooms.length;
      const z = zooms[i], inner = z.querySelector("img");
      img.src = z.dataset.zoom; img.alt = inner ? inner.alt : "";
      cap.textContent = z.dataset.cap || (inner ? inner.alt : "");
    };
    zooms.forEach((z, k) => z.addEventListener("click", () => { opener = z; show(k); lb.showModal(); }));
    document.getElementById("lb-prev").addEventListener("click", () => show(i - 1));
    document.getElementById("lb-next").addEventListener("click", () => show(i + 1));
    document.getElementById("lb-close").addEventListener("click", () => lb.close());
    lb.addEventListener("click", (e) => { if (e.target === lb) lb.close(); });
    lb.addEventListener("keydown", (e) => { if (e.key === "ArrowLeft") show(i - 1); else if (e.key === "ArrowRight") show(i + 1); });
    lb.addEventListener("close", () => { img.src = "data:,"; opener && opener.focus(); });
  }

  // ---- the latest release: version, size and a direct link to the Linux zip ----
  const card = document.getElementById("dl-linux"), size = document.getElementById("dl-size"), ver = document.getElementById("ver"), main = document.getElementById("dl-main");
  if (card && window.fetch) {
    const ctl = new AbortController(), timer = setTimeout(() => ctl.abort(), 6000);
    fetch(`https://api.github.com/repos/${REPO}/releases/latest`, { headers: { Accept: "application/vnd.github+json" }, signal: ctl.signal })
      .then((r) => (r.ok ? r.json() : Promise.reject(r.status)))
      .then((rel) => {
        const a = (rel.assets || []).find((x) => /linux/i.test(x.name) && /x86_64|amd64/i.test(x.name) && /\.(zip|tar\.gz)$/i.test(x.name));
        const date = rel.published_at ? new Date(rel.published_at).toLocaleDateString(undefined, { year: "numeric", month: "short", day: "numeric" }) : "";
        if (ver) ver.innerHTML = `Latest: <a href="${rel.html_url}">${rel.tag_name}</a>${date ? ", " + date : ""}. Older builds are on <a href="https://github.com/${REPO}/releases">Releases</a>.`;
        if (a) {
          card.href = a.browser_download_url; if (main) main.href = a.browser_download_url;
          if (size) size.textContent = `${a.name} · ${(a.size / 1048576).toFixed(0)} MB`;
          const sum = (rel.assets || []).find((x) => x.name === a.name + ".sha256"), sha = document.getElementById("dl-sha");
          if (sum && sha) sha.href = sum.browser_download_url;
        }
      })
      .catch(() => {})
      .finally(() => clearTimeout(timer));
  }
})();
