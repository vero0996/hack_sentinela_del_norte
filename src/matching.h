#ifndef MATCHING_H
#define MATCHING_H

#include <algorithm>
#include <cstddef>
#include <string>
#include <vector>

namespace sentinela {

// Funcion de prefijo de KMP: pi[i] es el borde propio mas largo de pattern[0..i].
inline std::vector<int> prefixFunction(const std::string& pattern) {
	std::vector<int> pi(pattern.size(), 0);
	for (std::size_t i = 1; i < pattern.size(); ++i) {
		int k = pi[i - 1];
		while (k > 0 && pattern[i] != pattern[k]) {
			k = pi[k - 1];
		}
		if (pattern[i] == pattern[k]) {
			++k;
		}
		pi[i] = k;
	}
	return pi;
}

// Primera aparicion de pattern en text con Knuth-Morris-Pratt. O(n + m) tiempo, O(m) memoria.
// Regresa la posicion inicial en base 1, o -1 si no aparece. Un patron vacio aparece en 1.
inline int findFirstKMP(const std::string& text, const std::string& pattern) {
	if (pattern.empty()) {
		return 1;
	}
	std::vector<int> pi = prefixFunction(pattern);
	int found = -1;
	std::size_t matched = 0;
	for (std::size_t i = 0; i < text.size() && found == -1; ++i) {
		while (matched > 0 && text[i] != pattern[matched]) {
			matched = static_cast<std::size_t>(pi[matched - 1]);
		}
		if (text[i] == pattern[matched]) {
			++matched;
		}
		if (matched == pattern.size()) {
			found = static_cast<int>(i + 2 - pattern.size());
		}
	}
	return found;
}

// Funcion Z: z[i] es el prefijo comun mas largo entre s y s[i..].
inline std::vector<int> zFunction(const std::string& s) {
	int n = static_cast<int>(s.size());
	std::vector<int> z(s.size(), 0);
	int left = 0;
	int right = 0;
	for (int i = 1; i < n; ++i) {
		if (i < right) {
			z[i] = std::min(right - i, z[i - left]);
		}
		while (i + z[i] < n && s[z[i]] == s[i + z[i]]) {
			++z[i];
		}
		if (i + z[i] > right) {
			left = i;
			right = i + z[i];
		}
	}
	if (n > 0) {
		z[0] = n;
	}
	return z;
}

// Tecnica de comparacion: algoritmo Z sobre pattern + text. O(n + m) tiempo y memoria.
// No necesita separador: z[i] >= m con i >= m implica una aparicion completa dentro de text.
inline int findFirstZ(const std::string& text, const std::string& pattern) {
	if (pattern.empty()) {
		return 1;
	}
	std::vector<int> z = zFunction(pattern + text);
	std::size_t m = pattern.size();
	int found = -1;
	for (std::size_t i = m; i < z.size() && found == -1; ++i) {
		if (static_cast<std::size_t>(z[i]) >= m) {
			found = static_cast<int>(i - m + 1);
		}
	}
	return found;
}

// Matriz de resultados: result[t][p] es la primera posicion de patterns[p] en transmissions[t].
// useZ elige el algoritmo Z en lugar de KMP (tecnica de comparacion).
inline std::vector<std::vector<int>> matchAll(
	const std::vector<std::string>& transmissions,
	const std::vector<std::string>& patterns,
	bool useZ = false
) {
	std::vector<std::vector<int>> result;
	for (const std::string& text : transmissions) {
		std::vector<int> row;
		for (const std::string& pattern : patterns) {
			row.push_back(useZ ? findFirstZ(text, pattern) : findFirstKMP(text, pattern));
		}
		result.push_back(row);
	}
	return result;
}

}  // namespace sentinela

#endif  // MATCHING_H
