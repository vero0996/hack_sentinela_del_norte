#include "../src/common_substring.h"

#include <cassert>
#include <iostream>
#include <random>
#include <string>
#include <utility>

using namespace sentinela;

using Range = std::pair<int, int>;

void checkBoth(const std::string& first, const std::string& second, Range expected) {
    assert(longestCommonSubstring(first, second) == expected);
    assert(longestCommonSubstringDP(first, second) == expected);
}

void testBasic() {
    checkBoth("AA1234BBB", "DD1234EE", {3, 6});
    checkBoth("ABCDEF", "ABCDEF", {1, 6});
    checkBoth("AB", "CD", {0, 0});
    checkBoth("", "ABC", {0, 0});
    checkBoth("ABC", "", {0, 0});

    std::cout << "Caso 1: coincidencia, cadenas iguales y sin caracteres comunes OK\n";
}

void testTiesUseLowestStartInFirst() {
    checkBoth("ABCXXCBA", "ZZABCYYCBA", {1, 3});
    checkBoth("CBA00ABC", "ABC11CBA", {1, 3});
    checkBoth("12AB", "AB12", {1, 2});

    std::cout << "Caso 2: empates resueltos con la menor posicion en la primera OK\n";
}

void testSubstringNotSubsequence() {
    // La subsecuencia comun mas larga seria "ABCD" (4), pero no es contigua en la segunda.
    checkBoth("ABCD", "AXBXCXD", {1, 1});

    std::cout << "Caso 3: subcadena contigua, no subsecuencia OK\n";
}

void testRandomAgainstDP() {
    std::mt19937 rng(2038);
    const std::string alphabets[] = {"AB", "ABC", "0123456789ABCDEF", "XYZ0"};

    for (int round = 0; round < 3000; ++round) {
        const std::string& alphabet = alphabets[round % 4];
        std::string first;
        std::string second;
        const int firstLength = static_cast<int>(rng() % 40);
        const int secondLength = static_cast<int>(rng() % 40);

        for (int i = 0; i < firstLength; ++i) {
            first.push_back(alphabet[rng() % alphabet.size()]);
        }
        for (int i = 0; i < secondLength; ++i) {
            second.push_back(alphabet[rng() % alphabet.size()]);
        }

        assert(longestCommonSubstring(first, second) == longestCommonSubstringDP(first, second));
    }

    std::cout << "Caso 4: automata de sufijos coincide con DP en 3000 casos OK\n";
}

void testLargeShared() {
    std::mt19937 rng(7);
    const std::string hex = "0123456789ABCDEF";
    auto randomHex = [&](int length) {
        std::string text;
        for (int i = 0; i < length; ++i) {
            text.push_back(hex[rng() % 16]);
        }
        return text;
    };

    const std::string shared = randomHex(22500);
    const std::string first = randomHex(90000) + shared + randomHex(90000);
    const std::string second = randomHex(90000) + shared + randomHex(90000);
    const Range range = longestCommonSubstring(first, second);
    const int length = range.second - range.first + 1;

    assert(length >= static_cast<int>(shared.size()));
    assert(second.find(first.substr(range.first - 1, length)) != std::string::npos);

    std::cout << "Caso 5: transmisiones de 202500 caracteres en tiempo lineal OK\n";
}

int main() {
    testBasic();
    testTiesUseLowestStartInFirst();
    testSubstringNotSubsequence();
    testRandomAgainstDP();
    testLargeShared();

    std::cout << "Todas las pruebas de Subcadena Comun pasaron.\n";
    return 0;
}
