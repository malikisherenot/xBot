#include "../core/Bot.hpp"

#include <Geode/Geode.hpp>
#include <imgui-cocos.hpp>

#include <cfloat>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>

using namespace geode::prelude;
using namespace xbot;

namespace {

bool settingsOpen = false;

float scale() {
#ifdef GEODE_IS_ANDROID
	return 1.7f;
#else
	return 1.f;
#endif
}

void applyStyle() {
	auto& io = ImGui::GetIO();

	auto fontPath = Mod::get()->getResourcesDir() / "arial-italic.ttf";
	std::error_code ec;
	if (std::filesystem::exists(fontPath, ec)) {
		if (auto font = io.Fonts->AddFontFromFileTTF(utils::string::pathToString(fontPath).c_str(), 16.f)) {
			io.FontDefault = font;
		}
	}

	ImGui::StyleColorsDark();
	auto& style = ImGui::GetStyle();
	style.WindowRounding = 8.f;
	style.FrameRounding = 6.f;
	style.GrabRounding = 6.f;
	style.WindowTitleAlign = {0.5f, 0.5f};
	style.ScaleAllSizes(scale());
	io.FontGlobalScale = scale();
}

bool numberField(char const* label, char* buffer, size_t size, char const* current) {
	static ImGuiID editing = 0;
	ImGuiID id = ImGui::GetID(label);

	if (editing != id) std::snprintf(buffer, size, "%s", current);
	ImGui::InputText(label, buffer, size, ImGuiInputTextFlags_CharsDecimal);

	if (ImGui::IsItemActive()) editing = id;
	if (ImGui::IsItemDeactivated() && editing == id) {
		editing = 0;
		return true;
	}
	return false;
}

bool gearButton(char const* id, float size) {
	auto origin = ImGui::GetCursorScreenPos();
	bool pressed = ImGui::InvisibleButton(id, {size, size});
	bool hovered = ImGui::IsItemHovered();

	auto* list = ImGui::GetWindowDrawList();
	auto color = ImGui::GetColorU32(hovered ? ImGuiCol_ButtonHovered : ImGuiCol_Text);
	ImVec2 center{origin.x + size * 0.5f, origin.y + size * 0.5f};

	for (int i = 0; i < 8; i++) {
		float angle = static_cast<float>(i) * 0.7853982f;
		ImVec2 dir{std::cos(angle), std::sin(angle)};
		list->AddLine(
			{center.x + dir.x * size * 0.28f, center.y + dir.y * size * 0.28f},
			{center.x + dir.x * size * 0.46f, center.y + dir.y * size * 0.46f},
			color, size * 0.16f
		);
	}
	list->AddCircleFilled(center, size * 0.34f, color, 24);
	list->AddCircleFilled(center, size * 0.14f, ImGui::GetColorU32(ImGuiCol_WindowBg), 16);

	return pressed;
}

void shortcuts() {
	auto& bot = Bot::get();
	auto& io = ImGui::GetIO();
	if (io.WantTextInput) return;

	if (ImGui::IsKeyPressed(ImGuiKey_Tab, false)) bot.menuOpen = !bot.menuOpen;
	if (ImGui::IsKeyPressed(ImGuiKey_V, false)) {
		bot.frozen = !bot.frozen;
		bot.applyAudio();
	}
	if (bot.frozen && ImGui::IsKeyPressed(ImGuiKey_C, true)) bot.pendingSteps++;
}

void floatingButton() {
#ifdef GEODE_IS_ANDROID
	auto& bot = Bot::get();
	static bool dragged = false;
	float size = 56.f * scale();

	ImGui::SetNextWindowPos({20.f, 120.f}, ImGuiCond_FirstUseEver);
	ImGui::SetNextWindowBgAlpha(0.f);
	ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, {0.f, 0.f});
	ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.f);
	ImGui::Begin(
		"##xbot-button", nullptr,
		ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoScrollbar |
			ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoSavedSettings
	);

	auto origin = ImGui::GetCursorScreenPos();
	bool clicked = ImGui::InvisibleButton("##xbot-circle", {size, size});
	bool active = ImGui::IsItemActive();

	auto* list = ImGui::GetWindowDrawList();
	ImVec2 center{origin.x + size * 0.5f, origin.y + size * 0.5f};
	list->AddCircleFilled(center, size * 0.5f, ImGui::GetColorU32(active ? ImGuiCol_ButtonActive : ImGuiCol_Button), 48);
	list->AddCircle(center, size * 0.5f, ImGui::GetColorU32(ImGuiCol_ButtonHovered), 48, 2.f);
	auto textSize = ImGui::CalcTextSize("xBot");
	list->AddText(
		{center.x - textSize.x * 0.5f, center.y - textSize.y * 0.5f},
		ImGui::GetColorU32(ImGuiCol_Text), "xBot"
	);

	if (active && ImGui::IsMouseDragging(0, 8.f)) {
		auto pos = ImGui::GetWindowPos();
		auto delta = ImGui::GetIO().MouseDelta;
		ImGui::SetWindowPos({pos.x + delta.x, pos.y + delta.y});
		dragged = true;
	}
	if (clicked && !dragged) bot.menuOpen = !bot.menuOpen;
	if (!active) dragged = false;

	ImGui::End();
	ImGui::PopStyleVar(2);
#endif
}

void settings() {
	auto& bot = Bot::get();
	if (!bot.menuOpen || !settingsOpen) return;

	ImGui::SetNextWindowPos({120.f, 260.f}, ImGuiCond_FirstUseEver);
	if (!ImGui::Begin("Settings", &settingsOpen, ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_AlwaysAutoResize)) {
		ImGui::End();
		return;
	}

	ImGui::Checkbox("Noclip P1", &bot.noclipP1);
	ImGui::Checkbox("Noclip P2", &bot.noclipP2);

	ImGui::End();
}

void menu() {
	auto& bot = Bot::get();
	if (!bot.menuOpen) return;

	static char nameBuffer[96] = {};
	static char speedBuffer[24] = {};
	static char tpsBuffer[24] = {};
	static bool ready = false;
	static bool dirty = true;
	static std::vector<std::string> files;
	static std::string status;

	if (!ready) {
		std::strncpy(nameBuffer, bot.name.c_str(), sizeof(nameBuffer) - 1);
		ready = true;
	}
	if (dirty) {
		files = bot.list();
		dirty = false;
	}

	ImGui::SetNextWindowPos({100.f, 100.f}, ImGuiCond_FirstUseEver);
	ImGui::SetNextWindowSize({340.f * scale(), 0.f}, ImGuiCond_FirstUseEver);
	if (!ImGui::Begin("xBot", &bot.menuOpen, ImGuiWindowFlags_NoCollapse)) {
		ImGui::End();
		return;
	}

	static char const* labels[] = {"Idle", "Record", "Replay"};
	for (int i = 0; i < 3; i++) {
		if (i > 0) ImGui::SameLine();
		bool active = static_cast<int>(bot.mode) == i;
		if (active) ImGui::PushStyleColor(ImGuiCol_Button, ImGui::GetStyleColorVec4(ImGuiCol_ButtonActive));
		if (ImGui::Button(labels[i])) bot.setMode(static_cast<Mode>(i));
		if (active) ImGui::PopStyleColor();
	}

	float gearSize = ImGui::GetFrameHeight();
	ImGui::SameLine(ImGui::GetWindowWidth() - gearSize - ImGui::GetStyle().WindowPadding.x);
	if (gearButton("##settings", gearSize)) settingsOpen = !settingsOpen;

	ImGui::Separator();

	char current[24];
	std::snprintf(current, sizeof(current), "%.3f", bot.speed);
	if (numberField("Speed", speedBuffer, sizeof(speedBuffer), current)) {
		float value = std::strtof(speedBuffer, nullptr);
		if (value > 0.f) bot.setSpeed(value);
	}

	std::snprintf(current, sizeof(current), "%d", static_cast<int>(bot.tps));
	if (numberField("TPS", tpsBuffer, sizeof(tpsBuffer), current)) {
		int value = std::atoi(tpsBuffer);
		if (value > 0) bot.setTps(value);
	}

	if (ImGui::Checkbox("Frame Stepper", &bot.frozen)) bot.applyAudio();
	if (bot.frozen) {
		ImGui::SameLine();
		ImGui::PushButtonRepeat(true);
		if (ImGui::Button("Step")) bot.pendingSteps++;
		ImGui::PopButtonRepeat();
	}

	if (auto layer = PlayLayer::get()) {
		ImGui::Text("Frame: %d | Inputs: %zu", layer->m_gameState.m_currentProgress, bot.macro.inputs.size());
	}
	else {
		ImGui::Text("Inputs: %zu", bot.macro.inputs.size());
	}

	ImGui::Separator();

	ImGui::InputText("Name", nameBuffer, sizeof(nameBuffer));
	if (ImGui::Button("Save")) {
		bot.name = nameBuffer;
		status = bot.save(nameBuffer) ? "Saved" : "Save failed";
		dirty = true;
	}
	ImGui::SameLine();
	if (ImGui::Button("Load")) {
		bot.name = nameBuffer;
		status = bot.load(nameBuffer) ? "Loaded" : "Load failed";
	}
	ImGui::SameLine();
	if (ImGui::Button("Refresh")) dirty = true;

	if (ImGui::BeginListBox("##macros", {-FLT_MIN, 6.f * ImGui::GetTextLineHeightWithSpacing()})) {
		for (auto const& file : files) {
			if (ImGui::Selectable(file.c_str(), file == nameBuffer)) {
				std::strncpy(nameBuffer, file.c_str(), sizeof(nameBuffer) - 1);
			}
		}
		ImGui::EndListBox();
	}

	if (!status.empty()) ImGui::TextUnformatted(status.c_str());

	ImGui::End();
}

}

$on_mod(Loaded) {
	Bot::get().restore();
	ImGuiCocos::get().setup(applyStyle).draw([] {
		shortcuts();
		floatingButton();
		menu();
		settings();
	});
}
