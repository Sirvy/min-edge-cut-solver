CXXFLAGSOPT = -O3
CXX = g++
MPICXX = mpic++
CXXLIBS = -fopenmp
OUTPUT = bin

all:
	mkdir -p $(OUTPUT)
	$(CXX) $(CXXFLAGS) $(CXXFLAGSOPT) src/sequential.cpp -o $(OUTPUT)/sequential.out
	$(CXX) $(CXXFLAGS) $(CXXLIBS) $(CXXFLAGSOPT) src/task_parallel.cpp -o $(OUTPUT)/task_parallel.out
	$(CXX) $(CXXFLAGS) $(CXXLIBS) $(CXXFLAGSOPT) src/data_parallel.cpp -o $(OUTPUT)/data_parallel.out
	$(MPICXX) $(CXXFLAGS) $(CXXLIBS) $(CXXFLAGSOPT) src/mpi.cpp -o $(OUTPUT)/mpi.out

run-samples:
	@echo
	@echo "==== Running sequential on example_1 ===="
	./bin/sequential.out sample/example_1.txt 3

	@echo
	@echo "==== Running sequential on example_2 ===="
	./bin/sequential.out sample/example_2.txt 15

	@echo
	@echo "==== Running task_parallel on example_2 with 1 thread ===="
	./bin/task_parallel.out sample/example_2.txt 15 1

	@echo
	@echo "==== Running task_parallel on example_2 with 8 threads ===="
	./bin/task_parallel.out sample/example_2.txt 15 8

	@echo
	@echo "==== Running data_parallel on example_2 with 1 thread ===="
	./bin/data_parallel.out sample/example_2.txt 15 1

	@echo
	@echo "==== Running data_parallel on example_2 with 8 threads ===="
	./bin/data_parallel.out sample/example_2.txt 15 8

	@echo
	@echo "==== Running mpi on example_2 with 2 nodes and 8 threads ===="
	mpirun -np 2 ./bin/mpi.out sample/example_2.txt 15 8

	@echo
	@echo "==== Running mpi on example_2 with 4 nodes and 8 threads ===="
	mpirun -np 4 ./bin/mpi.out sample/example_2.txt 15 8

clean:
	rm -rf $(OUTPUT)