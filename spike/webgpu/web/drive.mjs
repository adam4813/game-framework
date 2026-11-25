// Runs the web build in Chrome over CDP and prints its log.
//   node drive.mjs <dir> <mode> [--headed] [--shot <png>] [--query <args>] [-- <chrome flags>]
// Modes: run (wait for the program to finish or 12 s), input (mouse, keys, resize, pixel ratio), lock (pointer lock and
// fullscreen; only a headed Chrome grants them).
import { spawn } from "node:child_process";
import { createServer } from "node:http";
import { readFileSync, writeFileSync, mkdtempSync } from "node:fs";
import { tmpdir } from "node:os";
import { extname, join, normalize } from "node:path";

const argv = process.argv.slice(2);
const split = argv.indexOf("--");
const extra = split < 0 ? [] : argv.splice(split).slice(1);
const option = (name) => { const i = argv.indexOf(name); return i < 0 ? "" : argv.splice(i, 2)[1]; };
const flag = (name) => { const i = argv.indexOf(name); return i >= 0 && !!argv.splice(i, 1); };
const shotPath = option("--shot"), query = option("--query"), headed = flag("--headed"), noGpu = flag("--no-gpu"), host = option("--host") || "127.0.0.1";
const [dir, mode] = argv;
const sleep = (ms) => new Promise((r) => setTimeout(r, ms));

const types = { ".html": "text/html", ".js": "text/javascript", ".wasm": "application/wasm" };
const server = createServer((req, res) => {
	const path = normalize(new URL(req.url, "http://x").pathname).replace(/^([/\\])+/, "");
	try {
		const body = readFileSync(join(dir, path));
		res.writeHead(200, { "Content-Type": types[extname(path)] ?? "application/octet-stream", "Cache-Control": "no-store" }).end(body);
	} catch { res.writeHead(404).end(); }
}).listen(0, "127.0.0.1");
await new Promise((r) => server.on("listening", r));
const origin = option("--url") || `http://${host}:${server.address().port}`;

const profile = mkdtempSync(join(tmpdir(), "spike-chrome-"));
const chrome = spawn("C:\\Program Files\\Google\\Chrome\\Application\\chrome.exe", [
	...(headed ? ["--window-position=200,100"] : ["--headless=new"]), "--window-size=1000,700", "--no-first-run", "--no-default-browser-check",
	"--remote-debugging-port=0", `--user-data-dir=${profile}`, ...extra, "about:blank"], { stdio: ["ignore", "ignore", "pipe"] });
const wsUrl = await new Promise((resolve, reject) => {
	let text = "";
	chrome.stderr.on("data", (d) => { text += d; const m = text.match(/ws:\/\/[^\s]+/); if (m) resolve(m[0]); });
	setTimeout(() => reject(new Error("no devtools: " + text)), 15000);
});
const port = new URL(wsUrl).port;
const finishAll = (code) => { chrome.kill(); server.close(); process.exit(code); };
setTimeout(() => { console.log("DRIVER TIMEOUT"); finishAll(3); }, 90000);

let target;
for (let i = 0; i < 50 && !target; ++i) {
	target = (await (await fetch(`http://127.0.0.1:${port}/json`)).json()).find((t) => t.type === "page");
	if (!target) await sleep(100);
}
const ws = new WebSocket(target.webSocketDebuggerUrl);
await new Promise((r) => (ws.onopen = r));
let id = 0;
const pending = new Map();
ws.onmessage = (e) => {
	const m = JSON.parse(e.data);
	if (m.id) { pending.get(m.id)?.(m.result ?? { error: m.error }); pending.delete(m.id); }
	else if (m.method === "Runtime.consoleAPICalled") console.log(`[${m.params.type}] ` + m.params.args.map((a) => a.value ?? a.description).join(" "));
	else if (m.method === "Log.entryAdded") console.log(`[log.${m.params.entry.level}] ${m.params.entry.text}`);
	else if (m.method === "Runtime.exceptionThrown") console.log("[exception] " + JSON.stringify(m.params.exceptionDetails).slice(0, 600));
};
const send = (method, params = {}) => new Promise((r) => { pending.set(++id, r); ws.send(JSON.stringify({ id, method, params })); });
const js = async (expression) => {
	const r = await send("Runtime.evaluate", { expression, returnByValue: true, awaitPromise: true, userGesture: true });
	return r.result?.value ?? r.exceptionDetails?.exception?.description;
};
const report = async (label) => console.log(`-- ${label}: ` + JSON.stringify(await js(`(() => { const c = document.getElementById("canvas"), r = c.getBoundingClientRect();
	return { canvas: [c.width, c.height], css: [r.width, r.height], dpr: devicePixelRatio, inner: [innerWidth, innerHeight],
		lock: !!document.pointerLockElement, full: !!document.fullscreenElement, focus: document.hasFocus() }; })()`)));
const key = async (k, code, vk) => {
	const common = { key: k, code, windowsVirtualKeyCode: vk, nativeVirtualKeyCode: vk };
	await send("Input.dispatchKeyEvent", { type: "keyDown", ...common, text: k.length === 1 ? k : undefined });
	await sleep(80);
	await send("Input.dispatchKeyEvent", { type: "keyUp", ...common });
	await sleep(1200);
};
const mouse = (type, x, y, more = {}) => send("Input.dispatchMouseEvent", { type, x, y, ...more });
const metrics = (width, height, deviceScaleFactor) => send("Emulation.setDeviceMetricsOverride", { width, height, deviceScaleFactor, mobile: false });

await send("Runtime.enable"); await send("Log.enable"); await send("Page.enable");
if (headed) await send("Page.bringToFront");
if (noGpu) await send("Page.addScriptToEvaluateOnNewDocument", { source: "Object.defineProperty(Navigator.prototype, 'gpu', { value: undefined });" });
console.log("browser: " + (await send("Browser.getVersion")).product);
await send("Page.navigate", { url: `${origin}/webgpu_spike.html${query ? "?" + query : ""}` });
await sleep(3000);
await report("loaded");

if (mode === "run") {
	for (let i = 0; i < 24; ++i) {
		if ((await js("Module.spikeFinished !== undefined || !!Module.webgpuFailed"))) break;
		await sleep(500);
	}
	console.log("-- exit code " + (await js("Module.spikeFinished")) + ", status: " + (await js(`document.getElementById("status").textContent`)));
} else if (mode === "input") {
	await mouse("mouseMoved", 100, 120);
	await mouse("mousePressed", 100, 120, { button: "left", clickCount: 1 });
	await mouse("mouseReleased", 100, 120, { button: "left", clickCount: 1 });
	await mouse("mouseWheel", 100, 120, { deltaX: 0, deltaY: 120 });
	await sleep(300);
	await key("a", "KeyA", 65);
	await metrics(900, 500, 1); await sleep(700); await report("resized 900x500");
	await metrics(900, 500, 2); await sleep(700); await report("ratio 2, same size");
	await mouse("mousePressed", 450, 250, { button: "left", clickCount: 1 });
	await mouse("mouseReleased", 450, 250, { button: "left", clickCount: 1 });
	await sleep(300);
	await metrics(700, 400, 2); await sleep(700); await report("700x400 at ratio 2");
} else if (mode === "lock") {
	await mouse("mousePressed", 300, 300, { button: "left", clickCount: 1 });
	await mouse("mouseReleased", 300, 300, { button: "left", clickCount: 1 });
	await sleep(300);
	await key("p", "KeyP", 80); await report("after P (lock)");
	for (const x of [310, 330, 360]) { await mouse("mouseMoved", x, 300); await sleep(100); }
	await key("p", "KeyP", 80); await report("after P (unlock)");
	await key("f", "KeyF", 70); await sleep(800); await report("after F (fullscreen)");
	await key("f", "KeyF", 70); await sleep(800); await report("after F (leave)");
}
else if (mode === "crash") {
	fetch(`http://127.0.0.1:${port}/json/new?chrome://gpucrash`, { method: "PUT" });
	await sleep(4000);
	console.log("-- status: " + (await js(`document.getElementById("status").textContent`)));
}
if (shotPath) {
	const r = await send("Page.captureScreenshot", { format: "png" });
	writeFileSync(shotPath, Buffer.from(r.data, "base64"));
}
finishAll(0);
