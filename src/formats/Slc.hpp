#pragma once

#include <algorithm>
#include <cstdint>
#include <cstring>
#include <vector>

namespace xbot::slc {

struct Action {
	uint64_t frame = 0;
	uint8_t type = 0;
	bool holding = false;
	bool player2 = false;
};

struct Replay {
	double tps = 240.0;
	std::vector<Action> actions;
};

namespace detail {

constexpr uint64_t MaxActions = 2000000;

class Reader {
public:
	explicit Reader(std::vector<uint8_t> const& data) : m_data(data) {}

	size_t pos() const { return m_pos; }
	size_t left() const { return m_data.size() - m_pos; }
	void seek(size_t pos) { m_pos = pos; }

	bool skip(uint64_t count) {
		if (count > left()) return false;
		m_pos += static_cast<size_t>(count);
		return true;
	}

	bool unsignedValue(uint64_t& out, size_t bytes) {
		if (bytes == 0 || bytes > 8 || bytes > left()) return false;
		out = 0;
		for (size_t i = 0; i < bytes; i++) {
			out |= static_cast<uint64_t>(m_data[m_pos + i]) << (8 * i);
		}
		m_pos += bytes;
		return true;
	}

	bool real(double& out) {
		uint64_t bits;
		if (!unsignedValue(bits, 8)) return false;
		std::memcpy(&out, &bits, sizeof(out));
		return true;
	}

private:
	std::vector<uint8_t> const& m_data;
	size_t m_pos = 0;
};

inline void emitPlayer(std::vector<Action>& list, uint64_t previous, uint64_t state) {
	uint64_t frame = previous + (state >> 4);
	uint8_t button = static_cast<uint8_t>((state >> 2) & 0b11);
	bool holding = (state & 0b1) != 0;
	bool player2 = (state & 0b10) != 0;

	if (button == 0) {
		list.push_back({frame, 1, true, player2});
		list.push_back({frame, 1, false, player2});
		return;
	}
	list.push_back({frame, button, holding, player2});
}

inline uint64_t lastFrame(std::vector<Action> const& list) {
	return list.empty() ? 0 : list.back().frame;
}

inline bool readSection(Reader& r, std::vector<Action>& list, uint64_t count, size_t end) {
	uint64_t header;
	if (end - r.pos() < 2 || !r.unsignedValue(header, 2)) return false;

	uint64_t id = header >> 14;
	if (id == 0 || id == 1) {
		size_t byteSize = size_t(1) << ((header >> 12) & 0b11);
		uint64_t length = uint64_t(1) << ((header >> 8) & 0b1111);
		uint64_t repeats = id == 1 ? uint64_t(1) << ((header >> 3) & 0b11111) : 1;

		if (length * byteSize > end - r.pos()) return false;
		if (length * repeats > count - std::min<uint64_t>(count, list.size())) return false;

		std::vector<uint64_t> states;
		states.reserve(static_cast<size_t>(length));
		for (uint64_t i = 0; i < length; i++) {
			uint64_t state;
			if (!r.unsignedValue(state, byteSize)) return false;
			states.push_back(state);
		}

		for (uint64_t i = 0; i < repeats; i++) {
			for (uint64_t state : states) {
				emitPlayer(list, lastFrame(list), state);
			}
		}
		return true;
	}

	if (id == 2) {
		size_t deltaSize = size_t(1) << ((header >> 8) & 0b11);
		uint64_t type = (header >> 10) & 0b1111;

		uint64_t delta;
		if (deltaSize > end - r.pos() || !r.unsignedValue(delta, deltaSize)) return false;
		uint64_t frame = lastFrame(list) + delta;

		if (type == 3) {
			double tps;
			if (!r.real(tps)) return false;
			list.push_back({frame, 7, false, false});
		}
		else if (type <= 2) {
			if (!r.skip(8)) return false;
			list.push_back({frame, static_cast<uint8_t>(type + 4), false, false});
		}
		else if (type == 4) {
			list.push_back({frame, 8, false, false});
		}
		return true;
	}

	return false;
}

inline bool parseV3(std::vector<uint8_t> const& data, Replay& out) {
	Reader r(data);
	if (!r.skip(8)) return false;

	uint64_t metaSize;
	if (!r.unsignedValue(metaSize, 2) || metaSize < 8 || metaSize > r.left()) return false;

	size_t metaStart = r.pos();
	double tps;
	if (!r.real(tps)) return false;
	out.tps = tps;
	r.seek(metaStart);
	if (!r.skip(metaSize)) return false;

	bool found = false;
	while (r.left() > 1) {
		uint64_t id;
		uint64_t sizeField;
		if (!r.unsignedValue(id, 4) || !r.unsignedValue(sizeField, 8)) return false;

		uint64_t size = sizeField & 0x00FFFFFFFFFFFFFFull;
		if (size > r.left()) return false;

		if (id != 1 || found) {
			if (!r.skip(size)) return false;
			continue;
		}

		size_t start = r.pos();
		size_t end = size == 0 ? data.size() : start + static_cast<size_t>(size);

		uint64_t count;
		if (!r.unsignedValue(count, 8) || count > MaxActions) return false;

		std::vector<Action> list;
		list.reserve(static_cast<size_t>(std::min<uint64_t>(count, 1u << 20)));
		while (list.size() < count) {
			if (r.pos() >= end) return false;
			if (!readSection(r, list, count, end)) return false;
		}

		if (size != 0) r.seek(end);
		out.actions = std::move(list);
		found = true;
	}
	return found;
}

inline bool parseV2(std::vector<uint8_t> const& data, Replay& out) {
	Reader r(data);
	if (!r.skip(4)) return false;

	double tps;
	if (!r.real(tps)) return false;
	out.tps = tps;

	uint64_t metaSize;
	uint64_t length;
	uint64_t blobCount;
	if (!r.unsignedValue(metaSize, 8) || !r.skip(metaSize)) return false;
	if (!r.unsignedValue(length, 8) || !r.unsignedValue(blobCount, 8)) return false;
	if (blobCount > r.left() / 24) return false;

	struct Blob {
		uint64_t byteSize;
		uint64_t start;
		uint64_t length;
	};
	std::vector<Blob> blobs(static_cast<size_t>(blobCount));
	for (auto& blob : blobs) {
		if (!r.unsignedValue(blob.byteSize, 8) || !r.unsignedValue(blob.start, 8) ||
			!r.unsignedValue(blob.length, 8)) {
			return false;
		}
		if (blob.byteSize == 0 || blob.byteSize > 8) return false;
	}

	uint64_t frame = 0;
	std::vector<Action> list;
	for (auto const& blob : blobs) {
		if (blob.length > r.left() / blob.byteSize) return false;
		for (uint64_t i = 0; i < blob.length; i++) {
			uint64_t state;
			if (!r.unsignedValue(state, static_cast<size_t>(blob.byteSize))) return false;

			frame += state >> 5;
			uint8_t button = static_cast<uint8_t>((state & 0b11100) >> 2);
			bool holding = (state & 1) != 0;
			bool player2 = (state & 2) != 0;

			if (button == 7) {
				double value;
				if (!r.real(value)) return false;
			}
			if (button != 0) list.push_back({frame, button, holding, player2});
		}
	}

	out.actions = std::move(list);
	return true;
}

}

inline bool parse(std::vector<uint8_t> const& data, Replay& out) {
	static constexpr char v3[] = {'S', 'L', 'C', '3', 'R', 'P', 'L', 'Y'};
	static constexpr char v2[] = {'S', 'I', 'L', 'L'};

	if (data.size() >= sizeof(v3) && std::memcmp(data.data(), v3, sizeof(v3)) == 0) {
		return detail::parseV3(data, out);
	}
	if (data.size() >= sizeof(v2) && std::memcmp(data.data(), v2, sizeof(v2)) == 0) {
		return detail::parseV2(data, out);
	}
	return false;
}

}
