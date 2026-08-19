// wasm_abort.js — injected before the WASM module via --pre-js.
//
// Emscripten calls Module.onAbort(msg) just before the process terminates with abort().
// Without this hook, a bare "Aborted(native code called abort())" appears in the console
// with only stripped WASM function offsets in the stack — no context about *why* it aborted.
//
// This hook:
//   1. Logs the Emscripten abort message (e.g. "Assertion failed" text from Flecs).
//   2. Captures a JavaScript stack trace so you can see which JS frame triggered the abort
//      (typically the emscripten abort() thunk → __abort_js → the offending C++ function).
//
// Note: WASM function frames still show as $funcNNNN in release builds. Build with
// cmake --build --preset wasm-debug to get named frames.

Module["onAbort"] = function (msg) {
	// Capture a JS stack so we can see the abort call site.
	var jsStack = new Error("Abort triggered here").stack || "(stack unavailable)";
	console.error("[wasm] Abort message : " + msg);
	console.error("[wasm] JavaScript stack:\n" + jsStack);
};
