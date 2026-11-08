// Single-source shortest path benchmark: Dijkstra variants vs Bellman-Ford / SPFA / A*.
// Build: g++ -O2 -std=c++17 -o bench bench.cpp
// Usage: ./bench <family> <n> <seed> <sources> <timeout_s> [param]
//   family: sparse | dense | grid | scalefree
//   sparse: param = average out-degree (default 4)
//   dense:  param = edge probability in percent (default 50)
//   scalefree: param = edges per new node (default 3)
//   grid:   param = minimum edge weight (default 1); weights are U[param, 1000]
// Prints CSV rows: family,n,m,algo,source,time_ms,relaxations,pq_ops,status
#include <bits/stdc++.h>
#include <ext/pb_ds/priority_queue.hpp>
using namespace std;
using i64 = long long;
const i64 INF = LLONG_MAX / 4;

struct Graph {
    int n = 0;
    vector<int> off, to;
    vector<int> w;
    int cols = 0;  // grid width (for A* heuristic)
    int wmin = 1;  // minimum edge weight (scales the A* heuristic)
    i64 m() const { return (i64)to.size(); }
};

static Graph build(int n, vector<array<int, 3>>& e) {
    Graph g;
    g.n = n;
    g.off.assign(n + 1, 0);
    for (auto& x : e) g.off[x[0] + 1]++;
    for (int i = 0; i < n; i++) g.off[i + 1] += g.off[i];
    g.to.resize(e.size());
    g.w.resize(e.size());
    vector<int> pos(g.off.begin(), g.off.end() - 1);
    for (auto& x : e) {
        int p = pos[x[0]]++;
        g.to[p] = x[1];
        g.w[p] = x[2];
    }
    return g;
}

const int WMAX = 1000;

Graph gen_sparse(int n, int deg, mt19937_64& rng) {
    uniform_int_distribution<int> V(0, n - 1), W(1, WMAX);
    vector<array<int, 3>> e;
    e.reserve((size_t)n * (deg + 1));
    for (int i = 0; i < n; i++) e.push_back({i, (i + 1) % n, W(rng)});  // cycle => strongly connected
    for (i64 k = 0; k < (i64)n * deg; k++) {
        int u = V(rng), v = V(rng);
        if (u != v) e.push_back({u, v, W(rng)});
    }
    return build(n, e);
}

Graph gen_dense(int n, int pct, mt19937_64& rng) {
    uniform_int_distribution<int> P(0, 99), W(1, WMAX);
    vector<array<int, 3>> e;
    for (int u = 0; u < n; u++) {
        e.push_back({u, (u + 1) % n, W(rng)});
        for (int v = 0; v < n; v++)
            if (u != v && v != (u + 1) % n && P(rng) < pct) e.push_back({u, v, W(rng)});
    }
    return build(n, e);
}

Graph gen_grid(int side, int wmin, mt19937_64& rng) {
    uniform_int_distribution<int> W(wmin, WMAX);
    int n = side * side;
    vector<array<int, 3>> e;
    e.reserve((size_t)n * 4);
    for (int r = 0; r < side; r++)
        for (int c = 0; c < side; c++) {
            int u = r * side + c;
            if (c + 1 < side) { int w = W(rng); e.push_back({u, u + 1, w}); e.push_back({u + 1, u, w}); }
            if (r + 1 < side) { int w = W(rng); e.push_back({u, u + side, w}); e.push_back({u + side, u, w}); }
        }
    Graph g = build(n, e);
    g.cols = side;
    g.wmin = wmin;
    return g;
}

Graph gen_scalefree(int n, int k, mt19937_64& rng) {  // Barabasi-Albert, undirected (two arcs)
    uniform_int_distribution<int> W(1, WMAX);
    vector<int> targets;  // endpoint list for preferential attachment
    vector<array<int, 3>> e;
    for (int i = 0; i <= k; i++)
        for (int j = 0; j < i; j++) {
            int w = W(rng);
            e.push_back({i, j, w}); e.push_back({j, i, w});
            targets.push_back(i); targets.push_back(j);
        }
    for (int v = k + 1; v < n; v++) {
        set<int> chosen;
        while ((int)chosen.size() < k) chosen.insert(targets[uniform_int_distribution<size_t>(0, targets.size() - 1)(rng)]);
        for (int u : chosen) {
            int w = W(rng);
            e.push_back({u, v, w}); e.push_back({v, u, w});
            targets.push_back(u); targets.push_back(v);
        }
    }
    return build(n, e);
}

struct Stats { i64 relax = 0, pq = 0; bool ok = true; };
using Clock = chrono::steady_clock;

// Dijkstra, binary heap with lazy deletion (std::priority_queue)
Stats dijkstra_bh(const Graph& g, int s, vector<i64>& d, int target = -1) {
    Stats st;
    d.assign(g.n, INF);
    using P = pair<i64, int>;
    priority_queue<P, vector<P>, greater<P>> pq;
    d[s] = 0; pq.push({0, s}); st.pq++;
    while (!pq.empty()) {
        auto [du, u] = pq.top(); pq.pop(); st.pq++;
        if (du != d[u]) continue;
        if (u == target) break;
        for (int i = g.off[u]; i < g.off[u + 1]; i++) {
            int v = g.to[i]; st.relax++;
            i64 nd = du + g.w[i];
            if (nd < d[v]) { d[v] = nd; pq.push({nd, v}); st.pq++; }
        }
    }
    return st;
}

// Dijkstra with decrease-key on a GNU pb_ds heap (pairing heap or thin heap)
template <class Tag>
Stats dijkstra_pbds(const Graph& g, int s, vector<i64>& d) {
    Stats st;
    d.assign(g.n, INF);
    using P = pair<i64, int>;
    using PQ = __gnu_pbds::priority_queue<P, greater<P>, Tag>;
    PQ pq;
    vector<typename PQ::point_iterator> it(g.n, nullptr);
    vector<char> done(g.n, 0);
    d[s] = 0; it[s] = pq.push({0, s}); st.pq++;
    while (!pq.empty()) {
        auto [du, u] = pq.top(); pq.pop(); st.pq++;
        done[u] = 1;
        for (int i = g.off[u]; i < g.off[u + 1]; i++) {
            int v = g.to[i]; st.relax++;
            if (done[v]) continue;
            i64 nd = du + g.w[i];
            if (nd < d[v]) {
                d[v] = nd;
                if (it[v] == nullptr) it[v] = pq.push({nd, v});
                else pq.modify(it[v], {nd, v});
                st.pq++;
            }
        }
    }
    return st;
}

// Dijkstra with a plain array (O(V^2 + E)), the classic dense-graph variant
Stats dijkstra_array(const Graph& g, int s, vector<i64>& d) {
    Stats st;
    d.assign(g.n, INF);
    vector<char> done(g.n, 0);
    d[s] = 0;
    for (int it = 0; it < g.n; it++) {
        int u = -1; i64 best = INF;
        for (int v = 0; v < g.n; v++) if (!done[v] && d[v] < best) { best = d[v]; u = v; }
        st.pq += g.n;
        if (u < 0) break;
        done[u] = 1;
        for (int i = g.off[u]; i < g.off[u + 1]; i++) {
            int v = g.to[i]; st.relax++;
            i64 nd = best + g.w[i];
            if (nd < d[v]) d[v] = nd;
        }
    }
    return st;
}

// Bellman-Ford with early termination
Stats bellman_ford(const Graph& g, int s, vector<i64>& d, Clock::time_point deadline) {
    Stats st;
    d.assign(g.n, INF);
    d[s] = 0;
    for (int round = 0; round < g.n - 1; round++) {
        bool changed = false;
        for (int u = 0; u < g.n; u++) {
            if (d[u] == INF) continue;
            for (int i = g.off[u]; i < g.off[u + 1]; i++) {
                int v = g.to[i]; st.relax++;
                if (d[u] + g.w[i] < d[v]) { d[v] = d[u] + g.w[i]; changed = true; }
            }
        }
        if (!changed) break;
        if (Clock::now() > deadline) { st.ok = false; break; }
    }
    return st;
}

// SPFA (queue-based Bellman-Ford)
Stats spfa(const Graph& g, int s, vector<i64>& d, Clock::time_point deadline) {
    Stats st;
    d.assign(g.n, INF);
    vector<char> inq(g.n, 0);
    deque<int> q;
    d[s] = 0; q.push_back(s); inq[s] = 1; st.pq++;
    i64 pops = 0;
    while (!q.empty()) {
        int u = q.front(); q.pop_front(); inq[u] = 0; st.pq++;
        if ((++pops & 0xFFFF) == 0 && Clock::now() > deadline) { st.ok = false; break; }
        for (int i = g.off[u]; i < g.off[u + 1]; i++) {
            int v = g.to[i]; st.relax++;
            i64 nd = d[u] + g.w[i];
            if (nd < d[v]) { d[v] = nd; if (!inq[v]) { inq[v] = 1; q.push_back(v); st.pq++; } }
        }
    }
    return st;
}

// A* on a grid with an admissible Manhattan heuristic (min edge weight = 1)
Stats astar_grid(const Graph& g, int s, int t, vector<i64>& d) {
    Stats st;
    d.assign(g.n, INF);
    int C = g.cols, tr = t / C, tc = t % C;
    auto h = [&](int v) -> i64 { return (i64)g.wmin * (abs(v / C - tr) + abs(v % C - tc)); };
    using P = pair<i64, int>;
    priority_queue<P, vector<P>, greater<P>> pq;
    d[s] = 0; pq.push({h(s), s}); st.pq++;
    while (!pq.empty()) {
        auto [f, u] = pq.top(); pq.pop(); st.pq++;
        if (f != d[u] + h(u)) continue;
        if (u == t) break;
        for (int i = g.off[u]; i < g.off[u + 1]; i++) {
            int v = g.to[i]; st.relax++;
            i64 nd = d[u] + g.w[i];
            if (nd < d[v]) { d[v] = nd; pq.push({nd + h(v), v}); st.pq++; }
        }
    }
    return st;
}

int main(int argc, char** argv) {
    if (argc < 6) { fprintf(stderr, "usage: %s family n seed sources timeout_s [param]\n", argv[0]); return 1; }
    string fam = argv[1];
    int n = atoi(argv[2]);
    uint64_t seed = strtoull(argv[3], 0, 10);
    int K = atoi(argv[4]);
    double tmo = atof(argv[5]);
    int param = argc > 6 ? atoi(argv[6]) : -1;
    mt19937_64 rng(seed);
    Graph g;
    if (fam == "sparse") g = gen_sparse(n, param > 0 ? param : 4, rng);
    else if (fam == "dense") g = gen_dense(n, param > 0 ? param : 50, rng);
    else if (fam == "grid") { int side = (int)llround(sqrt((double)n)); g = gen_grid(side, param > 0 ? param : 1, rng); }
    else if (fam == "scalefree") g = gen_scalefree(n, param > 0 ? param : 3, rng);
    else { fprintf(stderr, "unknown family\n"); return 1; }

    mt19937_64 srng(seed * 7919 + 13);
    uniform_int_distribution<int> V(0, g.n - 1);
    vector<i64> ref, d;
    set<string> timed_out;  // once an algorithm times out on this graph, skip remaining sources
    auto emit = [&](const string& algo, int s, double ms, const Stats& st, const char* status) {
        printf("%s,%d,%lld,%s,%d,%.3f,%lld,%lld,%s\n", fam.c_str(), g.n, g.m(), algo.c_str(), s, ms, st.relax, st.pq, status);
        fflush(stdout);
    };
    auto run = [&](const string& algo, int s, auto&& fn) {
        if (timed_out.count(algo)) return;
        auto t0 = Clock::now();
        Stats st = fn(t0 + chrono::duration_cast<Clock::duration>(chrono::duration<double>(tmo)));
        double ms = chrono::duration<double, milli>(Clock::now() - t0).count();
        if (!st.ok) { timed_out.insert(algo); emit(algo, s, ms, st, "timeout"); return; }
        bool correct = (d == ref);
        emit(algo, s, ms, st, correct ? "ok" : "WRONG");
        if (!correct) { fprintf(stderr, "MISMATCH %s source %d\n", algo.c_str(), s); exit(2); }
    };

    bool p2p_only = getenv("P2P_ONLY") != nullptr;
    for (int k = 0; k < K && !p2p_only; k++) {
        int s = V(srng);
        auto t0 = Clock::now();
        Stats st = dijkstra_bh(g, s, ref);
        emit("dijkstra_binary", s, chrono::duration<double, milli>(Clock::now() - t0).count(), st, "ok");
        run("dijkstra_pairing", s, [&](auto) { return dijkstra_pbds<__gnu_pbds::pairing_heap_tag>(g, s, d); });
        run("dijkstra_thin", s, [&](auto) { return dijkstra_pbds<__gnu_pbds::thin_heap_tag>(g, s, d); });
        if (g.n <= 20000) run("dijkstra_array", s, [&](auto) { return dijkstra_array(g, s, d); });
        run("bellman_ford", s, [&](auto dl) { return bellman_ford(g, s, d, dl); });
        run("spfa", s, [&](auto dl) { return spfa(g, s, d, dl); });
    }

    // Point-to-point experiment on grids: Dijkstra with early exit vs A*
    if (fam == "grid") {
        for (int k = 0; k < K; k++) {
            int s = V(srng), t = V(srng);
            vector<i64> d1, d2;
            auto t0 = Clock::now();
            Stats a = dijkstra_bh(g, s, d1, t);
            double ms1 = chrono::duration<double, milli>(Clock::now() - t0).count();
            t0 = Clock::now();
            Stats b = astar_grid(g, s, t, d2);
            double ms2 = chrono::duration<double, milli>(Clock::now() - t0).count();
            if (d1[t] != d2[t]) { fprintf(stderr, "A* MISMATCH\n"); return 2; }
            emit("p2p_dijkstra", s, ms1, a, "ok");
            emit("p2p_astar", s, ms2, b, "ok");
        }
    }
    return 0;
}
