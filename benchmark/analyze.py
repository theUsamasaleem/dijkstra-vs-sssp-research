"""Aggregate results/raw.csv into summary.csv, LaTeX tables and TikZ figures (no numpy needed)."""
import csv, math, os, statistics as stats
from collections import defaultdict

HERE = os.path.dirname(os.path.abspath(__file__))
RAW = os.path.join(HERE, "results", "raw.csv")
PAPER = os.path.join(HERE, "..", "paper", "generated")
os.makedirs(PAPER, exist_ok=True)

ALGOS = ["dijkstra_binary", "dijkstra_pairing", "dijkstra_thin", "dijkstra_array", "bellman_ford", "spfa"]
LABEL = {"dijkstra_binary": "Dij-BH", "dijkstra_pairing": "Dij-PH", "dijkstra_thin": "Dij-TH",
         "dijkstra_array": "Dij-Arr", "bellman_ford": "BF", "spfa": "SPFA",
         "p2p_dijkstra": "Dijkstra (early exit)", "p2p_astar": "A*"}
STYLE = {"dijkstra_binary": "blue, mark=*", "dijkstra_pairing": "teal, mark=square*", "dijkstra_thin": "cyan!70!black, mark=triangle*",
         "dijkstra_array": "gray, mark=diamond*", "bellman_ford": "red, mark=x", "spfa": "orange!90!black, mark=o"}
FAMNAME = {"sparse": "Random sparse", "grid": "2-D grid", "scalefree": "Scale-free (BA)", "dense": "Random dense"}

rows, seen = [], set()
for r in csv.DictReader(open(RAW)):  # the dense n=2000,p=50 config appears in two sweeps; keep one copy
    k = (r["family"], r["n"], r["param"], r["algo"], r["source"])
    if k not in seen:
        seen.add(k); rows.append(r)
groups = defaultdict(list)
meta = {}
for r in rows:
    key = (r["family"], int(r["n"]), int(r["param"]), r["algo"])
    groups[key].append(r)
    meta[(r["family"], int(r["n"]), int(r["param"]))] = int(r["m"])


def q(xs, p):
    xs = sorted(xs)
    k = (len(xs) - 1) * p
    lo, hi = math.floor(k), math.ceil(k)
    return xs[lo] + (xs[hi] - xs[lo]) * (k - lo)


summary = {}
with open(os.path.join(HERE, "results", "summary.csv"), "w", newline="") as f:
    w = csv.writer(f)
    w.writerow(["family", "n", "m", "param", "algo", "runs_ok", "timeouts", "median_ms", "q1_ms", "q3_ms", "relax_per_edge", "pq_ops_median"])
    for key in sorted(groups):
        g = groups[key]
        ok = [r for r in g if r["status"] == "ok"]
        to = sum(1 for r in g if r["status"] == "timeout")
        fam, n, p, a = key
        m = meta[(fam, n, p)]
        if ok:
            t = [float(r["time_ms"]) for r in ok]
            rel = stats.mean(int(r["relaxations"]) for r in ok) / m
            pqm = stats.median(int(r["pq_ops"]) for r in ok)
            s = dict(ok=len(ok), to=to, med=stats.median(t), q1=q(t, .25), q3=q(t, .75), rel=rel, pq=pqm, m=m)
        else:
            s = dict(ok=0, to=to, med=None, q1=None, q3=None, rel=None, pq=None, m=m)
        summary[key] = s
        w.writerow([fam, n, m, p, a, s["ok"], to,
                    *(f"{v:.3f}" if v is not None else "" for v in (s["med"], s["q1"], s["q3"], s["rel"])), s["pq"] or ""])


def fmt_ms(v):
    if v is None: return r"T/O"
    if v < 10: return f"{v:.2f}"
    if v < 100: return f"{v:.1f}"
    return f"{v:,.0f}".replace(",", "{,}")


def fmt_n(n):
    e = int(round(math.log10(n)))
    return f"$10^{e}$" if 10 ** e == n else f"{n:,}".replace(",", "{,}")


# ---------- Table 1: median time per family/size ----------
lines = [r"\begin{tabular}{llr" + "r" * len(ALGOS) + "}", r"\toprule",
         "Family & $n$ & $m$ & " + " & ".join(LABEL[a] for a in ALGOS) + r" \\", r"\midrule"]
configs = [("sparse", 4), ("grid", 0), ("scalefree", 3), ("dense", 50)]
for fam, p in configs:
    ns = sorted({k[1] for k in summary if k[0] == fam and k[2] == p})
    for i, n in enumerate(ns):
        vals = {a: summary.get((fam, n, p, a)) for a in ALGOS}
        best = min((v["med"] for v in vals.values() if v and v["med"] is not None), default=None)
        cells = []
        for a in ALGOS:
            v = vals[a]
            if v is None: cells.append("--"); continue
            c = fmt_ms(v["med"])
            if v["med"] is not None and v["med"] == best: c = r"\textbf{" + c + "}"
            cells.append(c)
        m = summary[(fam, n, p, "dijkstra_binary")]["m"]
        lines.append((FAMNAME[fam] if i == 0 else "") + f" & {fmt_n(n)} & " + f"{m:,}".replace(",", "{,}") + " & " + " & ".join(cells) + r" \\")
    lines.append(r"\midrule")
lines[-1] = r"\bottomrule"
lines.append(r"\end{tabular}")
open(os.path.join(PAPER, "table_main.tex"), "w").write("\n".join(lines) + "\n")

# ---------- Table 2: relaxations per edge (machine-independent work) ----------
lines = [r"\begin{tabular}{ll" + "r" * 3 + "}", r"\toprule",
         r"Family & $n$ & Dijkstra & BF & SPFA \\", r"\midrule"]
for fam, p in configs:
    ns = sorted({k[1] for k in summary if k[0] == fam and k[2] == p})
    for i, n in enumerate(ns):
        cells = []
        for a in ["dijkstra_binary", "bellman_ford", "spfa"]:
            v = summary.get((fam, n, p, a))
            cells.append("T/O" if not v or v["rel"] is None else f"{v['rel']:.2f}")
        lines.append((FAMNAME[fam] if i == 0 else "") + f" & {fmt_n(n)} & " + " & ".join(cells) + r" \\")
    lines.append(r"\midrule")
lines[-1] = r"\bottomrule"
lines.append(r"\end{tabular}")
open(os.path.join(PAPER, "table_relax.tex"), "w").write("\n".join(lines) + "\n")

# ---------- Table 3: A* vs Dijkstra point-to-point, heuristic-strength sweep ----------
ag = defaultdict(list)
for r in csv.DictReader(open(os.path.join(HERE, "results", "astar.csv"))):
    ag[(int(r["param"]), r["algo"])].append(r)
lines = [r"\begin{tabular}{rrrrrr}", r"\toprule",
         r"$w_{\min}$ & \multicolumn{2}{c}{Median time (ms)} & \multicolumn{2}{c}{Mean arcs scanned ($\times 10^3$)} & Speed-up \\",
         r"\cmidrule(lr){2-3}\cmidrule(lr){4-5}", r" & Dijkstra & A* & Dijkstra & A* & \\", r"\midrule"]
for wm in sorted({k[0] for k in ag}):
    a, b = ag[(wm, "p2p_dijkstra")], ag[(wm, "p2p_astar")]
    ta = stats.median(float(r["time_ms"]) for r in a); tb = stats.median(float(r["time_ms"]) for r in b)
    ra = stats.mean(int(r["relaxations"]) for r in a) / 1e3; rb = stats.mean(int(r["relaxations"]) for r in b) / 1e3
    lines.append(f"{wm} & {fmt_ms(ta)} & {fmt_ms(tb)} & {ra:,.0f} & {rb:,.0f} & {ta/tb:.1f}$\\times$".replace(",", "{,}") + r" \\")
lines += [r"\bottomrule", r"\end{tabular}"]
open(os.path.join(PAPER, "table_astar.tex"), "w").write("\n".join(lines) + "\n")


# ---------- TikZ log-log line plot ----------
def tikz_plot(series, xlabel, ylabel, xticks, fname, width=7.0, height=4.6, legend=True, xlog=True):
    """series: list of (algo, [(x, y)]) with y>0. Draws a log-log (or semilog-y) plot in plain TikZ."""
    allx = [x for _, pts in series for x, _ in pts]
    ally = [y for _, pts in series for _, y in pts]
    fx = (lambda x: math.log10(x)) if xlog else (lambda x: x)
    x0, x1 = fx(min(allx)), fx(max(allx))
    y0, y1 = math.floor(math.log10(min(ally))), math.ceil(math.log10(max(ally)))
    sx = lambda x: (fx(x) - x0) / (x1 - x0) * width
    sy = lambda y: (math.log10(y) - y0) / (y1 - y0) * height
    out = [r"\begin{tikzpicture}[font=\scriptsize]"]
    for e in range(y0, y1 + 1):
        yy = (e - y0) / (y1 - y0) * height
        out.append(rf"\draw[gray!25] (0,{yy:.3f}) -- ({width},{yy:.3f});")
        out.append(rf"\node[left] at (0,{yy:.3f}) {{$10^{{{e}}}$}};")
    for xv, lab in xticks:
        xx = sx(xv)
        out.append(rf"\draw[gray!25] ({xx:.3f},0) -- ({xx:.3f},{height});")
        out.append(rf"\node[below] at ({xx:.3f},0) {{{lab}}};")
    out.append(rf"\draw (0,0) rectangle ({width},{height});")
    out.append(rf"\node[below=12pt] at ({width/2},0) {{{xlabel}}};")
    out.append(rf"\node[rotate=90,above=18pt] at (0,{height/2}) {{{ylabel}}};")
    marks = {"*": r"\fill", "square*": r"\fill", "triangle*": r"\fill", "diamond*": r"\fill", "x": r"\draw", "o": r"\draw"}
    for i, (a, pts) in enumerate(series):
        col, mk = STYLE[a].split(", mark=")
        coords = " -- ".join(f"({sx(x):.3f},{sy(y):.3f})" for x, y in pts)
        if len(pts) > 1: out.append(rf"\draw[{col}, thick] {coords};")
        for x, y in pts:
            X, Y = sx(x), sy(y)
            if mk in ("*",): out.append(rf"\fill[{col}] ({X:.3f},{Y:.3f}) circle (1.6pt);")
            elif mk == "o": out.append(rf"\draw[{col}, fill=white] ({X:.3f},{Y:.3f}) circle (1.6pt);")
            elif mk == "square*": out.append(rf"\fill[{col}] ({X-0.05:.3f},{Y-0.05:.3f}) rectangle ({X+0.05:.3f},{Y+0.05:.3f});")
            elif mk == "triangle*": out.append(rf"\fill[{col}] ({X:.3f},{Y+0.07:.3f}) -- ({X-0.06:.3f},{Y-0.04:.3f}) -- ({X+0.06:.3f},{Y-0.04:.3f}) -- cycle;")
            elif mk == "diamond*": out.append(rf"\fill[{col}] ({X:.3f},{Y+0.07:.3f}) -- ({X-0.06:.3f},{Y:.3f}) -- ({X:.3f},{Y-0.07:.3f}) -- ({X+0.06:.3f},{Y:.3f}) -- cycle;")
            else: out.append(rf"\draw[{col}, thick] ({X-0.05:.3f},{Y-0.05:.3f}) -- ({X+0.05:.3f},{Y+0.05:.3f}) ({X-0.05:.3f},{Y+0.05:.3f}) -- ({X+0.05:.3f},{Y-0.05:.3f});")
        if legend:
            ly = height - 0.25 - i * 0.3
            out.append(rf"\draw[{col}, thick] ({width+0.2},{ly:.3f}) -- ({width+0.6},{ly:.3f});")
            out.append(rf"\node[right] at ({width+0.6},{ly:.3f}) {{{LABEL[a]}}};")
    out.append(r"\end{tikzpicture}")
    open(os.path.join(PAPER, fname), "w").write("\n".join(out) + "\n")


def series_for(fam, p, algos, xkey="n"):
    res = []
    for a in algos:
        pts = []
        for k, v in sorted(summary.items()):
            if k[0] == fam and k[2] == p and k[3] == a and v["med"]:
                pts.append((k[1], v["med"]))
        if pts: res.append((a, pts))
    return res


SCALE_ALGOS = ["dijkstra_binary", "dijkstra_pairing", "dijkstra_thin", "bellman_ford", "spfa"]
ticks = [(1e4, "$10^4$"), (1e5, "$10^5$"), (1e6, "$10^6$")]
for fam, p in [("sparse", 4), ("grid", 0), ("scalefree", 3)]:
    tikz_plot(series_for(fam, p, SCALE_ALGOS), "Vertices $n$", "Median time (ms)", ticks, f"fig_{fam}.tex",
              width=3.0, height=3.0, legend=(fam == "scalefree"))

# density sweep: x = edge probability (%), fixed n = 2000
dens = []
for a in ALGOS:
    pts = sorted((k[2], v["med"]) for k, v in summary.items() if k[0] == "dense" and k[1] == 2000 and k[3] == a and v["med"])
    if pts: dens.append((a, pts))
tikz_plot(dens, "Edge probability (\\%), $n=2000$", "Median time (ms)",
          [(1, "1"), (2, "2"), (5, "5"), (10, "10"), (25, "25"), (50, "50"), (100, "100")], "fig_density.tex", width=6.0, height=3.6)

# key numbers for the text
with open(os.path.join(PAPER, "numbers.txt"), "w") as f:
    for k, v in sorted(summary.items()):
        f.write(f"{k}: {v}\n")
print("wrote", PAPER)
