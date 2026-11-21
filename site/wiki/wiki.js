// Spike wiki: header menu, search over search.json (titles, headings, summaries and text; all words must match),
// the page list folded away on a phone, "on this page" highlighting, previous/next with the [ and ] keys,
// clips that play while on screen (not with reduced motion), and the image lightbox. No dependencies.
(() => {
  const esc = (s) => String(s).replace(/&/g, "&amp;").replace(/</g, "&lt;").replace(/>/g, "&gt;").replace(/"/g, "&quot;");
  const reduce = matchMedia("(prefers-reduced-motion: reduce)");

  // ---- header menu ----
  const btn = document.getElementById("menubtn"), links = document.getElementById("navlinks");
  if (btn && links) {
    const set = (o) => { links.classList.toggle("open", o); btn.setAttribute("aria-expanded", String(o)); };
    btn.addEventListener("click", () => set(!links.classList.contains("open")));
    addEventListener("keydown", (e) => { if (e.key === "Escape") set(false); });
  }

  // ---- the page list: open on wide screens, folded on a phone (unless you are on the index) ----
  const pages = document.getElementById("pages");
  if (pages && matchMedia("(max-width: 900px)").matches && !/\/(index\.html)?$/.test(location.pathname)) pages.open = false;
  const cur = document.querySelector(".pages a[aria-current=page]"), side = document.getElementById("side");
  if (cur && side && side.scrollHeight > side.clientHeight) {
    const r = cur.getBoundingClientRect(), s = side.getBoundingClientRect();
    if (r.bottom > s.bottom - 40) side.scrollTop += r.top - s.top - s.height / 3;
  }

  // ---- search ----
  const q = document.getElementById("q"), out = document.getElementById("results");
  if (q && out) {
    let index = null, sel = -1;
    const load = () => index || (index = fetch("search.json").then((r) => r.json()).catch(() => []));
    const mark = (text, terms) => {
      let h = esc(text);
      for (const t of terms) h = h.replace(new RegExp("(" + t.replace(/[.*+?^${}()|[\]\\]/g, "\\$&") + ")", "ig"), "<mark>$1</mark>");
      return h;
    };
    const snippet = (text, terms) => {
      const low = text.toLowerCase();
      let i = -1;
      for (const t of terms) { i = low.indexOf(t); if (i >= 0) break; }
      if (i < 0) return text.slice(0, 120) + (text.length > 120 ? "…" : "");
      const s = Math.max(0, i - 50);
      return (s > 0 ? "…" : "") + text.slice(s, s + 130).trim() + "…";
    };
    const score = (p, terms) => {
      const title = p.t.toLowerCase(), sum = (p.u || "").toLowerCase(), text = p.x.toLowerCase();
      let total = 0, anchor = "";
      for (const t of terms) {
        let s = 0;
        if (title.includes(t)) s += title.startsWith(t) ? 30 : 20;
        const h = p.h.find(([, ht]) => ht.toLowerCase().includes(t));
        if (h) { s += 8; if (!anchor) anchor = h[0]; }
        if (sum.includes(t)) s += 4;
        const n = text.split(t).length - 1;
        s += Math.min(n, 12);
        if (!s) return null;   // every word must appear somewhere
        total += s;
      }
      return { p, s: total, anchor: title.includes(terms[0]) ? "" : anchor };
    };
    const items = () => [...out.querySelectorAll("a")];
    const select = (k) => {
      const a = items();
      if (!a.length) return;
      sel = (k + a.length) % a.length;
      a.forEach((x, i) => x.setAttribute("aria-selected", String(i === sel)));
      a[sel].scrollIntoView({ block: "nearest" });
    };
    const run = async () => {
      const raw = q.value.trim().toLowerCase();
      const terms = raw.split(/\s+/).filter((t) => t.length >= 2);
      sel = -1;
      if (!terms.length) { out.hidden = true; out.innerHTML = ""; return; }
      const hits = (await load()).map((p) => score(p, terms)).filter(Boolean).sort((a, b) => b.s - a.s).slice(0, 8);
      out.innerHTML = hits.length
        ? hits.map(({ p, anchor }) => `<li><a href="${esc(p.s)}.html${anchor ? "#" + esc(anchor) : ""}">${mark(p.t, terms)}${p.c ? `<span class="sec">${esc(p.c)}</span>` : ""}<small>${mark(snippet(p.x, terms), terms)}</small></a></li>`).join("")
        : `<li class="none">Nothing matches “${esc(raw)}”.</li>`;
      out.hidden = false;
    };
    q.addEventListener("input", run);
    q.addEventListener("focus", load, { once: true });
    q.addEventListener("keydown", (e) => {
      if (e.key === "ArrowDown") { e.preventDefault(); select(sel + 1); }
      else if (e.key === "ArrowUp") { e.preventDefault(); select(sel - 1); }
      else if (e.key === "Enter") { const a = items()[sel >= 0 ? sel : 0]; if (a) { e.preventDefault(); location.href = a.href; } }
      else if (e.key === "Escape") { q.value = ""; out.hidden = true; out.innerHTML = ""; }
    });
    document.addEventListener("click", (e) => { if (!e.target.closest(".search")) out.hidden = true; });
    addEventListener("keydown", (e) => {
      const typing = /^(INPUT|TEXTAREA|SELECT)$/.test(document.activeElement.tagName) || document.activeElement.isContentEditable;
      if (typing || e.ctrlKey || e.metaKey || e.altKey) return;
      if (e.key === "/") { e.preventDefault(); q.focus(); }
      else if (e.key === "[" || e.key === "]") {
        const a = document.querySelector(e.key === "[" ? ".pager .prev" : ".pager .next");
        if (a) location.href = a.href;
      }
    });
  }

  // ---- on this page: highlight the section in view ----
  const tocLinks = [...document.querySelectorAll(".toc a")];
  if (tocLinks.length && "IntersectionObserver" in window) {
    const byId = new Map(tocLinks.map((a) => [decodeURIComponent(a.hash.slice(1)), a]));
    const heads = [...byId.keys()].map((id) => document.getElementById(id)).filter(Boolean);
    const seen = new Set();
    const io = new IntersectionObserver((es) => {
      for (const e of es) e.isIntersecting ? seen.add(e.target) : seen.delete(e.target);
      const first = heads.find((h) => seen.has(h));
      if (first) tocLinks.forEach((a) => a.classList.toggle("on", a === byId.get(first.id)));
    }, { rootMargin: "-60px 0px -60% 0px" });
    heads.forEach((h) => io.observe(h));
  }

  // ---- clips ----
  const clips = [...document.querySelectorAll("[data-clip]")].map((box) => ({ box, v: box.querySelector("video"), user: false }));
  const pause = (c) => { c.v.pause(); c.box.classList.add("paused"); };
  const play = (c) => { c.v.preload = "auto"; const p = c.v.play(); c.box.classList.remove("paused"); if (p && p.catch) p.catch(() => c.box.classList.add("paused")); };
  for (const c of clips) {
    if (!c.v) continue;
    c.v.removeAttribute("controls"); c.box.classList.add("paused");
    const toggle = () => { c.user = true; c.v.paused ? play(c) : pause(c); };
    c.box.querySelector(".play")?.addEventListener("click", toggle);
    c.v.addEventListener("click", toggle);
  }
  if (clips.length && "IntersectionObserver" in window) {
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

  // ---- lightbox ----
  const lb = document.getElementById("lb"), zooms = [...document.querySelectorAll(".zoom[data-zoom]")];
  if (lb && typeof lb.showModal === "function" && zooms.length) {
    const img = document.getElementById("lb-img"), cap = document.getElementById("lb-cap");
    let i = 0, opener = null;
    const show = (k) => {
      i = (k + zooms.length) % zooms.length;
      const z = zooms[i], inner = z.querySelector("img"), fc = z.closest("figure")?.querySelector("figcaption");
      img.src = z.dataset.zoom; img.alt = inner ? inner.alt : "";
      cap.textContent = fc ? fc.textContent : inner ? inner.alt : "";
    };
    zooms.forEach((z, k) => z.addEventListener("click", () => { opener = z; show(k); lb.showModal(); }));
    document.getElementById("lb-prev").addEventListener("click", () => show(i - 1));
    document.getElementById("lb-next").addEventListener("click", () => show(i + 1));
    document.getElementById("lb-close").addEventListener("click", () => lb.close());
    lb.addEventListener("click", (e) => { if (e.target === lb) lb.close(); });
    lb.addEventListener("keydown", (e) => { if (e.key === "ArrowLeft") show(i - 1); else if (e.key === "ArrowRight") show(i + 1); });
    lb.addEventListener("close", () => { img.src = "data:,"; opener && opener.focus(); });
  }
})();
