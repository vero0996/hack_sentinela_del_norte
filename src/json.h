#ifndef JSON_H
#define JSON_H

#include <cmath>
#include <cstddef>
#include <cstdlib>
#include <iomanip>
#include <map>
#include <sstream>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace sentinela {

// Valor JSON generico: null, booleano, numero, cadena, arreglo u objeto.
class JsonValue {
public:
	enum class Type { Null, Bool, Number, String, Array, Object };

	JsonValue() : type_(Type::Null) {}

	// Construye un valor booleano.
	static JsonValue makeBool(bool value) {
		JsonValue result;
		result.type_ = Type::Bool;
		result.bool_ = value;
		return result;
	}

	// Construye un valor numerico.
	static JsonValue makeNumber(double value) {
		JsonValue result;
		result.type_ = Type::Number;
		result.number_ = value;
		return result;
	}

	// Construye un valor de cadena.
	static JsonValue makeString(std::string value) {
		JsonValue result;
		result.type_ = Type::String;
		result.string_ = std::move(value);
		return result;
	}

	// Construye un arreglo con los elementos dados.
	static JsonValue makeArray(std::vector<JsonValue> items) {
		JsonValue result;
		result.type_ = Type::Array;
		result.array_ = std::move(items);
		return result;
	}

	// Construye un objeto con los miembros dados.
	static JsonValue makeObject(std::map<std::string, JsonValue> members) {
		JsonValue result;
		result.type_ = Type::Object;
		result.object_ = std::move(members);
		return result;
	}

	Type type() const { return type_; }

	// Indica si el objeto contiene la llave key.
	bool has(const std::string& key) const {
		return type_ == Type::Object && object_.count(key) > 0;
	}

	// Miembro key de un objeto; lanza si no existe.
	const JsonValue& at(const std::string& key) const {
		expect(Type::Object, "objeto");
		auto found = object_.find(key);
		if (found == object_.end()) {
			throw std::runtime_error("Falta la llave '" + key + "'");
		}
		return found->second;
	}

	// Elemento index de un arreglo; lanza si esta fuera de rango.
	const JsonValue& at(std::size_t index) const {
		expect(Type::Array, "arreglo");
		if (index >= array_.size()) {
			throw std::runtime_error("Indice de arreglo fuera de rango");
		}
		return array_[index];
	}

	// Numero de elementos de un arreglo.
	std::size_t size() const {
		expect(Type::Array, "arreglo");
		return array_.size();
	}

	// Valor numerico como double.
	double asNumber() const {
		expect(Type::Number, "numero");
		return number_;
	}

	// Valor numerico como entero; lanza si tiene parte fraccionaria.
	long long asInt() const {
		double value = asNumber();
		if (std::floor(value) != value) {
			throw std::runtime_error("Se esperaba un entero");
		}
		return static_cast<long long>(value);
	}

	// Valor booleano.
	bool asBool() const {
		expect(Type::Bool, "booleano");
		return bool_;
	}

	// Valor de cadena.
	const std::string& asString() const {
		expect(Type::String, "cadena");
		return string_;
	}

	// Elementos de un arreglo.
	const std::vector<JsonValue>& asArray() const {
		expect(Type::Array, "arreglo");
		return array_;
	}

	// Miembros de un objeto.
	const std::map<std::string, JsonValue>& asObject() const {
		expect(Type::Object, "objeto");
		return object_;
	}

private:
	Type type_;
	bool bool_ = false;
	double number_ = 0.0;
	std::string string_;
	std::vector<JsonValue> array_;
	std::map<std::string, JsonValue> object_;

	// Lanza si el valor no es del tipo esperado.
	void expect(Type expected, const char* name) const {
		if (type_ != expected) {
			throw std::runtime_error(std::string("Se esperaba un ") + name + " en el JSON");
		}
	}
};

namespace detail {

// Analizador descendente recursivo de JSON (RFC 8259, sin extensiones).
class JsonParser {
public:
	explicit JsonParser(const std::string& text) : text_(text), pos_(0) {}

	// Analiza el documento completo; lanza si sobra texto.
	JsonValue parseDocument() {
		JsonValue value = parseValue();
		skipSpaces();
		if (pos_ != text_.size()) {
			fail("Texto sobrante despues del valor JSON");
		}
		return value;
	}

private:
	const std::string& text_;
	std::size_t pos_;

	// Lanza un error indicando la posicion actual.
	[[noreturn]] void fail(const std::string& message) const {
		throw std::runtime_error(message + " (posicion " + std::to_string(pos_) + ")");
	}

	// Avanza sobre espacios en blanco.
	void skipSpaces() {
		while (pos_ < text_.size() && (text_[pos_] == ' ' || text_[pos_] == '\n' ||
			text_[pos_] == '\r' || text_[pos_] == '\t')) {
			++pos_;
		}
	}

	// Consume el caracter esperado o lanza.
	void consume(char expected) {
		skipSpaces();
		if (pos_ >= text_.size() || text_[pos_] != expected) {
			fail(std::string("Se esperaba '") + expected + "'");
		}
		++pos_;
	}

	// Consume una palabra literal (true, false, null) o lanza.
	void consumeWord(const char* word) {
		std::string expected(word);
		if (text_.compare(pos_, expected.size(), expected) != 0) {
			fail("Literal JSON invalido");
		}
		pos_ += expected.size();
	}

	// Siguiente caracter sin consumirlo, o '\0' al final.
	char peek() {
		skipSpaces();
		return pos_ < text_.size() ? text_[pos_] : '\0';
	}

	// Analiza cualquier valor segun su primer caracter.
	JsonValue parseValue() {
		char next = peek();
		JsonValue value;
		if (next == '{') {
			value = parseObject();
		} else if (next == '[') {
			value = parseArray();
		} else if (next == '"') {
			value = JsonValue::makeString(parseString());
		} else if (next == 't') {
			consumeWord("true");
			value = JsonValue::makeBool(true);
		} else if (next == 'f') {
			consumeWord("false");
			value = JsonValue::makeBool(false);
		} else if (next == 'n') {
			consumeWord("null");
		} else {
			value = JsonValue::makeNumber(parseNumber());
		}
		return value;
	}

	// Analiza un objeto {"llave": valor, ...}.
	JsonValue parseObject() {
		consume('{');
		std::map<std::string, JsonValue> members;
		bool more = peek() != '}';
		while (more) {
			if (peek() != '"') {
				fail("Se esperaba una llave de cadena");
			}
			std::string key = parseString();
			consume(':');
			members[key] = parseValue();
			more = peek() == ',';
			if (more) {
				consume(',');
			}
		}
		consume('}');
		return JsonValue::makeObject(std::move(members));
	}

	// Analiza un arreglo [valor, ...].
	JsonValue parseArray() {
		consume('[');
		std::vector<JsonValue> items;
		bool more = peek() != ']';
		while (more) {
			items.push_back(parseValue());
			more = peek() == ',';
			if (more) {
				consume(',');
			}
		}
		consume(']');
		return JsonValue::makeArray(std::move(items));
	}

	// Analiza una cadena con escapes; \u se acepta solo para ASCII.
	std::string parseString() {
		consume('"');
		std::string result;
		bool closed = false;
		while (!closed) {
			if (pos_ >= text_.size()) {
				fail("Cadena sin cerrar");
			}
			char current = text_[pos_++];
			if (current == '"') {
				closed = true;
			} else if (current == '\\') {
				result.push_back(parseEscape());
			} else {
				result.push_back(current);
			}
		}
		return result;
	}

	// Traduce la secuencia de escape que sigue a una diagonal invertida.
	char parseEscape() {
		if (pos_ >= text_.size()) {
			fail("Escape incompleto");
		}
		char code = text_[pos_++];
		char result = code;
		if (code == 'n') {
			result = '\n';
		} else if (code == 't') {
			result = '\t';
		} else if (code == 'r') {
			result = '\r';
		} else if (code == 'b') {
			result = '\b';
		} else if (code == 'f') {
			result = '\f';
		} else if (code == 'u') {
			if (pos_ + 4 > text_.size()) {
				fail("Escape unicode incompleto");
			}
			long value = std::strtol(text_.substr(pos_, 4).c_str(), nullptr, 16);
			pos_ += 4;
			result = value < 128 ? static_cast<char>(value) : '?';
		} else if (code != '"' && code != '\\' && code != '/') {
			fail("Escape invalido");
		}
		return result;
	}

	// Analiza un numero con strtod a partir de la posicion actual.
	double parseNumber() {
		skipSpaces();
		const char* start = text_.c_str() + pos_;
		char* end = nullptr;
		double value = std::strtod(start, &end);
		if (end == start) {
			fail("Valor JSON invalido");
		}
		pos_ += static_cast<std::size_t>(end - start);
		return value;
	}
};

}  // namespace detail

// Convierte texto JSON en un JsonValue; lanza std::runtime_error si es invalido.
inline JsonValue parseJson(const std::string& text) {
	detail::JsonParser parser(text);
	return parser.parseDocument();
}

// Formatea un numero para JSON: enteros sin decimales, reales con 17 cifras.
inline std::string formatJsonNumber(double value) {
	if (!std::isfinite(value)) {
		throw std::runtime_error("No se puede escribir un numero no finito en JSON");
	}
	std::ostringstream out;
	if (std::floor(value) == value && std::fabs(value) < 1e15) {
		out << static_cast<long long>(value);
	} else {
		out << std::setprecision(17) << value;
	}
	return out.str();
}

// Escapa una cadena y la regresa entre comillas.
inline std::string formatJsonString(const std::string& value) {
	std::string out = "\"";
	for (char current : value) {
		if (current == '"' || current == '\\') {
			out.push_back('\\');
			out.push_back(current);
		} else if (current == '\n') {
			out += "\\n";
		} else if (static_cast<unsigned char>(current) < 0x20) {
			out.push_back(' ');
		} else {
			out.push_back(current);
		}
	}
	out.push_back('"');
	return out;
}

}  // namespace sentinela

#endif  // JSON_H
