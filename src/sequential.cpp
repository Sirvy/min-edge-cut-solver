#include <iostream>
#include <fstream>
#include <string>
#include <vector>
#include <chrono>

using namespace std;

int min(const int x, const int y) {
    return x > y ? y : x;
}

struct Graph {
    vector<vector<int>> edges;
    int weightSum = 0;
    int numberOfNodes = 0;

    void loadFromFile(fstream & file) {
        file >> numberOfNodes;
        edges = vector<vector<int>>(numberOfNodes, vector<int>(numberOfNodes, 0));
        {
            int i = 0;
            int j = 0;
            int x;
            while (file >> x) {
                edges[i][j++] = x;
                weightSum += x;
                if (j == numberOfNodes) {
                    j = 0;
                    i++;
                }
            }
        }
    }
};

class MROSolver {
    Graph * m_graph;
    int m_a;
    struct resultState {
        int m_value;
        vector<bool> m_bits;
    } m_minimum;

    struct state {
        vector<bool> m_bits;
        int m_depth;
        int m_oneCounter;
        int m_weight;
        state(vector<bool> bits, int depth, int oneCounter, int weight)
            :   m_bits(bits),
                m_depth(depth),
                m_oneCounter(oneCounter),
                m_weight(weight) 
            {}
    };
    
    int calculateWeight(const vector<bool> & bits, int idx, bool value, int maxSize = 0) const {
        if (maxSize == 0) maxSize = idx;
        int sum = 0;
        for (int i = 0; i < maxSize; i++) {
            if (bits[i] != value) {
                sum += m_graph->edges[idx][i];
            }
        }
        return sum;
    }

    int calculateDOVZR(const vector<bool> & bits, int size) const {
        int sum = 0;
        for (size_t i = size; i < bits.size(); i++) {
            sum += min(calculateWeight(bits, i, false, size), calculateWeight(bits, i, true, size));
        }
        return sum;
    }

    bool isOk(const state & s) const {
        if (s.m_oneCounter > m_a) return false;
        if (s.m_weight >= m_minimum.m_value) return false;
        if ((m_graph->numberOfNodes - s.m_depth) < (m_a - s.m_oneCounter)) return false;
        if (s.m_weight + calculateDOVZR(s.m_bits, s.m_depth) >= m_minimum.m_value) return false;

        return true;
    }

    void dfs(state & s) {
        if (s.m_depth == m_graph->numberOfNodes) {
            if (s.m_oneCounter != m_a) return;
            if (s.m_weight < m_minimum.m_value) {
                m_minimum.m_value = s.m_weight;
                m_minimum.m_bits = s.m_bits;
            }
            return;
        }

        s.m_bits[s.m_depth] = false;
        state nextStateLeft(s.m_bits, s.m_depth+1, s.m_oneCounter, s.m_weight + calculateWeight(s.m_bits, s.m_depth, false));
        if (isOk(nextStateLeft)) {
            dfs(nextStateLeft);
        }

        s.m_bits[s.m_depth] = true;
        state nextStateRight(s.m_bits, s.m_depth+1, s.m_oneCounter+1, s.m_weight + calculateWeight(s.m_bits, s.m_depth, true));
        if (isOk(nextStateRight)) {
            dfs(nextStateRight);
        }
    }

public:
    MROSolver(Graph * graph, int a)
        : m_graph(graph), m_a(a) {
            m_minimum.m_value = m_graph->weightSum;
            m_minimum.m_bits = vector<bool>(m_graph->numberOfNodes, 0);
        }

    void solve() {
        state s(vector<bool>(m_graph->numberOfNodes, 0), 0, 0, 0);
        dfs(s);

        cout << "minimum: " << m_minimum.m_value << endl;
        cout << "solution: ";
        for (auto it : m_minimum.m_bits) {
            cout << it << " ";
        }
        cout << endl;
    }
};

int main(int argc, char *argv[]) {

    if (argc != 3) {
        cout << "Not enough arguments! ./a.out FILENAME PARAMETER_A" << endl;
        return 1;
    }

    string filename = argv[1];
    int a = atoi(argv[2]);

    fstream file(filename, ios_base::in);

    Graph * graph = new Graph();
    graph->loadFromFile(file);

    MROSolver * solver = new MROSolver(graph, a);
    
    auto start = chrono::high_resolution_clock::now();

    solver->solve();

    auto stop = chrono::high_resolution_clock::now();
    auto duration = chrono::duration_cast<chrono::milliseconds>(stop - start);
    cout << "duration: " << duration.count() / 1000.0 << " s" << endl << endl;

    delete solver;

    return 0;
}