/**
 * flecs-api.js — cross-platform helpers for the Flecs Remote (REST) API.
 *
 * The engine starts the Flecs Remote API automatically (see engine/ecs/flecs_remote and the
 * project AI instructions); it listens on http://localhost:27750. This module wraps the REST
 * endpoints that are useful when inspecting or driving a running game from an agent or the
 * terminal — querying entities/components and mutating component state (e.g. switching the
 * active scene without touching the keyboard).
 *
 * Modelled on the flecs.js client (`connect(host).query(expr)`), but implemented with the
 * built-in global `fetch` (Node 18+) so it runs cross-platform with no dependencies. The
 * official `flecs` npm package is a maintained alternative, but was last published in 2023 and
 * lagged the current REST API — hence this small in-repo wrapper.
 *
 * Flecs Remote API reference (endpoints + options):
 *   Overview:         https://www.flecs.dev/flecs/md_docs_2FlecsRemoteApi.html
 *   JavaScript lib:   https://www.flecs.dev/flecs/md_docs_2FlecsRemoteApi.html#javascript-library
 *   GET  query:       https://www.flecs.dev/flecs/md_docs_2FlecsRemoteApi.html#get-query
 *   GET  entity:      https://www.flecs.dev/flecs/md_docs_2FlecsRemoteApi.html#get-entity
 *   PUT  component:   https://www.flecs.dev/flecs/md_docs_2FlecsRemoteApi.html#put-component
 *   DELETE component: https://www.flecs.dev/flecs/md_docs_2FlecsRemoteApi.html#delete-component
 *
 * Endpoints used:
 *   GET    /query?expr=<expr>&<opts>        run a query
 *   GET    /entity/<path>?<opts>            fetch one entity
 *   PUT    /component/<path>?component=<c>  add a tag, or set a value with &value=
 *   DELETE /component/<path>?component=<c>  remove a component/tag
 *
 * Notes:
 *   - Anonymous entities are addressed by id as "#<id>" (e.g. "#768").
 *   - Component/type names use dot paths, e.g. "engine.ecs.Transform" or the scene activation
 *     tag "engine.scene.SceneManagementModule.Active".
 *
 * Library usage:
 *   const { connect } = require("./flecs-api");
 *   const flecs = connect();                       // defaults to localhost:27750
 *   console.log(await flecs.query("engine.ecs.Transform", { entityIds: true }));
 *   await flecs.switchScene("#768");               // move the Active scene tag to the Game scene
 *
 * CLI usage:
 *   node flecs-api.js query  "engine.scripting.ScriptComponent" --entity-ids // Fully qualified component name
 *   node flecs-api.js entity FallingCube.CubeJumpScript
 *   node flecs-api.js add    "#768" engine.scene.SceneManagementModule.Active
 *   node flecs-api.js remove "#765" engine.scene.SceneManagementModule.Active
 *   node flecs-api.js switch-scene Game        // switch by SceneId identity name (preferred)
 *   node flecs-api.js switch-scene "#807"      // ...or by raw runtime id
 *   node flecs-api.js scenes                   // scene(s) currently carrying Active
 *   node flecs-api.js list-scenes              // all scenes: identity name, id, active flag
 *   node flecs-api.js pause            // add the Paused tag (freezes Pausable systems)
 *   node flecs-api.js resume           // remove the Paused tag
 *   node flecs-api.js toggle-pause     // flip the current pause state
 *   node flecs-api.js list-buttons     // all UI Button entities: id, name, label
 *   node flecs-api.js click PlayButton // fire a UI button's OnClick (by name or #id)
 *   node flecs-api.js override-input '{"mouse":{"left":{"pressed":true}}}'
 *   node flecs-api.js restore-input
 *   (append --host http://host:port to target a different server)
 */

"use strict";

// Application-specific full path of the scene-activation tag. Scenes are entities carrying a
// SceneComponent; adding this tag activates one (see engine/scene/scene_module.cpp).
const SCENE_ACTIVE_TAG = "engine.scene.SceneManagementModule.Active";

// Full path of the SceneId identity enum. Each scene entity carries a (SceneId, <constant>) pair
// (e.g. (SceneId, Game)) as a stable, queryable identity, so scenes can be resolved by name
// instead of by their runtime-assigned entity id (see engine/scene/scene_components.hpp).
const SCENE_ID_ENUM = "engine.scene.SceneManagementModule.SceneId";

// Full path of the InputState singleton component (engine/input/input_components.hpp). The
// singleton value is stored on the component's own entity, so it is addressed by that entity.
const INPUT_STATE_COMPONENT = "engine.input.InputState";

// Systems that write the InputState singleton every frame: InputPoll (PreUpdate) copies live
// platform input in, InputResetFrameState (PostUpdate) clears the per-frame "pressed"/scroll
// flags. Disabling both lets an injected InputState value persist so scripts can observe it.
const INPUT_DRIVER_SYSTEMS = ["InputPoll", "InputResetFrameState"];

// UI component/tag paths (registered inside engine::ui::UIModule). Adding the click-request tag to
// a Button entity makes the UI module fire that button's OnClick handler + SoundEffect on the next
// frame, exactly like a real mouse click — the programmatic way to drive the UI over REST.
const UI_BUTTON_COMPONENT = "engine.ui.UIModule.Button";
const UI_CLICK_REQUEST_TAG = "engine.ui.UIModule.UIClickRequest";

class FlecsClient {
	constructor(baseUrl = "http://localhost:27750") {
		this.baseUrl = baseUrl.replace(/\/$/, "");
	}

	// Low-level request. Returns parsed JSON when possible, otherwise the raw text.
	async request(method, path, query) {
		let url = this.baseUrl + path;
		if (query) {
			const params = Object.entries(query)
				.filter(([, v]) => v !== undefined && v !== null)
				.map(([k, v]) => `${k}=${encodeURIComponent(v)}`);
			if (params.length) url += "?" + params.join("&");
		}

		const res = await fetch(url, {method});
		const text = await res.text();
		try {
			return JSON.parse(text);
		} catch {
			return text;
		}
	}

	// Run a query expression, e.g. "engine.ecs.Transform, engine.audio.AudioModule.SoundEffect".
	// Component names MUST use the full Flecs dot-path (C++ namespace + module class for
	// components registered inside a module constructor). An unresolved identifier throws an
	// exception in the game process, which will crash it — always use a qualified path.
	//
	// Path conventions for this engine:
	//   engine.<ns>.<type>               — registered directly on world (e.g. engine_context.cpp)
	//   engine.<ns>.<ModuleClass>.<type> — registered inside world.import<Module>() constructor
	//
	// Examples:
	//   engine.ecs.Transform                      (registered directly, engine::ecs namespace)
	//   engine.audio.AudioModule.SoundEffect      (inside AudioModule constructor)
	//   engine.render.RenderModule.DirectionalLight
	//   engine.input.InputModule.InputState       (use as singleton; registered in InputModule)
	//   engine.scene.SceneManagementModule.Active (tag, registered in SceneManagementModule)
	query(expr, {entityIds = false, values = false, fullPaths = false, tryQuery = true} = {}) {
		return this.request("GET", "/query", {
			expr,
			entity_ids: entityIds ? "true" : undefined,
			values: values ? "true" : undefined,
			full_paths: fullPaths ? "true" : undefined,
			// tryQuery=true tells Flecs REST not to throw an HTTP error on query failure.
			// Without it, an unresolved identifier throws a C++ exception inside the game process,
			// crashing it. Always keep this enabled so bad queries return an error JSON instead.
			try: tryQuery ? "true" : undefined,
		});
	}

	// Fetch a single entity by path ("FallingCube", "FallingCube.CubeJumpScript", "#768").
	// https://www.flecs.dev/flecs/md_docs_2FlecsRemoteApi.html#get-entity
	entity(path, {values = false} = {}) {
		return this.request("GET", "/entity/" + encodeURIComponent(path), {
			values: values ? "true" : undefined,
		});
	}

	// Add a tag, or set a component value (pass `value` as a JSON/flecs-expr payload string).
	// https://www.flecs.dev/flecs/md_docs_2FlecsRemoteApi.html#put-component
	addComponent(path, component, value) {
		return this.request("PUT", "/component/" + encodeURIComponent(path), {component, value});
	}

	// Remove a component/tag from an entity.
	// https://www.flecs.dev/flecs/md_docs_2FlecsRemoteApi.html#delete-component
	removeComponent(path, component) {
		return this.request("DELETE", "/component/" + encodeURIComponent(path), {component});
	}

	// Enable or disable an entity (toggles the Disabled tag) or a single component on it.
	// Useful for pausing a system without a rebuild, e.g. toggle("#600", false) to stop InputPoll.
	// https://www.flecs.dev/flecs/md_docs_2FlecsRemoteApi.html#put-toggle
	toggle(path, enable, component) {
		return this.request("PUT", "/toggle/" + encodeURIComponent(path), {
			enable: enable ? "true" : "false",
			component,
		});
	}

	// List scene entities that currently carry the Active tag.
	scenes(activeTag = SCENE_ACTIVE_TAG) {
		return this.query(activeTag, {entityIds: true});
	}

	// Resolve the runtime "#<id>" of the scene whose SceneId identity is `name` (e.g. "Game",
	// "Title") by querying its (SceneId, name) pair. Returns null if no such scene exists.
	async sceneIdentityEntity(name) {
		const res = await this.query(`(${SCENE_ID_ENUM},${SCENE_ID_ENUM}.${name})`, {entityIds: true});
		const r = ((res && res.results) || [])[0];
		return r ? "#" + r.id : null;
	}

	// Accept either a raw entity ref ("#807") or a SceneId identity name ("Game") and return a
	// concrete "#<id>" address. Identity names are preferred — they are stable across launches.
	async resolveSceneRef(ref) {
		if (typeof ref === "string" && ref.startsWith("#")) return ref;
		return this.sceneIdentityEntity(ref);
	}

	// List every registered scene with its SceneId identity name, runtime id, and active flag.
	async listScenes() {
		const res = await this.query(`(${SCENE_ID_ENUM},*)`, {entityIds: true, fullPaths: true});
		const active = await this.scenes();
		const activeIds = new Set(((active && active.results) || []).map((r) => r.id));
		const out = [];
		for (const r of (res && res.results) || []) {
			let scene = null;
			const ids = r.fields && r.fields.ids;
			if (ids && ids.length) {
				const pair = ids[ids.length - 1];
				if (Array.isArray(pair) && pair[1]) scene = String(pair[1]).split(".").pop();
			}
			out.push({scene, id: "#" + r.id, active: activeIds.has(r.id)});
		}
		return out;
	}

	// Move the scene Active tag to the target scene: remove it from every entity that currently
	// has it, then add it to the target. `target` may be a SceneId identity name ("Game") or a
	// raw "#<id>". Mirrors engine::scene::ActivateScene in C++.
	async switchScene(target, activeTag = SCENE_ACTIVE_TAG) {
		const targetSceneEntity = await this.resolveSceneRef(target);
		if (!targetSceneEntity) throw new Error(`No scene found for "${target}"`);
		const current = await this.query(activeTag, {entityIds: true});
		const results = (current && current.results) || [];
		for (const result of results) {
			if (result.name) await this.removeComponent(result.name, activeTag);
		}
		await this.addComponent(targetSceneEntity, activeTag);
		// Adds are deferred a frame or two (applied on the next world.progress, then observers
		// run). Wait until the target actually carries the Active tag before returning.
		await this.waitFor(async () => {
			const now = await this.query(activeTag, {entityIds: true});
			return ((now && now.results) || []).some((r) => this.entityMatches(r, targetSceneEntity));
		});
		return this.scenes(activeTag);
	}

	// Resolve entities matching a set of leaf names (from a query) to their "#<id>" addresses.
	// Systems and singleton components live under module scopes, so addressing them by id avoids
	// depending on their full path.
	async resolveEntityIds(queryExpr, leafNames) {
		const res = await this.query(queryExpr, {entityIds: true});
		const results = (res && res.results) || [];
		const wanted = new Set(leafNames);
		const found = {};
		for (const r of results) {
			if (r.name && wanted.has(r.name)) found[r.name] = "#" + r.id;
		}
		return found;
	}

	// Poll `fn` (sync or async, returns truthy when the condition holds) until it succeeds or the
	// attempts run out. Flecs REST mutations are deferred — commands apply on the next progress
	// tick and observers run a frame later — so read-after-write needs a short retry window rather
	// than a blind sleep. Returns true if the condition was observed, false on timeout.
	async waitFor(fn, {tries = 12, delayMs = 30} = {}) {
		for (let i = 0; i < tries; i++) {
			try {
				if (await fn()) return true;
			} catch {
				/* transient REST error mid-poll; retry */
			}
			await new Promise((r) => setTimeout(r, delayMs));
		}
		return false;
	}

	// Does a query result row refer to the given entity ref? Handles both "#<id>" addresses and
	// named/full-path entities.
	entityMatches(result, entityRef) {
		if (typeof entityRef === "string" && entityRef.startsWith("#")) {
			return String(result.id) === entityRef.slice(1);
		}
		return result.name === entityRef;
	}

	// Whether `entityRef` currently has `component`, checked via a query on the component (works
	// for tags and data components). Returns null when `component` is addressed by raw #id and so
	// cannot be used as a query expression (caller should treat null as "unverifiable").
	async hasComponent(entityRef, component) {
		if (typeof component === "string" && component.startsWith("#")) return null;
		const res = await this.query(component, {entityIds: true, fullPaths: true});
		const results = (res && res.results) || [];
		return results.some((r) => this.entityMatches(r, entityRef));
	}

	// Override live input: disable InputPoll, inject the given state for several frames (to
	// outlast InputResetFrameState's pressed-flag clear), then re-enable InputPoll.
	// Uses toggle() — the only safe way to pause a pipeline system via REST. Adding
	// flecs.core.Disabled directly corrupts Flecs pipeline stats (stats.c assert).
	// The singleton entity is addressed by numeric id as both path and component param.
	async overrideInput(state, {frames = 6, intervalMs = 150} = {}) {
		const sysList = await this.resolveEntityIds("flecs.system.System", INPUT_DRIVER_SYSTEMS);
		const pollId = sysList["InputPoll"];
		if (!pollId) throw new Error("InputPoll system not found");
		const compIds = await this.resolveEntityIds("flecs.core.Component", ["InputState"]);
		const entityId = compIds["InputState"];
		if (!entityId) throw new Error("InputState component entity not found");
		await this.toggle(pollId, false);
		const value = typeof state === "string" ? state : JSON.stringify(state);
		for (let i = 0; i < frames; i++) {
			await this.addComponent(entityId, entityId, value);
			await new Promise((r) => setTimeout(r, intervalMs));
		}
		await this.toggle(pollId, true);
		return {entityId, frames};
	}

	// Re-enable InputPoll if overrideInput was interrupted.
	async restoreInput() {
		const sysList = await this.resolveEntityIds("flecs.system.System", INPUT_DRIVER_SYSTEMS);
		const pollId = sysList["InputPoll"];
		if (pollId) await this.toggle(pollId, true);
		return {restored: pollId};
	}

	// Resolve the Paused tag's component entity ("#<id>"). Registered lazily under the scene
	// module, so it is addressed by id rather than a hard-coded path.
	async pausedTagId() {
		const ids = await this.resolveEntityIds("flecs.core.Component", ["Paused"]);
		return ids["Paused"] || null;
	}

	// Whether the world currently carries the Paused tag (i.e. the active scene is paused).
	async isPaused() {
		const id = await this.pausedTagId();
		if (!id) return false;
		const res = await this.query(id, {entityIds: true});
		return !!(res && res.results && res.results.length);
	}

	// Pause or resume the active scene by adding/removing the Paused tag on its own singleton
	// entity — the REST equivalent of world.add<scene::Paused>() / world.remove<...>(). The
	// scene-management observer reacts and swaps the pipeline, so this mirrors pressing ESC.
	async setPaused(paused) {
		const id = await this.pausedTagId();
		if (!id) throw new Error("Paused tag not found — is a scene with pausable systems loaded?");
		if (paused) await this.addComponent(id, id);
		else await this.removeComponent(id, id);
		// The tag add/remove is deferred and the pipeline-swap observer runs a frame later, so
		// confirm the world actually reached the requested pause state before returning.
		const confirmed = await this.waitFor(async () => (await this.isPaused()) === paused);
		return {paused, entityId: id, confirmed};
	}

	// Flip the current pause state.
	async togglePause() {
		const paused = await this.isPaused();
		return this.setPaused(!paused);
	}

	// Resolve a button reference (a raw "#id", a full dot-path, or a bare leaf name like
	// "PlayButton") to a concrete "#<id>" address. Leaf names are convenient but are not valid REST
	// paths for nested entities (e.g. TitleUI.PlayButton), so match them against the Button set.
	async resolveButtonRef(target) {
		if (typeof target === "string" && target.startsWith("#")) return target;
		const res = await this.query(UI_BUTTON_COMPONENT, {entityIds: true, fullPaths: true});
		for (const r of (res && res.results) || []) {
			const name = String(r.name || "");
			if (name === target || name.split(".").pop() === target) return "#" + r.id;
		}
		return target; // fall back: assume it is already a valid entity path
	}

	// Trigger a UI button by adding the UIClickRequest tag. `target` is a button address — a raw
	// "#<id>", a full path, or a bare name ("PlayButton"; see listButtons). The UI module consumes
	// the tag on the next frame, firing the button's OnClick handler and SoundEffect, then removes
	// it. Waits until the tag has been consumed before returning.
	async click(target) {
		const ref = await this.resolveButtonRef(target);
		await this.addComponent(ref, UI_CLICK_REQUEST_TAG);
		const consumed = await this.waitFor(async () => {
			const still = await this.query(UI_CLICK_REQUEST_TAG, {entityIds: true});
			return !((still && still.results) || []).some((r) => this.entityMatches(r, ref));
		});
		return {target, ref, consumed};
	}

	// List every UI Button entity with its address and label, so buttons (including anonymous ones
	// created by the factories) can be discovered and clicked by id or full path.
	async listButtons() {
		const res = await this.query(UI_BUTTON_COMPONENT, {entityIds: true, fullPaths: true, values: true});
		const out = [];
		for (const r of (res && res.results) || []) {
			const value = r.values && r.values[0];
			out.push({id: "#" + r.id, path: r.name || null, label: value ? value.label : undefined});
		}
		return out;
	}
}

// flecs.js-style entry point.
function connect(host = "http://localhost:27750") {
	return new FlecsClient(host);
}

module.exports = {
	FlecsClient,
	connect,
	SCENE_ACTIVE_TAG,
	INPUT_STATE_COMPONENT,
	INPUT_DRIVER_SYSTEMS,
	UI_BUTTON_COMPONENT,
	UI_CLICK_REQUEST_TAG
};

// ---- CLI ----
async function main(argv) {
	const args = argv.slice(2);

	// Extract --host and boolean flags; keep positional args in order.
	let host = process.env.FLECS_HOST || "http://localhost:27750";
	const flags = new Set();
	const positional = [];
	for (let i = 0; i < args.length; i++) {
		const a = args[i];
		if (a === "--host") host = args[++i];
		else if (a.startsWith("--")) flags.add(a.slice(2));
		else positional.push(a);
	}

	const [verb, arg1, arg2] = positional;
	const flecs = connect(host);
	const opts = {
		entityIds: flags.has("entity-ids"),
		values: flags.has("values"),
		fullPaths: flags.has("full-paths"),
	};

	const print = (r) => console.log(typeof r === "string" ? r : JSON.stringify(r, null, 2));

	switch (verb) {
		case "query": {
			// Guard: an unresolved component name throws a C++ exception inside the game process,
			// crashing it. Require at least one term to be qualified with an "engine." or "game."
			// prefix so bare names like "SoundEffect" (which will fail) are caught early.
			const terms = (arg1 || "").split(",").map((t) => t.trim());
			const unqualified = terms.filter(
				(t) => t && !t.startsWith("(") && !t.startsWith("#") && !/\b(engine|game)\./.test(t)
			);
			if (unqualified.length) {
				console.error(
					`[flecs-api] Query term(s) appear unqualified: ${unqualified.join(", ")}\n` +
					`Components registered in a module constructor need the full dot-path, e.g.:\n` +
					`  engine.audio.AudioModule.SoundEffect  (not SoundEffect)\n` +
					`  engine.ecs.Transform                  (registered directly, no module class)\n` +
					`An unresolved identifier crashes the game process. Aborting.`
				);
				process.exitCode = 1;
				break;
			}
			print(await flecs.query(arg1, opts));
			break;
		}
		case "entity":
			print(await flecs.entity(arg1, opts));
			break;
		case "add":
			print(await flecs.addComponent(arg1, arg2));
			break;
		case "remove":
			print(await flecs.removeComponent(arg1, arg2));
			break;
		case "toggle":
			print(await flecs.toggle(arg1, arg2 !== "false", positional[3]));
			break;
		case "switch-scene":
			// arg1 may be a SceneId identity name ("Game", "Title") or a raw "#<id>".
			print(await flecs.switchScene(arg1));
			break;
		case "scenes":
			print(await flecs.scenes());
			break;
		case "list-scenes":
			print(await flecs.listScenes());
			break;
		case "override-input":
			// arg1 is a component-value expression (JSON). Defaults to a held left mouse click.
			print(await flecs.overrideInput(arg1 || '{"mouse":{"left":{"pressed":true}}}'));
			break;
		case "restore-input":
			print(await flecs.restoreInput());
			break;
		case "pause":
			print(await flecs.setPaused(true));
			break;
		case "resume":
			print(await flecs.setPaused(false));
			break;
		case "toggle-pause":
			print(await flecs.togglePause());
			break;
		case "click":
			print(await flecs.click(arg1));
			break;
		case "list-buttons":
			print(await flecs.listButtons());
			break;
		case "paused":
			print({paused: await flecs.isPaused()});
			break;
		default:
			console.log(
				"Usage: node flecs-api.js <query|entity|add|remove|toggle|switch-scene|scenes|list-scenes|\n" +
				"                          click|list-buttons|pause|resume|toggle-pause|paused|\n" +
				"                          override-input|restore-input> [args] [--host URL]\n" +
				"       [--entity-ids] [--values] [--full-paths]\n" +
				"       switch-scene accepts a SceneId identity name (e.g. Game, Title) or a raw #id.\n" +
				"       click accepts a Button name (e.g. PlayButton) or a raw #id (see list-buttons).\n" +
				"Docs:  https://www.flecs.dev/flecs/md_docs_2FlecsRemoteApi.html"
			);
			process.exitCode = verb ? 1 : 0;
	}
}

if (require.main === module) {
	main(process.argv).catch((err) => {
		console.error("flecs-api error:", err.message);
		process.exitCode = 1;
	});
}
