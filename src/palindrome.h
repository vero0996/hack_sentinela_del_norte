#ifndef PALINDROME_H
#define PALINDROME_H

#include <algorithm>
#include <cstddef>
#include <string>
#include <utility>
#include <vector>

namespace sentinela {

namespace detail {

// Actualiza el mejor palindromo: mas largo gana; en empate, el de inicio menor.
inline void keepBestPalindrome(int start, int length, int& bestStart, int& bestLength) {
	if (length > bestLength || (length == bestLength && start < bestStart)) {
		bestStart = start;
		bestLength = length;
	}
}

// Convierte (inicio en base 0, longitud) al rango [inicio, fin] en base 1; {0, 0} si esta vacio.
inline std::pair<int, int> toRange(int start, int length) {
	if (length == 0) {
		return {0, 0};
	}
	return {start + 1, start + length};
}

}  // namespace detail

// Palindromo contiguo mas largo con Manacher. O(n) tiempo y memoria.
// odd[i]: radio del palindromo impar centrado en i (incluye el centro).
// even[i]: radio del palindromo par centrado entre i - 1 e i.
// Regresa [inicio, fin] en base 1; en empate gana el inicio menor; {0, 0} si s esta vacia.
inline std::pair<int, int> longestPalindromeManacher(const std::string& s) {
	int n = static_cast<int>(s.size());
	std::vector<int> odd(s.size(), 0);
	std::vector<int> even(s.size(), 0);
	int bestStart = 0;
	int bestLength = 0;

	int left = 0;
	int right = -1;
	for (int i = 0; i < n; ++i) {
		int radius = i > right ? 1 : std::min(odd[left + right - i], right - i + 1);
		while (i - radius >= 0 && i + radius < n && s[i - radius] == s[i + radius]) {
			++radius;
		}
		odd[i] = radius;
		if (i + radius - 1 > right) {
			left = i - radius + 1;
			right = i + radius - 1;
		}
		detail::keepBestPalindrome(i - radius + 1, 2 * radius - 1, bestStart, bestLength);
	}

	left = 0;
	right = -1;
	for (int i = 0; i < n; ++i) {
		int radius = i > right ? 0 : std::min(even[left + right - i + 1], right - i + 1);
		while (i - radius - 1 >= 0 && i + radius < n && s[i - radius - 1] == s[i + radius]) {
			++radius;
		}
		even[i] = radius;
		if (i + radius - 1 > right) {
			left = i - radius;
			right = i + radius - 1;
		}
		detail::keepBestPalindrome(i - radius, 2 * radius, bestStart, bestLength);
	}

	return detail::toRange(bestStart, bestLength);
}

// Tecnica de comparacion: expansion desde cada uno de los 2n - 1 centros.
// O(n^2) tiempo en el peor caso, O(1) memoria adicional. Mismas reglas de empate.
inline std::pair<int, int> longestPalindromeExpand(const std::string& s) {
	int n = static_cast<int>(s.size());
	int bestStart = 0;
	int bestLength = 0;
	for (int center = 0; center < 2 * n - 1; ++center) {
		int left = center / 2;
		int right = left + center % 2;
		while (left >= 0 && right < n && s[left] == s[right]) {
			--left;
			++right;
		}
		detail::keepBestPalindrome(left + 1, right - left - 1, bestStart, bestLength);
	}
	return detail::toRange(bestStart, bestLength);
}

}  // namespace sentinela

#endif  // PALINDROME_H
