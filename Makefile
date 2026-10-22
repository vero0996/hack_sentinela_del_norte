# SENTINELA DEL NORTE - tareas del proyecto
CXX ?= c++
CXXFLAGS = -std=c++17 -O2 -Wall -Wextra -Wpedantic
TESTS = mst tsp max_flow voronoi matching palindrome common_substring
SAMPLES = test/samples

.PHONY: all test bench app data grader-zip deliverable-zip clean

all: sentinel

# Nucleo C++: un solo main.cpp con encabezados.
sentinel: src/main.cpp $(wildcard src/*.h)
	$(CXX) $(CXXFLAGS) src/main.cpp -o $@

# Pruebas unitarias, verificador del autograder y modo archivos.
test: sentinel
	@mkdir -p build
	@for t in $(TESTS); do \
		$(CXX) $(CXXFLAGS) test/test_$$t.cpp -o build/test_$$t || exit 1; \
		./build/test_$$t > build/test_$$t.log || { cat build/test_$$t.log; exit 1; }; \
		tail -1 build/test_$$t.log; \
	done
	python3 test/grader_check.py ./sentinel src/main.cpp
	./sentinel $(SAMPLES)/transmission1.txt $(SAMPLES)/transmission2.txt $(SAMPLES)/mcode1.txt $(SAMPLES)/mcode2.txt $(SAMPLES)/mcode3.txt | diff - $(SAMPLES)/expected_output.txt
	@echo "Modo archivos coincide con $(SAMPLES)/expected_output.txt"

# Benchmarks reproducibles -> results/.
bench: sentinel
	python3 bench/benchmark.py ./sentinel

# Digital Twin en http://127.0.0.1:8000
app: sentinel
	python3 app/server.py

# Regenera los datos sinteticos (semilla fija).
data:
	python3 app/data/generate_data.py

# ZIP para el autograder: exactamente un .cpp (src/main.cpp).
grader-zip:
	@mkdir -p dist && rm -f dist/sentinela_grader.zip
	zip -qr dist/sentinela_grader.zip src README.md team.txt test -x 'test/*.cpp' '*.DS_Store' '*__pycache__*'
	@unzip -l dist/sentinela_grader.zip | tail -1

# Paquete completo para Canvas (incluye app, pruebas, resultados y reflexion).
deliverable-zip:
	@mkdir -p dist && rm -f dist/sentinela_entrega.zip
	zip -qr dist/sentinela_entrega.zip src app test bench results README.md team.txt reflection.md Makefile -x '*.DS_Store' '*__pycache__*'
	@unzip -l dist/sentinela_entrega.zip | tail -1

clean:
	rm -rf sentinel build dist
