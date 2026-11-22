#pragma once

#include <array>
#include <string>
#include <string_view>
#include <vector>

#include <flecs.h>

namespace crabnado {

// A story comic: a golden-age comic page (assets/textures/crabnado/story/<page id>/, drawn by tools/make_story.py) that fades in over the title's backdrop, then shows its panels one at a time. Next (or Space or
// Enter) shows the next panel, and on the last one reads the finish label ("Fight!", "Continue") and finishes the comic; Skip (or
// Esc) finishes it at once. The title scene owns the opening page (Build() in Load(), Release() in Unload()); the arena
// scene builds and releases one for each page that follows an Act.
class StoryComic {
public:
	static constexpr int kPanelCount = 3;

	// Whether the page's art is on disk, so a data typo skips the page instead of showing nothing.
	[[nodiscard]] static bool PageExists(const flecs::world& world, std::string_view pageId);

	// Builds page `pageId`, its panels and the Skip and Next buttons, hidden, as children of `parent` (a w x h screen).
	// `finishLabel` is the Next button's label on the last panel.
	void Build(
		const flecs::world& world, flecs::entity parent, float width, float height, std::string_view pageId,
		std::string finishLabel
	);
	// Frees the comic's textures. Its entities go with their parent.
	void Release(const flecs::world& world);

	// Fades the page in and shows the first panel. Does nothing once started.
	void Start();
	// Advances the fades and reads the keyboard. Call every frame.
	void Tick(const flecs::world& world, float deltaTime);

	[[nodiscard]] bool Started() const { return started_; }
	// True once the player has pressed Next on the last panel or pressed Skip.
	[[nodiscard]] bool Finished() const { return finished_; }

private:
	void Next();
	void Skip();

	flecs::entity root_;
	flecs::entity page_;
	std::array<flecs::entity, kPanelCount> panels_;
	flecs::entity nextButton_;
	flecs::entity skipButton_;
	std::vector<std::string> textures_;
	std::string finishLabel_;

	bool started_{false};
	bool finished_{false};
	float pageFade_{0.0F};
	// How many panels are showing, and each panel's fade in from 0 to 1.
	int shown_{0};
	std::array<float, kPanelCount> panelFade_{};
};

} // namespace crabnado
