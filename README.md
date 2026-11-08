# Dijkstra vs Label-Correcting SSSP: A Reproducible Empirical Study

Code, raw data and paper for an empirical comparison of single-source shortest-path algorithms:
Dijkstra (binary heap, pairing heap, thin heap, array), Bellman–Ford, SPFA, and A* (point-to-point).

**Paper:** [`paper/paper.pdf`](paper/paper.pdf) (IEEE conference format)

## Key findings (all runs verified against a reference solution)
| Finding | Evidence |
|---|---|
| On low-diameter graphs (random sparse, scale-free), SPFA / Bellman–Ford beat Dijkstra in wall-clock time | up to 2.5× faster at n = 10⁵, despite 2–16× more edge relaxations |
| On grids, label-correcting methods scale badly; Dijkstra wins | n = 10⁶: Dijkstra 257 ms vs SPFA 2.05 s (8×) vs Bellman–Ford 5.7 s (22×) |
| A lazy binary heap beats pairing/thin (Fibonacci-type) heaps | 1.3–2.5× faster in every scaling experiment |
| Array Dijkstra O(n²) is competitive only near-complete graphs | p = 100%: 6.56 ms vs 6.84 ms (binary heap) |
| A*'s benefit depends entirely on heuristic tightness | 1.0× → 5.8× speed-up as the lower bound tightens |

## Reproduce
```bash
cd benchmark
./run_all.sh          # compiles bench.cpp, writes results/raw.csv and results/astar.csv (~10 min)
python3 analyze.py    # writes results/summary.csv and paper/generated/*.tex
cd ../paper && latexmk -pdf paper.tex
```
Requirements: g++ (C++17), Python 3 (standard library only), a LaTeX distribution with IEEEtran, TikZ, algorithm2e, cleveref.

## Layout
```
benchmark/bench.cpp     all algorithms + graph generators (single file)
benchmark/run_all.sh    full experiment suite
benchmark/analyze.py    aggregation, LaTeX tables, TikZ figures
benchmark/results/      raw CSV, summary, machine info
paper/                  paper.tex, refs.bib, generated tables/figures
```

## License
MIT. If you use this work, please cite the paper.
