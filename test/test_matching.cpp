#include "../src/matching.h"

#include <cassert>
#include <iostream>
#include <random>
#include <string>
#include <vector>

using namespace sentinela;

// Oraculo: busqueda ingenua de la biblioteca estandar con la misma convencion (base 1, -1).
int findFirstNaive(const std::string& text, const std::string& pattern) {
    const std::size_t position = text.find(pattern);
    return position == std::string::npos ? -1 : static_cast<int>(position) + 1;
}

void checkBoth(const std::string& text, const std::string& pattern, int expected) {
    assert(findFirstKMP(text, pattern) == expected);
    assert(findFirstZ(text, pattern) == expected);
}

void testBasicPositions() {
    checkBoth("ABCDEF", "ABC", 1);
    checkBoth("ABCDEF", "DEF", 4);
    checkBoth("ABCDEF", "CD", 3);
    checkBoth("ABCDEF", "XYZ", -1);

    std::cout << "Caso 1: inicio, final, en medio y ausente OK\n";
}

void testRepeatedAndOverlapping() {
    checkBoth("AAAAAB", "AAB", 4);
    checkBoth("ABABABAC", "ABAC", 5);
    checkBoth("FFFFFFFF", "FFF", 1);

    std::cout << "Caso 2: patrones repetidos y traslapados reportan la primera aparicion OK\n";
}

void testEdgeCases() {
    checkBoth("ABC", "", 1);
    checkBoth("", "A", -1);
    checkBoth("AB", "ABC", -1);
    checkBoth("ABC", "ABC", 1);

    std::cout << "Caso 3: patron vacio, texto vacio y patron mas largo OK\n";
}

void testMatchAll() {
    const auto result = matchAll(
        {"AAABCDFFF", "001234ABCDEF"},
        {"ABCD", "1234", "FFFF"}
    );

    assert((result == std::vector<std::vector<int>>{{3, -1, -1}, {7, 3, -1}}));

    std::cout << "Caso 4: matriz de transmisiones por firmas OK\n";
}

void testRandomAgainstNaive() {
    std::mt19937 rng(2038);
    const std::string alphabets[] = {"AB", "0123456789ABCDEF"};

    for (int round = 0; round < 2000; ++round) {
        const std::string& alphabet = alphabets[round % 2];
        std::string text;
        std::string pattern;
        const int textLength = static_cast<int>(rng() % 60);
        const int patternLength = 1 + static_cast<int>(rng() % 6);

        for (int i = 0; i < textLength; ++i) {
            text.push_back(alphabet[rng() % alphabet.size()]);
        }
        for (int i = 0; i < patternLength; ++i) {
            pattern.push_back(alphabet[rng() % alphabet.size()]);
        }

        checkBoth(text, pattern, findFirstNaive(text, pattern));
    }

    std::cout << "Caso 5: KMP y Z coinciden con busqueda ingenua en 2000 casos OK\n";
}

int main() {
    testBasicPositions();
    testRepeatedAndOverlapping();
    testEdgeCases();
    testMatchAll();
    testRandomAgainstNaive();

    std::cout << "Todas las pruebas de Matching pasaron.\n";
    return 0;
}
