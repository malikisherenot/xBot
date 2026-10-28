#include "Formats.hpp"
#include "Slc.hpp"

#include <gdr/gdr.hpp>

#include <algorithm>
#include <cctype>
#include <fstream>
#include <iterator>
#include <sstream>
#include <vector>

namespace {

struct GInput : gdr::Input<"xBot"> {
	GInput() = default;
	GInput(int frame, int button, bool player2, bool down) : Input(frame, button, player2, down) {}
};

struct GReplay : gdr::Replay<GReplay, GInput> {
	GReplay() : Replay("xBot", 1) {}
};

std::string extensionOf(std::filesystem::path const& path) {
	auto ext = path.extension().string();
	std::transform(ext.begin(), ext.end(), ext.begin(), [](unsigned char c) { return std::tolower(c); });
	return ext;
}

bool readBytes(std::filesystem::path const& path, std::vector<uint8_t>& out) {
	std::ifstream file(path, std::ios::binary);
	if (!file) return false;
	out.assign(std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>());
	return !out.empty();
}

void finalize(xbot::Macro& macro) {
	std::stable_sort(
		macro.inputs.begin(), macro.inputs.end(),
		[](xbot::Input const& a, xbot::Input const& b) { return a.frame < b.frame; }
	);
}

bool loadGdr(std::filesystem::path const& path, xbot::Macro& out) {
	std::vector<uint8_t> data;
	if (!readBytes(path, data)) return false;

	auto result = GReplay::importData(data);
	if (!result.isOk()) return false;
	auto replay = std::move(result).unwrap();

	out.tps = replay.framerate > 0.f ? replay.framerate : 240.f;
	out.inputs.clear();
	out.inputs.reserve(replay.inputs.size());
	for (auto const& input : replay.inputs) {
		out.inputs.push_back({
			static_cast<uint32_t>(input.frame),
			static_cast<int>(input.button),
			static_cast<bool>(input.down),
			static_cast<bool>(input.player2)
		});
	}
	finalize(out);
	return !out.inputs.empty();
}

bool loadText(std::filesystem::path const& path, xbot::Macro& out) {
	std::ifstream file(path);
	if (!file) return false;

	out.tps = 240.f;
	out.inputs.clear();

	std::string line;
	bool first = true;
	while (std::getline(file, line)) {
		if (line.empty() || line[0] == '#') continue;

		std::istringstream stream(line);
		std::vector<double> values;
		double value;
		while (stream >> value) values.push_back(value);
		if (values.empty()) continue;

		if (values.size() == 1) {
			if (first && values[0] > 0) out.tps = static_cast<float>(values[0]);
			first = false;
			continue;
		}
		first = false;

		xbot::Input input;
		input.frame = static_cast<uint32_t>(values[0]);
		input.down = values[1] != 0;
		input.player2 = values.size() > 2 && values[2] != 0;
		input.button = values.size() > 3 ? static_cast<int>(values[3]) : 1;
		out.inputs.push_back(input);
	}
	finalize(out);
	return !out.inputs.empty();
}

bool loadSlc(std::filesystem::path const& path, xbot::Macro& out) {
	std::vector<uint8_t> data;
	if (!readBytes(path, data)) return false;

	xbot::slc::Replay replay;
	if (!xbot::slc::parse(data, replay)) return false;

	out.tps = replay.tps > 0.0 ? static_cast<float>(replay.tps) : 240.f;
	out.inputs.clear();

	bool newAttempt = false;
	for (auto const& action : replay.actions) {
		if (action.type == 5) {
			newAttempt = true;
			continue;
		}
		if (action.type < 1 || action.type > 3 || action.frame > UINT32_MAX) continue;

		if (newAttempt) {
			out.inputs.clear();
			newAttempt = false;
		}

		auto frame = static_cast<uint32_t>(action.frame);
		while (!out.inputs.empty() && out.inputs.back().frame > frame) {
			out.inputs.pop_back();
		}
		out.inputs.push_back({frame, action.type, action.holding, action.player2});
	}
	return !out.inputs.empty();
}

}

namespace xbot::formats {

bool supported(std::filesystem::path const& path) {
	auto ext = extensionOf(path);
	return ext == ".gdr" || ext == ".gdr2" || ext == ".txt" || ext == ".slc";
}

bool load(std::filesystem::path const& path, Macro& out) {
	auto ext = extensionOf(path);
	if (ext == ".gdr" || ext == ".gdr2") return loadGdr(path, out);
	if (ext == ".txt") return loadText(path, out);
	if (ext == ".slc") return loadSlc(path, out);
	return false;
}

bool save(std::filesystem::path const& path, Macro const& macro, int levelId, std::string const& levelName) {
	GReplay replay;
	replay.framerate = macro.tps;
	replay.gameVersion = 22081;
	replay.levelInfo.id = levelId;
	replay.levelInfo.name = levelName;
	replay.duration = macro.inputs.empty() ? 0.f : static_cast<float>(macro.inputs.back().frame) / macro.tps;
	for (auto const& input : macro.inputs) {
		replay.inputs.emplace_back(static_cast<int>(input.frame), input.button, input.player2, input.down);
	}

	auto result = replay.exportData();
	if (!result.isOk()) return false;
	auto data = std::move(result).unwrap();

	std::ofstream file(path, std::ios::binary);
	if (!file) return false;
	file.write(reinterpret_cast<char const*>(data.data()), static_cast<std::streamsize>(data.size()));
	return static_cast<bool>(file);
}

}
