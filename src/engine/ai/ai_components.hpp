#pragma once

#include <string>
#include <unordered_map>

namespace engine::ai {

/// Generic AI blackboard: a shared key-value context any AI subsystem can read and write.
/// BehaviorTree actions/conditions, GOAP world state, utility context, and FSM conditions
/// all operate on compatible data — attaching a Blackboard lets multiple AI systems on the
/// same entity share facts without coupling their component types.
struct Blackboard {
	std::unordered_map<std::string, float>       floats;
	std::unordered_map<std::string, bool>        bools;
	std::unordered_map<std::string, std::string> strings;

	// Convenience helpers.
	void SetFloat(const std::string& key, const float v)        { floats[key]   = v; }
	void SetBool(const std::string& key,  const bool  v)        { bools[key]    = v; }
	void SetString(const std::string& key, std::string v)       { strings[key]  = std::move(v); }

	[[nodiscard]] float       GetFloat(const std::string& key, const float def = 0.0F) const;
	[[nodiscard]] bool        GetBool(const std::string& key,  const bool  def = false) const;
	[[nodiscard]] std::string GetString(const std::string& key, const std::string& def = {}) const;
};

inline float Blackboard::GetFloat(const std::string& key, const float def) const {
	const auto it = floats.find(key); return it != floats.end() ? it->second : def;
}
inline bool Blackboard::GetBool(const std::string& key, const bool def) const {
	const auto it = bools.find(key); return it != bools.end() ? it->second : def;
}
inline std::string Blackboard::GetString(const std::string& key, const std::string& def) const {
	const auto it = strings.find(key); return it != strings.end() ? it->second : def;
}

} // namespace engine::ai
