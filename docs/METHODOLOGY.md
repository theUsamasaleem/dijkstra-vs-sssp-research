# Methodology

See Section IV ("Experimental Setup") of `paper/paper.pdf`. In short:
- Graph families: random sparse (m ≈ 5n), 2-D grid, Barabási–Albert scale-free (k = 3), random dense (G(n,p)); integer weights U[1,1000].
- Sizes 10⁴–10⁶ vertices; density sweep p ∈ {1,2,5,10,25,50,100}% at n = 2000.
- 10 random sources per graph (20 s–t pairs for A*), median and IQR reported, seed 42.
- Every algorithm's distance array is checked element-by-element against binary-heap Dijkstra.
- 30 s timeout per run; single-threaded; machine details in `benchmark/results/machine.txt`.
