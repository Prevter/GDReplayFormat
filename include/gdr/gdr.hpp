#pragma once

#include <iostream>
#include <utility>
#include <vector>
#include <optional>

#include "json.hpp"

namespace gdr {

using namespace nlohmann;

struct Bot {
	std::string name;
	std::string version;

	inline Bot(std::string name, std::string version)
		: name(std::move(name)), version(std::move(version)) {}
};

struct Level {
	uint32_t id = 0;
	std::string name;

	Level() = default;

	inline Level(std::string name, uint32_t id = 0)
		: name(std::move(name)), id(id) {}
};

class Input {
 protected:
 	Input() = default;
 	template <typename, typename>
 	friend class Replay;
 public:
	uint32_t frame;
	int button;
	bool player2;
	bool down;

	inline virtual void parseExtension(json::object_t obj) {}
	inline virtual json::object_t saveExtension() const {
		return {};
	}

	inline Input(int frame, int button, bool player2, bool down)
		: frame(frame), button(button), player2(player2), down(down) {}


	inline static Input hold(int frame, int button, bool player2 = false) {
		return {frame, button, player2, true};
	}

	inline static Input release(int frame, int button, bool player2 = false) {
		return {frame, button, player2, false};
	}

	inline bool operator<(Input const& other) const {
		return frame < other.frame;
	}
};

template <typename S = void, typename T = Input>
class Replay {
 	Replay() = default;
 public:
	using InputType = T;
	using Self = std::conditional_t<std::is_same_v<S, void>, Replay<S, T>, S>;

	std::string author;
	std::string description;

	float duration = 0.f;
	float gameVersion;
	float version = 1.0;

	float framerate = 240.f;

	int seed = 0;
    int coins = 0;

	bool ldm = false;

	Bot botInfo;
	Level levelInfo;

	std::vector<InputType> inputs;

	uint32_t frameForTime(double time) {
		return static_cast<uint32_t>(time * (double)framerate);
	}

	virtual void parseExtension(json::object_t obj) {}
	virtual json::object_t saveExtension() const {
		return {};
	}

	Replay(std::string const& botName, std::string const& botVersion)
		: botInfo(botName, botVersion) {}

	static std::optional<Self> importData(std::vector<uint8_t> const& data, bool importInputs = true) {
		Self replay;
		json replayJson;

		replayJson = json::from_msgpack(data, true, false);
		if (replayJson.is_discarded()) {
			replayJson = json::parse(data, nullptr, false);
			if (replayJson.is_discarded()) return std::nullopt;
		}

#define STRINGIFY(x) #x
#define SAFE_ASSIGN(var) if (!replayJson.contains(STRINGIFY(var))) return std::nullopt; replay.var = replayJson[STRINGIFY(var)]
#define SAFE_ASSIGN1(var, obj) if (!obj.contains(STRINGIFY(var))) return std::nullopt; replay.obj##Info.var = obj[STRINGIFY(var)]

		if (!replayJson.contains("bot") || !replayJson["bot"].is_object())
			return std::nullopt;

		if (!replayJson.contains("level") || !replayJson["level"].is_object())
			return std::nullopt;

		SAFE_ASSIGN(gameVersion);
		SAFE_ASSIGN(description);
		SAFE_ASSIGN(version);
		SAFE_ASSIGN(duration);

		SAFE_ASSIGN(author);
		SAFE_ASSIGN(seed);
		SAFE_ASSIGN(coins);
		SAFE_ASSIGN(ldm);

		auto& bot = replayJson["bot"];
		SAFE_ASSIGN1(name, bot);
		SAFE_ASSIGN1(version, bot);

		auto& level = replayJson["level"];
		SAFE_ASSIGN1(id, level);
		SAFE_ASSIGN1(name, level);

#undef SAFE_ASSIGN1
#undef SAFE_ASSIGN
#undef STRINGIFY

		if (replayJson.contains("framerate"))
			replay.framerate = replayJson["framerate"];
		replay.parseExtension(replayJson.get<json::object_t>());

		if (!importInputs)
			return replay;

		for (json const& inputJson : replayJson["inputs"]) {
			InputType input;
			input.frame = inputJson["frame"];
			input.button = inputJson["btn"];
			input.player2 = inputJson["2p"];
			input.down = inputJson["down"];
			input.parseExtension(inputJson.get<json::object_t>());

			replay.inputs.push_back(input);
		}

		return replay;
	}

	std::vector<uint8_t> exportData(bool exportJson = false) {
		json replayJson = saveExtension();
		replayJson["gameVersion"] = gameVersion;
		replayJson["description"] = description;
		replayJson["version"] = version;
		replayJson["duration"] = duration;
		replayJson["bot"]["name"] = botInfo.name;
		replayJson["bot"]["version"] = botInfo.version;
		replayJson["level"]["id"] = levelInfo.id;
		replayJson["level"]["name"] = levelInfo.name;
		replayJson["author"] = author;
		replayJson["seed"] = seed;
		replayJson["coins"] = coins;
		replayJson["ldm"] = ldm;
		replayJson["framerate"] = framerate;

		for (InputType const& input : inputs) {
			json inputJson = input.saveExtension();
			inputJson["frame"] = input.frame;
			inputJson["btn"] = input.button;
			inputJson["2p"] = input.player2;
			inputJson["down"] = input.down;

			replayJson["inputs"].push_back(inputJson);
		}

		if (exportJson) {
			std::string replayString = replayJson.dump();
			return { replayString.begin(), replayString.end() };
		} else {
			return json::to_msgpack(replayJson);
		}
	}
};

} // namespace gdr