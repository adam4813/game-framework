/**
 * Template: Sound effect generator using jsfxr.
 *
 * Usage:
 *   cd Tools && npm install jsfxr
 *   node generate_sounds.js [--force]
 *
 * How to define sounds:
 *   - Preset string: uses sfxr.generate(preset) directly, e.g., "powerUp"
 *   - { base: "preset", overrides: {...} }: starts from preset, applies overrides
 *   - NEVER use raw param objects — unset values default to 0 (silence)
 *
 * Available presets:
 *   pickupCoin, laserShoot, explosion, powerUp, hitHurt,
 *   jump, blipSelect, synth, tone, click, random
 *
 * Customize OUTPUT_DIR below to match your engine's asset directory.
 */

const fs = require("fs");
const path = require("path");
const {sfxr} = require("jsfxr");

// Customize this path for your project's audio asset directory
const OUTPUT_DIR = path.join(__dirname, "..", "assets", "audio", "sfx");
const force = process.argv.includes("--force");

const SAMPLE_DEFAULTS = {sample_rate: 44100, sample_size: 8};

// ─── Define your sounds here ──────────────────────────────────────────────
// Replace these examples with your project's sounds.
const sounds = {
	// Click sound (simple preset)
	click: "click",
	// Jump sound — ascending tone, punchy start
	jump: {base: "jump", overrides: {p_base_freq: 0.38, p_freq_ramp: 0.22, p_vib_strength: 0.0}}
};

// ─── Generator (no changes needed below) ──────────────────────────────────

if (!fs.existsSync(OUTPUT_DIR)) {
	fs.mkdirSync(OUTPUT_DIR, {recursive: true});
}

let generated = 0, skipped = 0;
const MIN_SIZE = 2000;
const MAX_RETRIES = 10;

for (const [name, definition] of Object.entries(sounds)) {
	const outFile = path.join(OUTPUT_DIR, `${name}.wav`);

	if (!force && fs.existsSync(outFile)) {
		skipped++;
		console.log(`  SKIP  ${name}.wav (exists)`);
		continue;
	}

	let buf;
	for (let attempt = 0; attempt < MAX_RETRIES; attempt++) {
		let params;
		if (typeof definition === "string") {
			params = sfxr.generate(definition);
		} else if (definition.base) {
			params = sfxr.generate(definition.base);
			Object.assign(params, definition.overrides || {});
		} else {
			params = {...SAMPLE_DEFAULTS, ...definition};
		}

		const wave = sfxr.toWave(params);
		buf = Buffer.from(wave.wav);

		if (buf.length >= MIN_SIZE || attempt === MAX_RETRIES - 1) break;
	}
	fs.writeFileSync(outFile, buf);
	generated++;
	console.log(`  GEN   ${name}.wav (${buf.length} bytes)`);
}

console.log(
	`\nGenerated ${generated} sound files to assets/audio/sfx/` +
	(skipped ? ` (${skipped} skipped, use --force to regenerate)` : "")
);
