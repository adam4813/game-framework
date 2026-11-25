// --pre-js: main waits for the device the shell asked for. If that fails, the shell shows why and main never runs.
Module["preRun"] = [].concat(Module["preRun"] || [], function () {
	addRunDependency("webgpu-device");
	Module["webgpuReady"].then(function (device) {
		Module["webgpuDevice"] = device;
		removeRunDependency("webgpu-device");
	}, function () {});
});
