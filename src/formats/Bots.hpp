#pragma once

#include "Slc.hpp"

#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <string>
#include <utility>
#include <vector>

namespace xbot::bots {

struct Click {
	uint64_t frame = 0;
	int button = 1;
	bool down = false;
	bool player2 = false;
};

struct Replay {
	double fps = 240.0;
	std::vector<Click> clicks;
};

namespace detail {

struct Json {
	enum class Type { Null, Bool, Number, String, Array, Object };

	Type type = Type::Null;
	bool boolean = false;
	double number = 0.0;
	std::string string;
	std::vector<Json> array;
	std::vector<std::pair<std::string, Json>> object;

	Json const* find(std::string const& key) const {
		if (type != Type::Object) return nullptr;
		for (auto const& entry : object) {
			if (entry.first == key) return &entry.second;
		}
		return nullptr;
	}

	bool isNumber() const { return type == Type::Number; }
	bool isBool() const { return type == Type::Bool; }
	bool isArray() const { return type == Type::Array; }
	bool isObject() const { return type == Type::Object; }
};

class JsonParser {
public:
	explicit JsonParser(std::string const& text) : m_text(text) {}

	bool parse(Json& out) {
		skipSpace();
		if (!value(out, 0)) return false;
		skipSpace();
		return m_pos == m_text.size();
	}

private:
	std::string const& m_text;
	size_t m_pos = 0;

	void skipSpace() {
		while (m_pos < m_text.size()) {
			char c = m_text[m_pos];
			if (c == ' ' || c == '\t' || c == '\n' || c == '\r') m_pos++;
			else break;
		}
	}

	bool literal(char const* word) {
		size_t length = std::strlen(word);
		if (m_text.compare(m_pos, length, word) != 0) return false;
		m_pos += length;
		return true;
	}

	bool value(Json& out, int depth) {
		if (depth > 64 || m_pos >= m_text.size()) return false;
		char c = m_text[m_pos];

		if (c == '{') return object(out, depth);
		if (c == '[') return array(out, depth);
		if (c == '"') {
			out.type = Json::Type::String;
			return string(out.string);
		}
		if (c == 't') {
			out.type = Json::Type::Bool;
			out.boolean = true;
			return literal("true");
		}
		if (c == 'f') {
			out.type = Json::Type::Bool;
			out.boolean = false;
			return literal("false");
		}
		if (c == 'n') {
			out.type = Json::Type::Null;
			return literal("null");
		}
		return number(out);
	}

	bool number(Json& out) {
		size_t start = m_pos;
		while (m_pos < m_text.size()) {
			char c = m_text[m_pos];
			if ((c >= '0' && c <= '9') || c == '-' || c == '+' || c == '.' || c == 'e' || c == 'E') m_pos++;
			else break;
		}
		if (m_pos == start) return false;
		std::string token = m_text.substr(start, m_pos - start);
		char* end = nullptr;
		out.number = std::strtod(token.c_str(), &end);
		if (end == token.c_str()) return false;
		out.type = Json::Type::Number;
		return true;
	}

	static void appendUtf8(std::string& out, uint32_t code) {
		if (code < 0x80) {
			out += static_cast<char>(code);
		}
		else if (code < 0x800) {
			out += static_cast<char>(0xC0 | (code >> 6));
			out += static_cast<char>(0x80 | (code & 0x3F));
		}
		else {
			out += static_cast<char>(0xE0 | (code >> 12));
			out += static_cast<char>(0x80 | ((code >> 6) & 0x3F));
			out += static_cast<char>(0x80 | (code & 0x3F));
		}
	}

	bool string(std::string& out) {
		if (m_text[m_pos] != '"') return false;
		m_pos++;
		out.clear();
		while (m_pos < m_text.size()) {
			char c = m_text[m_pos++];
			if (c == '"') return true;
			if (c != '\\') {
				out += c;
				continue;
			}
			if (m_pos >= m_text.size()) return false;
			char escape = m_text[m_pos++];
			switch (escape) {
				case '"': out += '"'; break;
				case '\\': out += '\\'; break;
				case '/': out += '/'; break;
				case 'b': out += '\b'; break;
				case 'f': out += '\f'; break;
				case 'n': out += '\n'; break;
				case 'r': out += '\r'; break;
				case 't': out += '\t'; break;
				case 'u': {
					if (m_pos + 4 > m_text.size()) return false;
					uint32_t code = 0;
					for (int i = 0; i < 4; i++) {
						char h = m_text[m_pos++];
						code <<= 4;
						if (h >= '0' && h <= '9') code |= static_cast<uint32_t>(h - '0');
						else if (h >= 'a' && h <= 'f') code |= static_cast<uint32_t>(h - 'a' + 10);
						else if (h >= 'A' && h <= 'F') code |= static_cast<uint32_t>(h - 'A' + 10);
						else return false;
					}
					appendUtf8(out, code);
					break;
				}
				default: return false;
			}
		}
		return false;
	}

	bool array(Json& out, int depth) {
		out.type = Json::Type::Array;
		m_pos++;
		skipSpace();
		if (m_pos < m_text.size() && m_text[m_pos] == ']') {
			m_pos++;
			return true;
		}
		while (true) {
			skipSpace();
			Json item;
			if (!value(item, depth + 1)) return false;
			out.array.push_back(std::move(item));
			skipSpace();
			if (m_pos >= m_text.size()) return false;
			char c = m_text[m_pos++];
			if (c == ']') return true;
			if (c != ',') return false;
		}
	}

	bool object(Json& out, int depth) {
		out.type = Json::Type::Object;
		m_pos++;
		skipSpace();
		if (m_pos < m_text.size() && m_text[m_pos] == '}') {
			m_pos++;
			return true;
		}
		while (true) {
			skipSpace();
			std::string key;
			if (m_pos >= m_text.size() || !string(key)) return false;
			skipSpace();
			if (m_pos >= m_text.size() || m_text[m_pos++] != ':') return false;
			skipSpace();
			Json item;
			if (!value(item, depth + 1)) return false;
			out.object.emplace_back(std::move(key), std::move(item));
			skipSpace();
			if (m_pos >= m_text.size()) return false;
			char c = m_text[m_pos++];
			if (c == '}') return true;
			if (c != ',') return false;
		}
	}
};

inline bool readFloat(slc::detail::Reader& reader, double& out) {
	uint64_t bits;
	if (!reader.unsignedValue(bits, 4)) return false;
	uint32_t narrow = static_cast<uint32_t>(bits);
	float value;
	std::memcpy(&value, &narrow, sizeof(value));
	out = value;
	return true;
}

inline bool validFrame(double frame) {
	return frame >= 0.0 && frame < 4294967295.0;
}

inline bool startsWith(std::vector<uint8_t> const& data, char const* prefix, size_t length) {
	return data.size() >= length && std::memcmp(data.data(), prefix, length) == 0;
}

inline bool parseXd(std::string const& text, Replay& out) {
	out.clicks.clear();
	out.fps = 240.0;

	double multiplier = 1.0;
	size_t pos = 0;
	while (pos < text.size()) {
		size_t end = text.find('\n', pos);
		if (end == std::string::npos) end = text.size();
		std::string line = text.substr(pos, end - pos);
		pos = end + 1;
		if (!line.empty() && line.back() == '\r') line.pop_back();
		if (line.empty()) continue;

		std::vector<std::string> fields;
		size_t start = 0;
		while (true) {
			size_t bar = line.find('|', start);
			if (bar == std::string::npos) {
				fields.push_back(line.substr(start));
				break;
			}
			fields.push_back(line.substr(start, bar - start));
			start = bar + 1;
		}

		if (fields.size() < 4) {
			if (fields[0] == "android") {
				multiplier = 4.0;
			}
			else {
				double fps = std::strtod(fields[0].c_str(), nullptr);
				if (fps <= 0.0) return false;
				multiplier = 240.0 / fps;
			}
			continue;
		}

		double frame = std::round(std::strtod(fields[0].c_str(), nullptr) * multiplier);
		int button = std::atoi(fields[2].c_str());
		bool posOnly = fields.size() > 4 && fields[4] == "1";
		if (posOnly || button < 1 || button > 3 || !validFrame(frame)) continue;

		out.clicks.push_back({static_cast<uint64_t>(frame), button, fields[1] == "1", fields[3] == "1"});
	}
	return !out.clicks.empty();
}

inline bool parseMhrJson(Json const& root, Replay& out) {
	auto events = root.find("events");
	auto meta = root.find("meta");
	if (!events || !events->isArray() || !meta) return false;
	auto fps = meta->find("fps");
	if (!fps || !fps->isNumber() || fps->number <= 0.0) return false;

	out.fps = fps->number;
	out.clicks.clear();
	for (auto const& event : events->array) {
		auto frame = event.find("frame");
		auto down = event.find("down");
		if (!frame || !frame->isNumber() || !down || !down->isBool() || !validFrame(frame->number)) continue;
		auto player2 = event.find("p2");
		out.clicks.push_back({
			static_cast<uint64_t>(frame->number), 1, down->boolean, player2 && player2->isBool() && player2->boolean
		});
	}
	return !out.clicks.empty();
}

inline bool parseMhrBinary(std::vector<uint8_t> const& data, Replay& out) {
	static constexpr char header[] = {'H', 'A', 'C', 'K', 'P', 'R', 'O', 0x07};
	if (!startsWith(data, header, sizeof(header))) return false;

	slc::detail::Reader reader(data);
	uint64_t metaSize, fps, eventSize, eventCount;
	if (!reader.skip(8) || !reader.unsignedValue(metaSize, 4) || !reader.unsignedValue(fps, 4)) return false;
	if (metaSize < 4 || fps == 0 || fps > 0x7FFFFFFF || !reader.skip(metaSize - 4) || !reader.skip(8)) return false;
	if (!reader.unsignedValue(eventSize, 4) || !reader.unsignedValue(eventCount, 4)) return false;
	if (eventSize < 8 || eventCount > reader.left() / eventSize) return false;

	out.fps = static_cast<double>(fps);
	out.clicks.clear();
	std::vector<Click> others;
	for (uint64_t i = 0; i < eventCount; i++) {
		uint64_t type, down, player2, frame;
		if (!reader.unsignedValue(type, 2) || !reader.unsignedValue(down, 1) ||
			!reader.unsignedValue(player2, 1) || !reader.unsignedValue(frame, 4) || !reader.skip(eventSize - 8)) {
			return false;
		}
		if (frame > 0x7FFFFFFF) continue;
		Click click{frame, 1, down == 1, player2 == 1};
		(type == 1 ? out.clicks : others).push_back(click);
	}
	if (out.clicks.empty()) out.clicks = std::move(others);
	return !out.clicks.empty();
}

inline bool parseEchoBinary(std::vector<uint8_t> const& data, Replay& out) {
	static constexpr char header[] = {'M', 'E', 'T', 'A'};
	static constexpr char full[] = {'D', 'B', 'G', 0x00};
	if (!startsWith(data, header, sizeof(header)) || data.size() < 48) return false;

	size_t actionSize = std::memcmp(data.data() + 4, full, sizeof(full)) == 0 ? 34 : 6;

	slc::detail::Reader reader(data);
	if (!reader.skip(24)) return false;
	double fps;
	if (!readFloat(reader, fps) || fps <= 0.0) return false;
	if (!reader.skip(20)) return false;

	out.fps = std::round(fps);
	out.clicks.clear();
	while (reader.left() >= actionSize) {
		uint64_t frame, down, player2;
		if (!reader.unsignedValue(frame, 4) || !reader.unsignedValue(down, 1) ||
			!reader.unsignedValue(player2, 1) || !reader.skip(actionSize - 6)) {
			return false;
		}
		out.clicks.push_back({frame, 1, down == 1, player2 == 1});
	}
	return !out.clicks.empty();
}

inline bool parseEchoJson(Json const& root, Replay& out) {
	out.clicks.clear();

	if (auto inputs = root.find("inputs")) {
		auto fps = root.find("fps");
		if (!inputs->isArray() || !fps || !fps->isNumber() || fps->number <= 0.0) return false;
		out.fps = std::round(fps->number);
		for (auto const& input : inputs->array) {
			auto frame = input.find("frame");
			auto holding = input.find("holding");
			if (!frame || !frame->isNumber() || !holding || !holding->isBool() || !validFrame(frame->number)) continue;
			auto player2 = input.find("player_2");
			out.clicks.push_back({
				static_cast<uint64_t>(frame->number), 1, holding->boolean,
				player2 && player2->isBool() && player2->boolean
			});
		}
		return !out.clicks.empty();
	}

	if (auto clicks = root.find("Echo Replay")) {
		auto fps = root.find("FPS");
		auto start = root.find("Starting Frame");
		if (!clicks->isArray() || !fps || !fps->isNumber() || fps->number <= 0.0) return false;
		double offset = start && start->isNumber() ? start->number : 0.0;
		out.fps = std::round(fps->number);
		for (auto const& click : clicks->array) {
			auto frame = click.find("Frame");
			auto hold = click.find("Hold");
			if (!frame || !frame->isNumber() || !hold || !hold->isBool() || !validFrame(frame->number + offset)) continue;
			auto player2 = click.find("Player 2");
			out.clicks.push_back({
				static_cast<uint64_t>(frame->number + offset), 1, hold->boolean,
				player2 && player2->isBool() && player2->boolean
			});
		}
		return !out.clicks.empty();
	}
	return false;
}

inline bool parseTasbot(Json const& root, Replay& out) {
	auto fps = root.find("fps");
	auto macro = root.find("macro");
	if (!fps || !fps->isNumber() || fps->number <= 0.0 || !macro || !macro->isArray()) return false;

	out.fps = fps->number;
	out.clicks.clear();
	for (auto const& entry : macro->array) {
		auto frame = entry.find("frame");
		if (!frame || !frame->isNumber() || !validFrame(frame->number)) continue;

		for (int player = 0; player < 2; player++) {
			auto side = entry.find(player == 0 ? "player_1" : "player_2");
			auto click = side ? side->find("click") : nullptr;
			if (!click || !click->isNumber()) continue;
			int state = static_cast<int>(click->number);
			if (state != 1 && state != 2) continue;
			out.clicks.push_back({static_cast<uint64_t>(frame->number), 1, state == 1, player == 1});
		}
	}
	return !out.clicks.empty();
}

inline bool parseJsonFormats(std::string const& text, Replay& out) {
	Json root;
	if (!JsonParser(text).parse(root) || !root.isObject()) return false;

	if (root.find("events") && root.find("meta")) return parseMhrJson(root, out);
	if (root.find("inputs") || root.find("Echo Replay")) return parseEchoJson(root, out);
	if (root.find("macro")) return parseTasbot(root, out);
	return false;
}

}

inline bool parse(std::vector<uint8_t> const& data, std::string const& extension, Replay& out) {
	if (data.empty()) return false;

	if (extension == ".xd") {
		return detail::parseXd(std::string(data.begin(), data.end()), out);
	}

	if (detail::startsWith(data, "HACKPRO", 7)) return detail::parseMhrBinary(data, out);
	if (detail::startsWith(data, "META", 4)) return detail::parseEchoBinary(data, out);

	if (extension == ".json" || extension == ".echo" || extension == ".mhr") {
		return detail::parseJsonFormats(std::string(data.begin(), data.end()), out);
	}
	return false;
}

}
