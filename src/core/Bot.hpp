#pragma once

#include <Geode/Geode.hpp>
#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

namespace xbot {

enum class Mode { Idle, Record, Replay };

struct Input {
	uint32_t frame = 0;
	int button = 1;
	bool down = false;
	bool player2 = false;
};

struct Macro {
	float tps = 240.f;
	std::vector<Input> inputs;
};

class Bot {
public:
	static Bot& get();

	Mode mode = Mode::Idle;
	Macro macro;
	float speed = 1.f;
	float tps = 240.f;
	bool frozen = false;
	bool menuOpen = false;
	bool stepper = false;
	bool noclip = false;
	bool noclipP1 = true;
	bool noclipP2 = true;
	bool restart = false;
	bool resetting = false;
	int pendingSteps = 0;
	double accumulator = 0.0;
	std::string name = "macro";

	bool inLevel() const;
	bool vanilla() const;
	float stepDelta() const;

	void restore();
	void setMode(Mode mode);
	void setSpeed(float value);
	void setTps(int value);
	void applyAudio();
	void resetAudio();

	void record(uint32_t frame, int button, bool down, bool player2);
	void rewind(uint32_t frame);
	void inject(GJBaseGameLayer* layer, uint32_t frame);

	std::filesystem::path dir() const;
	std::vector<std::string> list() const;
	bool save(std::string const& file);
	bool load(std::string const& file);
	void autosave();

private:
	size_t m_cursor = 0;

	void press(GJBaseGameLayer* layer, bool down, int button, bool player2);
};

}
