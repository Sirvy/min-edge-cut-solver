#include <iostream>
#include <fstream>
#include <string>
#include <vector>
#include <chrono>
#include <mpi.h>

#define INF 9999999
#define min(X, Y) (((X) < (Y)) ? (X) : (Y))
#define MAX_ARRAY_SIZE 100

#define TAG_DONE 0
#define TAG_WORK 1
#define TAG_TERMINATE 2

#define DEBUG false

using namespace std;

class MROSolver {
private:
    struct state {
        vector<bool> m_bits;
        int m_depth;
        int m_oneCounter;
        int m_weight;
        state() {}
        state(vector<bool> bits, int depth, int oneCounter, int weight)
            :   m_bits(bits),
                m_depth(depth),
                m_oneCounter(oneCounter),
                m_weight(weight) 
            {}
    };

    vector<vector<int>> m_edges;
    int m_n, m_a, m_k, m_dfsMaxDepth;
    struct minimum_state {
        int m_value;
        vector<bool> m_bits;
    } m_minimum;
    vector<state> m_items;

    struct mpi_state_message {
        bool bits[MAX_ARRAY_SIZE];
        int bitsSize;
        int depth;
        int oneCounter;
        int weight;
    };

    struct mpi_minimum_message {
        bool bits[MAX_ARRAY_SIZE];
        int bitsSize;
        int value;
    };
    
    int calculateWeight(vector<bool> & bits, int depth, bool value, int maxSize = 0) {
        if (maxSize == 0) maxSize = depth;
        int sum = 0;
        for (int i = 0; i < maxSize; i++) {
            if (bits[i] != value) {
                sum += m_edges[depth][i];
            }
        }
        return sum;
    }

    int calculateDOVZR(vector<bool> & bits, int size) {
        int sum = 0;
        for (size_t i = size; i < bits.size(); i++) {
            sum += min(calculateWeight(bits, i, false, size), calculateWeight(bits, i, true, size));
        }
        return sum;
    }

    bool isOk(state & s) {
        if (s.m_oneCounter > m_a) return false;
        if (s.m_weight >= m_minimum.m_value) return false;
        if ((m_n - s.m_depth) < (m_a - s.m_oneCounter)) return false;
        if (s.m_weight + calculateDOVZR(s.m_bits, s.m_depth) >= m_minimum.m_value) return false;

        return true;
    }

    void dfs(state & s) {
        s.m_bits[s.m_depth] = false;
        state nextStateLeft(s.m_bits, s.m_depth+1, s.m_oneCounter, s.m_weight + calculateWeight(s.m_bits, s.m_depth, false));
        if (isOk(nextStateLeft)) {
            if (s.m_depth < m_dfsMaxDepth) {
                dfs(nextStateLeft);
            } else {
                m_items.push_back(nextStateLeft);
            }
        }

        s.m_bits[s.m_depth] = true;
        state nextStateRight(s.m_bits, s.m_depth+1, s.m_oneCounter+1, s.m_weight + calculateWeight(s.m_bits, s.m_depth, true));
        if (isOk(nextStateRight)) {
            if (s.m_depth < m_dfsMaxDepth) {
                dfs(nextStateRight);
            } else {
                m_items.push_back(nextStateRight);
            }
        }
    }

    void dfsAlmostSeq(state & s) {
        if (s.m_depth == m_n) {
            if (s.m_oneCounter != m_a) return;
            if (s.m_weight < m_minimum.m_value) {
                #pragma omp critical
                {
                    if (s.m_weight < m_minimum.m_value) {
                        m_minimum.m_value = s.m_weight;
                        m_minimum.m_bits = s.m_bits;
                    }
                }
            }
            return;
        }

        s.m_bits[s.m_depth] = false;
        state nextStateLeft(s.m_bits, s.m_depth+1, s.m_oneCounter, s.m_weight + calculateWeight(s.m_bits, s.m_depth, false));
        if (isOk(nextStateLeft)) {
            #pragma omp task if (nextStateLeft.m_depth+3 < m_n)
            dfsAlmostSeq(nextStateLeft);
        }

        s.m_bits[s.m_depth] = true;
        state nextStateRight(s.m_bits, s.m_depth+1, s.m_oneCounter+1, s.m_weight + calculateWeight(s.m_bits, s.m_depth, true));
        if (isOk(nextStateRight)) {
            #pragma omp task if (nextStateRight.m_depth+3 < m_n)
            dfsAlmostSeq(nextStateRight);
        }

        #pragma omp taskwait
    }

    mpi_state_message packState(state s) {                
        mpi_state_message msg;
        msg.depth = s.m_depth;
        msg.oneCounter = s.m_oneCounter;
        msg.weight = s.m_weight;
        msg.bitsSize = s.m_bits.size();
        int j = 0;
        for (auto i = s.m_bits.begin(); i != s.m_bits.end(); ++i) {
            msg.bits[j++] = *i;
        }

        return msg;
    }

    mpi_minimum_message packMinimum(minimum_state minimum) {
        mpi_minimum_message minimumMessage;
        minimumMessage.value = minimum.m_value;
        minimumMessage.bitsSize = minimum.m_bits.size();
        int j = 0;
        for (auto i = minimum.m_bits.begin(); i != minimum.m_bits.end(); ++i) {
            minimumMessage.bits[j++] = *i;
        }

        return minimumMessage;
    }

    state unpackStateMessage(mpi_state_message msg) {
        state message;
        message.m_depth = msg.depth;
        message.m_oneCounter = msg.oneCounter;
        message.m_weight = msg.weight;
        for (int i = 0; i < msg.bitsSize; i++) {
            message.m_bits.push_back(msg.bits[i]);
        }

        return message;
    }

    minimum_state unpackMinimumMessage(mpi_minimum_message msg) {
        minimum_state minimum;
        minimum.m_value = msg.value;
        for (int i = 0; i < msg.bitsSize; i++) {
            minimum.m_bits.push_back(msg.bits[i]);
        }

        return minimum;
    }

public:
    MROSolver(vector<vector<int>> & edges, int n, int a, int k)
        : m_edges(edges), m_n(n), m_a(a), m_k(k) {
            m_minimum.m_value = INF;
            m_minimum.m_bits = vector<bool>(n, 0);
            m_dfsMaxDepth = n > 3 ? 3 : n-1;
        }

    void solveMaster(int numberOfProcesses) {
        /* Perform DFS to create jobs */
        vector<bool> bits(m_n, 0);
        state s(bits, 0, 0, 0);
        dfs(s);

        int processingItemIndex = 0;
        int maxProcessingItems = m_items.size();

        /* Initial work distribution */
        for (int dest = 1; dest < numberOfProcesses; dest++) {
            if (processingItemIndex < maxProcessingItems) {
                mpi_state_message msg = packState(m_items[processingItemIndex]);
                MPI_Send(&msg, sizeof(struct mpi_state_message), MPI_PACKED, dest, TAG_WORK, MPI_COMM_WORLD);
                if (DEBUG) {
                    cout << "Master sending tag TAG_WORK to slave " << dest << endl;
                }
                processingItemIndex++;
            }
        }

        int workingSlaves = numberOfProcesses - 1;
        MPI_Status status;

        while (workingSlaves > 0) {
            /* Receive result from Slave */
            mpi_minimum_message minimumMessage;
            MPI_Recv(&minimumMessage, sizeof(struct mpi_minimum_message), MPI_PACKED, MPI_ANY_SOURCE, TAG_DONE, MPI_COMM_WORLD, &status);
            if (DEBUG) {
                cout << "Master received tag TAG_DONE from slave " << status.MPI_SOURCE << endl;
            }
            minimum_state localMinimum = unpackMinimumMessage(minimumMessage);

            /* Update best result */
            if (localMinimum.m_value < m_minimum.m_value) {
                m_minimum.m_value = localMinimum.m_value;
                m_minimum.m_bits = localMinimum.m_bits;
            }

            /* If there are more jobs to do */
            if (processingItemIndex < maxProcessingItems) {
                /* Delegate the job to the Slave that is free */
                mpi_state_message msg = packState(m_items[processingItemIndex]);
                MPI_Send(&msg, sizeof(struct mpi_state_message), MPI_PACKED, status.MPI_SOURCE, TAG_WORK, MPI_COMM_WORLD);
                if (DEBUG) {
                    cout << "Master sending tag TAG_WORK to slave " << status.MPI_SOURCE << endl;
                }
                processingItemIndex++;
            } else {
                /* Send termination signal to the slaves */
                MPI_Send(nullptr, 0, MPI_PACKED, status.MPI_SOURCE, TAG_TERMINATE, MPI_COMM_WORLD);
                if (DEBUG) {
                    cout << "Master sending tag TAG_TERMINATE to slave " << status.MPI_SOURCE << endl;
                }
                workingSlaves--;
            }    
        }

        /* Print final result */
        cout << "minimum: " << m_minimum.m_value << endl;
        cout << "solution: ";
        for (auto it : m_minimum.m_bits) {
            cout << it << " ";
        }
        cout << endl;
    }

    void solveSlave(int rank) {
        MPI_Status status;

        while (true) {
            /* Receive job from master */
            mpi_state_message msg;
            MPI_Recv(&msg, sizeof(struct mpi_state_message), MPI_PACKED, 0, MPI_ANY_TAG, MPI_COMM_WORLD, &status);

            if (status.MPI_TAG == TAG_TERMINATE) {
                if (DEBUG) {
                    cout << "Slave " << rank << " received tag TAG_TERMINATE from master" << endl;
                }
                break;
            }
            else if (status.MPI_TAG == TAG_WORK) {
                if (DEBUG) {
                    cout << "Slave " << rank << " received tag TAG_WORK from master" << endl;
                }

                state message = unpackStateMessage(msg);

                #pragma omp parallel num_threads(m_k)
                {
                    #pragma omp single
                    dfsAlmostSeq(message);
                }

                /* Send best result found back to Master */
                mpi_minimum_message minimumMessage = packMinimum(m_minimum);
                MPI_Send(&minimumMessage, sizeof(struct mpi_minimum_message), MPI_PACKED, 0, TAG_DONE, MPI_COMM_WORLD);
                if (DEBUG) {
                    cout << "Slave " << rank << " sends tag TAG_DONE to master" << endl;
                }
            }
        }
    }
};

int main(int argc, char *argv[]) {

    if (argc != 4) {
        cout << "Not enough arguments! ./a.out filename a k" << endl;
        return 1;
    }

    string filename = argv[1];
    int a = atoi(argv[2]);
    int k = atoi(argv[3]);

    int n;
    fstream file(filename, ios_base::in);

    if (!file) {
        cout << "File " << filename << " doesn't exist" << endl;
        return 1;
    }

    file >> n;

    /* parse input data (graph) into matrix of edges */
    vector<vector<int>> edges(n, vector<int>(n, 0));
    {
        int i = 0;
        int j = 0;
        int x;
        while (file >> x) {
            edges[i][j++] = x;
            if (j == n) {
                j = 0;
                i++;
            }
        }
    }

    int myRank;
    int numberOfProcesses;

    MPI_Init( &argc, &argv );

    /* find out process rank */
    MPI_Comm_rank(MPI_COMM_WORLD, &myRank);

    /* find out number of processes */
    MPI_Comm_size(MPI_COMM_WORLD, &numberOfProcesses);

    MROSolver * solver = new MROSolver(edges, n, a, k);

    if (myRank == 0) { 
        /* Master */
        
        auto start = chrono::high_resolution_clock::now();
        
        solver->solveMaster(numberOfProcesses);

        auto stop = chrono::high_resolution_clock::now();
        auto duration = chrono::duration_cast<chrono::milliseconds>(stop - start);
        cout << "duration: " << duration.count() / 1000.0 << " s" << endl << endl;
    }
    else { 
        /* Slave */ 
        
        solver->solveSlave(myRank);
    }

    delete solver;

    MPI_Finalize();


    return 0;
}