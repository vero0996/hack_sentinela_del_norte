#ifndef COMMON_SUBSTRING_H
#define COMMON_SUBSTRING_H

#include <array>
#include <cstddef>
#include <string>
#include <utility>
#include <vector>

namespace sentinela {

// Automata de sufijos: reconoce exactamente las subcadenas del texto con el que se construye.
// Las transiciones se guardan en una tabla plana de estados x sigma, donde sigma es el numero
// de caracteres distintos del texto (16 para transmisiones hexadecimales).
class SuffixAutomaton {
public:
	// Construye el automata agregando un caracter a la vez. O(n * sigma) tiempo y memoria.
	explicit SuffixAutomaton(const std::string& text) {
		symbolIndex_.fill(-1);
		for (char symbol : text) {
			unsigned char key = static_cast<unsigned char>(symbol);
			if (symbolIndex_[key] == -1) {
				symbolIndex_[key] = alphabetSize_;
				++alphabetSize_;
			}
		}
		std::size_t capacity = 2 * text.size() + 1;
		length_.reserve(capacity);
		link_.reserve(capacity);
		next_.reserve(capacity * static_cast<std::size_t>(alphabetSize_));
		addState(0, -1);
		for (char symbol : text) {
			extend(symbolIndex_[static_cast<unsigned char>(symbol)]);
		}
	}

	// Para cada posicion de query calcula la subcadena mas larga que termina ahi y aparece
	// en el texto. Regresa {inicio en base 0, longitud} de la mayor; en empate, inicio menor.
	std::pair<int, int> longestMatchIn(const std::string& query) const {
		int state = 0;
		int length = 0;
		int bestEnd = -1;
		int bestLength = 0;
		for (std::size_t i = 0; i < query.size(); ++i) {
			int symbol = symbolIndex_[static_cast<unsigned char>(query[i])];
			if (symbol == -1) {
				state = 0;
				length = 0;
			} else {
				while (state != 0 && transition(state, symbol) == -1) {
					state = link_[state];
					length = length_[state];
				}
				if (transition(state, symbol) != -1) {
					state = transition(state, symbol);
					++length;
				}
			}
			if (length > bestLength) {
				bestLength = length;
				bestEnd = static_cast<int>(i);
			}
		}
		return {bestEnd - bestLength + 1, bestLength};
	}

private:
	std::array<int, 256> symbolIndex_;
	int alphabetSize_ = 0;
	std::vector<int> length_;
	std::vector<int> link_;
	std::vector<int> next_;
	int last_ = 0;

	// Transicion del estado con el simbolo, o -1 si no existe.
	int transition(int state, int symbol) const {
		return next_[static_cast<std::size_t>(state) * alphabetSize_ + symbol];
	}

	// Asigna la transicion del estado con el simbolo.
	void setTransition(int state, int symbol, int target) {
		next_[static_cast<std::size_t>(state) * alphabetSize_ + symbol] = target;
	}

	// Crea un estado sin transiciones y regresa su indice.
	int addState(int length, int link) {
		length_.push_back(length);
		link_.push_back(link);
		next_.insert(next_.end(), static_cast<std::size_t>(alphabetSize_), -1);
		return static_cast<int>(length_.size()) - 1;
	}

	// Crea una copia de q (mismas transiciones y enlace) con otra longitud.
	int cloneState(int q, int length) {
		int clone = addState(length, link_[q]);
		for (int symbol = 0; symbol < alphabetSize_; ++symbol) {
			setTransition(clone, symbol, transition(q, symbol));
		}
		return clone;
	}

	// Agrega el simbolo al final del texto reconocido (construccion en linea de Blumer et al.).
	void extend(int symbol) {
		int current = addState(length_[last_] + 1, 0);
		int p = last_;
		while (p != -1 && transition(p, symbol) == -1) {
			setTransition(p, symbol, current);
			p = link_[p];
		}
		if (p != -1) {
			int q = transition(p, symbol);
			if (length_[p] + 1 == length_[q]) {
				link_[current] = q;
			} else {
				int clone = cloneState(q, length_[p] + 1);
				while (p != -1 && transition(p, symbol) == q) {
					setTransition(p, symbol, clone);
					p = link_[p];
				}
				link_[q] = clone;
				link_[current] = clone;
			}
		}
		last_ = current;
	}
};

// Subcadena comun mas larga (contigua) con automata de sufijos sobre second. O((n + m) * sigma).
// Regresa [inicio, fin] en base 1 dentro de first; en empate gana el inicio menor en first;
// {0, 0} si no comparten ningun caracter.
inline std::pair<int, int> longestCommonSubstring(const std::string& first, const std::string& second) {
	SuffixAutomaton automaton(second);
	std::pair<int, int> match = automaton.longestMatchIn(first);
	if (match.second == 0) {
		return {0, 0};
	}
	return {match.first + 1, match.first + match.second};
}

// Tecnica de comparacion: programacion dinamica con dos filas. O(n * m) tiempo, O(m) memoria.
// dp[i][j] = longitud del sufijo comun de first[0..i) y second[0..j).
// Empates: inicio menor en first y, si persiste, inicio menor en second.
inline std::pair<int, int> longestCommonSubstringDP(const std::string& first, const std::string& second) {
	std::vector<int> previous(second.size() + 1, 0);
	std::vector<int> current(second.size() + 1, 0);
	int bestLength = 0;
	int bestStart = 0;
	int bestSecondStart = 0;
	for (std::size_t i = 1; i <= first.size(); ++i) {
		for (std::size_t j = 1; j <= second.size(); ++j) {
			current[j] = first[i - 1] == second[j - 1] ? previous[j - 1] + 1 : 0;
			int length = current[j];
			int start = static_cast<int>(i) - length;
			int secondStart = static_cast<int>(j) - length;
			bool better = length > bestLength || (length == bestLength && length > 0 &&
				(start < bestStart || (start == bestStart && secondStart < bestSecondStart)));
			if (better) {
				bestLength = length;
				bestStart = start;
				bestSecondStart = secondStart;
			}
		}
		std::swap(previous, current);
	}
	if (bestLength == 0) {
		return {0, 0};
	}
	return {bestStart + 1, bestStart + bestLength};
}

}  // namespace sentinela

#endif  // COMMON_SUBSTRING_H
