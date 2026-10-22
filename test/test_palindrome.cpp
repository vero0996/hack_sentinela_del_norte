#include "../src/palindrome.h"

#include <cassert>
#include <iostream>
#include <random>
#include <string>
#include <utility>

using namespace sentinela;

using Range = std::pair<int, int>;

void checkBoth(const std::string& text, Range expected) {
    assert(longestPalindromeManacher(text) == expected);
    assert(longestPalindromeExpand(text) == expected);
}

void testOddAndEven() {
    checkBoth("AABCDCBAE", {2, 8});
    checkBoth("ABCBA123321", {6, 11});
    checkBoth("XABBAY", {2, 5});

    std::cout << "Caso 1: palindromos de longitud impar y par OK\n";
}

void testWholeAndSingle() {
    checkBoth("AAAABAAAA", {1, 9});
    checkBoth("A", {1, 1});
    checkBoth("", {0, 0});

    std::cout << "Caso 2: cadena completa, un caracter y cadena vacia OK\n";
}

void testTiesUseLowestStart() {
    checkBoth("ABCDEF", {1, 1});
    checkBoth("ABA12321", {4, 8});
    checkBoth("ABACDC", {1, 3});
    checkBoth("AA0BB", {1, 2});

    std::cout << "Caso 3: empates resueltos con la menor posicion inicial OK\n";
}

void testRandomAgainstExpansion() {
    std::mt19937 rng(2038);
    const std::string alphabets[] = {"AB", "ABC", "0123456789ABCDEF"};

    for (int round = 0; round < 3000; ++round) {
        const std::string& alphabet = alphabets[round % 3];
        const int length = static_cast<int>(rng() % 50);
        std::string text;

        for (int i = 0; i < length; ++i) {
            text.push_back(alphabet[rng() % alphabet.size()]);
        }

        assert(longestPalindromeManacher(text) == longestPalindromeExpand(text));
    }

    std::cout << "Caso 4: Manacher coincide con expansion desde centro en 3000 casos OK\n";
}

void testLargePalindrome() {
    std::mt19937 rng(7);
    const std::string hex = "0123456789ABCDEF";
    std::string half;

    for (int i = 0; i < 250000; ++i) {
        half.push_back(hex[rng() % 16]);
    }

    const std::string palindrome = half + "A" + std::string(half.rbegin(), half.rend());
    const std::string text = "0123" + palindrome + "456";
    const Range range = longestPalindromeManacher(text);

    assert(range.second - range.first + 1 >= static_cast<int>(palindrome.size()));
    assert(range.first <= 5);

    std::cout << "Caso 5: palindromo de 500001 caracteres en tiempo lineal OK\n";
}

int main() {
    testOddAndEven();
    testWholeAndSingle();
    testTiesUseLowestStart();
    testRandomAgainstExpansion();
    testLargePalindrome();

    std::cout << "Todas las pruebas de Palindromo pasaron.\n";
    return 0;
}
