// wasm_audio_unlock.js — injected before the WASM module via --pre-js.
//
// Browsers keep Web Audio suspended until a user gesture. miniaudio (inside raylib) resumes its
// AudioContext only on "click"/"touchend", so a player whose first input is a key press hears
// nothing. This shim records every AudioContext the page creates and resumes any suspended one on
// keydown/pointerdown too (a mouse pointerdown counts as a gesture; on touch screens miniaudio's own
// touchend does the unlock). It doesn't depend on miniaudio internals, which closure may rename.
(function () {
	var Original = window["AudioContext"] || window["webkitAudioContext"];
	if (!Original) {
		return;
	}
	var contexts = [];

	function Tracked(options) {
		var ctx = new Original(options);
		contexts.push(ctx);
		return ctx;
	}
	Tracked.prototype = Original.prototype; // keeps instanceof AudioContext working
	window["AudioContext"] = Tracked;
	window["webkitAudioContext"] = Tracked;

	function resumeAll() {
		for (var i = 0; i < contexts.length; i++) {
			if (contexts[i]["state"] === "suspended") {
				contexts[i]["resume"]();
			}
		}
	}
	["keydown", "pointerdown"].forEach(function (type) {
		document.addEventListener(type, resumeAll, true);
	});
})();
