#!/usr/bin/env node
/**
 * flecs-mcp.js — Model Context Protocol server exposing the Flecs Remote API as agent tools.
 *
 * This is a thin, zero-dependency adapter over the FlecsClient in ./flecs-api.js. The client is
 * the capability layer (query/entity/component/scene/pause/input helpers over the engine's Flecs
 * REST API on http://localhost:27750); this file wraps each capability as an MCP tool so any MCP
 * client (Claude Desktop, Cursor, an IDE, another agent) can discover and invoke them without
 * knowing anything about REST.
 *
 * Transport: JSON-RPC 2.0 over stdio, newline-delimited (one JSON object per line), per the MCP
 * stdio transport. stdout carries ONLY protocol messages; all diagnostics go to stderr.
 *
 * ---------------------------------------------------------------------------------------------
 * IMPORTANT — entity ids are runtime-assigned. Flecs hands out entity ids (game objects,
 * singletons) at runtime, so they differ on every launch. NEVER hardcode an id like "#806".
 * Scenes are the exception you don't have to worry about: each carries a stable SceneId identity,
 * so flecs_switch_scene takes an identity NAME ("Game"/"Title") — use flecs_list_scenes to see
 * them. For other objects, discover ids fresh via flecs_query. The pause tools resolve the Paused
 * tag's id for you, so they need no ids.
 *
 * ---------------------------------------------------------------------------------------------
 * Run it (the game must be running so the Flecs REST API is listening):
 *   node tools/flecs-mcp.js                       # defaults to http://localhost:27750
 *   node tools/flecs-mcp.js --host http://host:port
 *   FLECS_HOST=http://host:port node tools/flecs-mcp.js
 *
 * Register in Claude Desktop (claude_desktop_config.json):
 *   {
 *     "mcpServers": {
 *       "flecs": {
 *         "command": "node",
 *         "args": ["<abs>/tools/flecs-mcp.js"],
 *         "env": { "FLECS_HOST": "http://localhost:27750" }
 *       }
 *     }
 *   }
 *
 * Register in Cursor (.cursor/mcp.json, same shape):
 *   { "mcpServers": { "flecs": { "command": "node", "args": ["<abs>/tools/flecs-mcp.js"] } } }
 *
 * Use with GitHub Copilot CLI (repo-scoped, opt-in per session — the CLI only auto-loads the
 * global ~/.copilot/mcp-config.json, so this repo ships .github/mcp-config.json and you pass it):
 *   copilot --additional-mcp-config @.github/mcp-config.json
 * (run from the repo root so the relative tools/flecs-mcp.js path resolves).
 *
 * ---------------------------------------------------------------------------------------------
 * ADDING A TOOL (the common case — expect this to grow):
 *   Append one object to the `tools` array below:
 *     {
 *       name: "flecs_do_thing",
 *       description: "One line the agent sees.",
 *       inputSchema: { type: "object", properties: { foo: { type: "string" } }, required: ["foo"] },
 *       handler: (args) => flecs.doThing(args.foo),   // may be async; return any JSON value
 *     }
 *   The handler's return value is JSON-stringified into an MCP text content block automatically;
 *   throwing yields an isError result. No transport code to touch.
 *
 * MIGRATING TO @modelcontextprotocol/sdk LATER (if you ever want the official SDK):
 *   The `tools` array is transport-agnostic. Swapping transports is a mechanical loop — roughly:
 *     for (const t of tools) server.tool(t.name, t.description, t.inputSchema, t.handler);
 *   plus `new StdioServerTransport()`. The tool definitions (the valuable part) are unchanged;
 *   only the ~40 lines of JSON-RPC plumbing at the bottom of this file go away.
 */

"use strict";

const {connect} = require("./flecs-api");

const SERVER_NAME = "flecs-remote";
const SERVER_VERSION = "0.1.0";
const DEFAULT_PROTOCOL = "2025-06-18";

// ---- host / client ----
let host = process.env.FLECS_HOST || "http://localhost:27750";
{
	const argv = process.argv.slice(2);
	for (let i = 0; i < argv.length; i++) {
		if (argv[i] === "--host") host = argv[++i];
	}
}
const flecs = connect(host);

const log = (...a) => process.stderr.write("[flecs-mcp] " + a.map(String).join(" ") + "\n");

// ---------------------------------------------------------------------------------------------
// Tool registry. Each entry: { name, description, inputSchema (JSON Schema), handler(args)->any }.
// Handlers reuse the exported FlecsClient, so this file adds no new behaviour — only exposure.
// ---------------------------------------------------------------------------------------------
const tools = [
	{
		name: "flecs_query",
		description:
			'Run a Flecs query expression (e.g. "engine.ecs.Transform" or "Position, Velocity") and return matching entities.',
		inputSchema: {
			type: "object",
			properties: {
				expr: {type: "string", description: "Flecs query expression."},
				entity_ids: {type: "boolean", description: "Include numeric entity ids."},
				values: {type: "boolean", description: "Include component values."},
				full_paths: {type: "boolean", description: "Return full entity paths instead of leaf names."},
			},
			required: ["expr"],
		},
		handler: (a) =>
			flecs.query(a.expr, {entityIds: a.entity_ids, values: a.values, fullPaths: a.full_paths}),
	},
	{
		name: "flecs_entity",
		description: 'Fetch a single entity by path or id ("SunLight", "FallingCube.CubeJumpScript", "#806").',
		inputSchema: {
			type: "object",
			properties: {
				path: {type: "string", description: "Entity path or #id."},
				values: {type: "boolean", description: "Include component values."},
			},
			required: ["path"],
		},
		handler: (a) => flecs.entity(a.path, {values: a.values}),
	},
	{
		name: "flecs_add",
		description:
			"Add a tag, or set a component value on an entity. Provide `value` (JSON / flecs-expr string) to set data. Confirms the component is present before returning (mutations are deferred a frame or two); `confirmed:false` means it was not observed within the retry window (or is unverifiable when the component is given as a raw #id).",
		inputSchema: {
			type: "object",
			properties: {
				entity: {type: "string", description: "Target entity path or #id."},
				component: {type: "string", description: "Component/tag path or #id."},
				value: {type: "string", description: "Optional component value payload."},
			},
			required: ["entity", "component"],
		},
		handler: async (a) => {
			await flecs.addComponent(a.entity, a.component, a.value);
			const confirmed = await flecs.waitFor(async () => {
				const has = await flecs.hasComponent(a.entity, a.component);
				return has === null ? true : has === true;
			});
			return {entity: a.entity, component: a.component, added: true, confirmed};
		},
	},
	{
		name: "flecs_remove",
		description:
			"Remove a component/tag from an entity. Confirms the component is gone before returning (mutations are deferred a frame or two); `confirmed:false` means removal was not observed within the retry window (or is unverifiable when the component is given as a raw #id).",
		inputSchema: {
			type: "object",
			properties: {
				entity: {type: "string", description: "Target entity path or #id."},
				component: {type: "string", description: "Component/tag path or #id."},
			},
			required: ["entity", "component"],
		},
		handler: async (a) => {
			await flecs.removeComponent(a.entity, a.component);
			const confirmed = await flecs.waitFor(async () => {
				const has = await flecs.hasComponent(a.entity, a.component);
				return has === null ? true : has === false;
			});
			return {entity: a.entity, component: a.component, removed: true, confirmed};
		},
	},
	{
		name: "flecs_toggle",
		description:
			"Enable/disable an entity (or one component on it) via the Disabled tag — the safe way to pause a single system without a rebuild.",
		inputSchema: {
			type: "object",
			properties: {
				entity: {type: "string", description: "Target entity path or #id."},
				enable: {type: "boolean", description: "true to enable, false to disable."},
				component: {type: "string", description: "Optional component to toggle instead of the whole entity."},
			},
			required: ["entity", "enable"],
		},
		handler: (a) => flecs.toggle(a.entity, a.enable, a.component),
	},
	{
		name: "flecs_scenes",
		description: "List scene entities that currently carry the Active tag.",
		inputSchema: {type: "object", properties: {}},
		handler: () => flecs.scenes(),
	},
	{
		name: "flecs_list_scenes",
		description:
			"List every registered scene with its SceneId identity name (e.g. \"Title\", \"Game\"), runtime id, and active flag. Use the identity name with flecs_switch_scene — it is stable across launches.",
		inputSchema: {type: "object", properties: {}},
		handler: () => flecs.listScenes(),
	},
	{
		name: "flecs_switch_scene",
		description:
			'Switch the running scene by SceneId identity name (e.g. "Game" or "Title") — resolved via the scene\'s (SceneId, <name>) pair, so it is stable across launches and needs no runtime id. A raw "#<id>" is also accepted. Use flecs_list_scenes to see the available identity names.',
		inputSchema: {
			type: "object",
			properties: {
				scene: {
					type: "string",
					description: 'SceneId identity name (e.g. "Game", "Title"), or a raw #id.',
				},
			},
			required: ["scene"],
		},
		handler: (a) => flecs.switchScene(a.scene),
	},
	{
		name: "flecs_pause",
		description:
			"Pause the active scene (add the Paused tag). Freezes engine.ecs.Pausable systems (physics, gameplay scripts) while input/rendering/UI keep running.",
		inputSchema: {type: "object", properties: {}},
		handler: () => flecs.setPaused(true),
	},
	{
		name: "flecs_resume",
		description: "Resume the active scene (remove the Paused tag).",
		inputSchema: {type: "object", properties: {}},
		handler: () => flecs.setPaused(false),
	},
	{
		name: "flecs_toggle_pause",
		description: "Flip the current pause state.",
		inputSchema: {type: "object", properties: {}},
		handler: () => flecs.togglePause(),
	},
	{
		name: "flecs_paused",
		description: "Report whether the active scene is currently paused.",
		inputSchema: {type: "object", properties: {}},
		handler: async () => ({paused: await flecs.isPaused()}),
	},
	{
		name: "flecs_override_input",
		description:
			"Disable InputPoll and inject an InputState value for several frames (e.g. a held mouse click), then re-enable InputPoll.",
		inputSchema: {
			type: "object",
			properties: {
				state: {
					type: "object",
					description: 'InputState override, e.g. {"mouse":{"left":{"pressed":true}}}.',
				},
				frames: {type: "integer", description: "How many frames to hold (default 6)."},
				interval_ms: {type: "integer", description: "Delay between injections in ms (default 150)."},
			},
			required: ["state"],
		},
		handler: (a) => flecs.overrideInput(a.state, {frames: a.frames, intervalMs: a.interval_ms}),
	},
	{
		name: "flecs_restore_input",
		description: "Re-enable InputPoll if flecs_override_input was interrupted.",
		inputSchema: {type: "object", properties: {}},
		handler: () => flecs.restoreInput(),
	},
];

const toolByName = new Map(tools.map((t) => [t.name, t]));

// ---------------------------------------------------------------------------------------------
// JSON-RPC 2.0 over stdio (newline-delimited). This block is the only transport-specific code;
// see the migration note in the header.
// ---------------------------------------------------------------------------------------------
function send(msg) {
	process.stdout.write(JSON.stringify(msg) + "\n");
}

function reply(id, result) {
	send({jsonrpc: "2.0", id, result});
}

function replyError(id, code, message) {
	send({jsonrpc: "2.0", id, error: {code, message}});
}

async function handleRequest(msg) {
	const {id, method, params} = msg;
	switch (method) {
		case "initialize":
			reply(id, {
				protocolVersion: (params && params.protocolVersion) || DEFAULT_PROTOCOL,
				capabilities: {tools: {}},
				serverInfo: {name: SERVER_NAME, version: SERVER_VERSION},
			});
			return;
		case "tools/list":
			reply(id, {
				tools: tools.map((t) => ({name: t.name, description: t.description, inputSchema: t.inputSchema})),
			});
			return;
		case "tools/call": {
			const name = params && params.name;
			const tool = toolByName.get(name);
			if (!tool) {
				replyError(id, -32602, `Unknown tool: ${name}`);
				return;
			}
			try {
				const result = await tool.handler((params && params.arguments) || {});
				const text = typeof result === "string" ? result : JSON.stringify(result, null, 2);
				reply(id, {content: [{type: "text", text}]});
			} catch (err) {
				const message = err && err.message ? err.message : String(err);
				reply(id, {content: [{type: "text", text: `Error: ${message}`}], isError: true});
			}
			return;
		}
		case "ping":
			reply(id, {});
			return;
		default:
			replyError(id, -32601, `Method not found: ${method}`);
	}
}

function handleMessage(msg) {
	if (msg == null || typeof msg !== "object") return;
	// Notifications (no id) expect no response — e.g. notifications/initialized.
	if (msg.id === undefined || msg.id === null) return;
	enqueue(msg);
}

// Requests are async (they hit the REST API) and some mutate then confirm across several frames.
// We serialize them through a FIFO promise chain so a scripted sequence like switch-scene ->
// pause -> check runs in arrival order instead of racing. `pending` also keeps the process alive
// until every queued reply has been written (interactive clients keep stdin open; a piped batch
// closes it, and we exit only once the queue has drained).
let pending = 0;
let stdinEnded = false;
let chain = Promise.resolve();

function maybeExit() {
	if (stdinEnded && pending === 0) process.exit(0);
}

function enqueue(msg) {
	pending++;
	chain = chain.then(async () => {
		try {
			await handleRequest(msg);
		} catch (e) {
			replyError(msg.id, -32603, `Internal error: ${e && e.message}`);
		} finally {
			pending--;
			maybeExit();
		}
	});
}

let buffer = "";
process.stdin.setEncoding("utf8");
process.stdin.on("data", (chunk) => {
	buffer += chunk;
	let idx;
	while ((idx = buffer.indexOf("\n")) >= 0) {
		const line = buffer.slice(0, idx).trim();
		buffer = buffer.slice(idx + 1);
		if (!line) continue;
		let msg;
		try {
			msg = JSON.parse(line);
		} catch {
			log("bad JSON:", line);
			continue;
		}
		handleMessage(msg);
	}
});
process.stdin.on("end", () => {
	stdinEnded = true;
	maybeExit();
});

log(`ready — ${tools.length} tools, host ${host}`);
