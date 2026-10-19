#include "hud_layout.hpp"

#include <format>

#include <nlohmann/json.hpp>

#include "json_file.hpp"

using nlohmann::json;

namespace signal_room {

namespace {

using engine::core::Rgba;

// Reads fields of one section, remembering the first error.
class Reader {
public:
	Reader(const json& doc, const Palette& palette, const std::string& path) : doc_{doc}, palette_{palette}, path_{path} {}

	void Number(const char* section, const char* key, float& out) {
		const auto& value = Section(section);
		if (!value.contains(key)) {
			return;
		}
		if (!value.at(key).is_number()) {
			Fail(std::format("{}.{} must be a number", section, key));
			return;
		}
		out = value.at(key).get<float>();
	}

	void Text(const char* section, const char* key, std::string& out) {
		const auto& value = Section(section);
		if (value.contains(key) && value.at(key).is_string()) {
			out = value.at(key).get<std::string>();
		}
	}

	Rgba Token(const std::string& token, const std::string& where) {
		const auto* colour = palette_.Get(token);
		if (colour == nullptr) {
			Fail(std::format("{} names unknown colour '{}'", where, token));
			return {};
		}
		return *colour;
	}

	void Colour(const char* key, Rgba& out) {
		const auto& colours = Section("colours");
		if (!colours.contains(key) || !colours.at(key).is_string()) {
			Fail(std::format("colours.{} must be a palette token", key));
			return;
		}
		out = Token(colours.at(key).get<std::string>(), std::format("colours.{}", key));
	}

	[[nodiscard]] const json& Section(const char* name) const {
		static const json empty = json::object();
		return doc_.contains(name) && doc_.at(name).is_object() ? doc_.at(name) : empty;
	}

	void Fail(const std::string& what) {
		if (error_.empty()) {
			error_ = std::format("HUD file {}: {}", path_, what);
		}
	}

	[[nodiscard]] const std::string& Error() const { return error_; }

private:
	const json& doc_;
	const Palette& palette_;
	const std::string& path_;
	std::string error_;
};

} // namespace

std::expected<HudLayout, std::string> LoadHudLayoutFile(const std::string& path, const Palette& palette) {
	const auto doc = ReadJsonFile(path, "HUD");
	if (!doc) {
		return std::unexpected(doc.error());
	}
	HudLayout hud;
	Reader in{*doc, palette, path};
	if (doc->contains("margin") && doc->at("margin").is_number()) {
		hud.margin = doc->at("margin").get<float>();
	}
	in.Number("card", "width", hud.cardWidth);
	in.Number("card", "height", hud.cardHeight);
	in.Number("card", "spacing", hud.cardSpacing);
	in.Number("card", "padding", hud.cardPadding);
	in.Number("card", "gauge_height", hud.gaugeHeight);
	in.Number("card", "swatch_size", hud.swatchSize);
	in.Number("fonts", "callsign", hud.callsignFont);
	in.Number("fonts", "tier", hud.tierFont);
	in.Number("fonts", "frequency", hud.frequencyFont);
	in.Number("fonts", "clock", hud.clockFont);
	in.Number("fonts", "tag", hud.tagFont);
	in.Number("fonts", "score", hud.scoreFont);
	in.Number("fonts", "chain", hud.chainFont);
	in.Number("timing", "exit_flash", hud.exitFlash);
	in.Number("timing", "red_flash_hz", hud.redFlashHz);
	in.Number("timing", "pop", hud.popSeconds);
	in.Number("timing", "pop_scale", hud.popScale);
	in.Text("labels", "last_call", hud.lastCallLabel);
	in.Text("labels", "chain_prefix", hud.chainPrefix);
	in.Colour("panel", hud.panel);
	in.Colour("border", hud.border);
	in.Colour("text", hud.text);
	in.Colour("text_dim", hud.textDim);
	in.Colour("track", hud.track);
	in.Colour("standby", hud.standby);
	in.Colour("flash_clear", hud.flashClear);
	in.Colour("flash_lost", hud.flashLost);
	in.Colour("last_call", hud.lastCall);
	for (const auto& [band, token] : in.Section("bands").items()) {
		if (!token.is_string()) {
			in.Fail(std::format("bands.{} must be a palette token", band));
			break;
		}
		hud.bands[band] = in.Token(token.get<std::string>(), std::format("bands.{}", band));
	}
	if (!in.Error().empty()) {
		return std::unexpected(in.Error());
	}
	if (hud.cardWidth <= 0.0F || hud.cardHeight <= 0.0F || hud.popSeconds <= 0.0F || hud.exitFlash < 0.0F) {
		return std::unexpected(std::format("HUD file {}: card size and pop time must be positive", path));
	}
	return hud;
}

} // namespace signal_room
