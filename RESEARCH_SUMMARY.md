# Summary

No single shortest-path algorithm wins everywhere. Graph **diameter** decides:
- Low-diameter graphs (random, social-network-like): SPFA / Bellman–Ford were up to 2.5× faster than Dijkstra.
- High-diameter graphs (grids, road-like): Dijkstra was 8× faster than SPFA and 22× faster than Bellman–Ford at 1M vertices.
- Fibonacci-type heaps did not help; a plain binary heap was fastest.
- A* only helps when its heuristic is tight (1.0× → 5.8×).

Details, tables and figures: `paper/paper.pdf`. Data: `benchmark/results/`.
