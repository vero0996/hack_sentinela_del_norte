# Ambiente de los benchmarks

- **fecha**: 2026-10-21T19:33:08
- **sistema**: Darwin 27.0.0 (arm64)
- **cpu**: Apple M4
- **memoria**: 24 GiB
- **compilador**: Apple clang version 21.0.0 (clang-2100.3.34.2)
- **opciones**: -std=c++17 -O2
- **python**: 3.15.0
- **binario**: /Users/luis/programacion/hack_sentinela_del_norte/sentinel
- **repeticiones**: 3
- **semilla**: 2038

## Procedimiento

```bash
c++ -std=c++17 -O2 src/main.cpp -o sentinel
python3 bench/benchmark.py ./sentinel
```

Las instancias se generan con semilla fija, asi que el procedimiento es reproducible; los tiempos absolutos dependen del equipo.
