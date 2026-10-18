<#
.SYNOPSIS
	Capture a screenshot of a running desktop build or a WASM build in headless Chrome.

.EXAMPLE
	# Desktop: capture the client area of a running game window (works behind other windows).
	pwsh tools/screenshot.ps1 -Process sandbox -Out sandbox.png

.EXAMPLE
	# Desktop, one instance: capture the window of the process you started (other instances may share its name).
	pwsh tools/screenshot.ps1 -Id $proc.Id -Out sandbox.png

.EXAMPLE
	# WASM: serve the build directory, load the page in headless Chrome, save a PNG and the console log.
	pwsh tools/screenshot.ps1 -Wasm build/wasm/Release -Page sandbox.html -Out web.png
#>
param(
	[string]$Process,
	[int]$Id,
	[string]$Wasm,
	[string]$Page = "index.html",
	[int]$Port = 8765,
	[int]$WaitMs = 8000,
	[string]$Out = "screenshot.png"
)

$ErrorActionPreference = "Stop"
$Out = [System.IO.Path]::GetFullPath($Out)

if ($Process -or $Id) {
	Add-Type -AssemblyName System.Drawing
	Add-Type @'
using System; using System.Runtime.InteropServices;
public static class ShotNative {
	[DllImport("user32.dll")] public static extern bool SetProcessDPIAware();
	[DllImport("user32.dll")] public static extern bool GetClientRect(IntPtr h, out RECT r);
	[DllImport("user32.dll")] public static extern bool PrintWindow(IntPtr h, IntPtr hdc, uint flags);
	public struct RECT { public int L, T, R, B; }
}
'@
	# Without DPI awareness the client rect is in scaled pixels and the capture is cropped.
	[ShotNative]::SetProcessDPIAware() | Out-Null
	$candidates = if ($Id) { Get-Process -Id $Id } else { Get-Process $Process }
	$proc = $candidates | Where-Object { $_.MainWindowHandle -ne 0 } | Select-Object -First 1
	if (-not $proc) { throw "No window for process '$(if ($Id) { $Id } else { $Process })'" }
	$rect = New-Object ShotNative+RECT
	[ShotNative]::GetClientRect($proc.MainWindowHandle, [ref]$rect) | Out-Null
	$bmp = New-Object System.Drawing.Bitmap ([Math]::Max(1, $rect.R)), ([Math]::Max(1, $rect.B))
	$gfx = [System.Drawing.Graphics]::FromImage($bmp)
	$hdc = $gfx.GetHdc()
	# 3 = PW_CLIENTONLY | PW_RENDERFULLCONTENT (captures GL content even when the window is covered).
	[ShotNative]::PrintWindow($proc.MainWindowHandle, $hdc, 3) | Out-Null
	$gfx.ReleaseHdc($hdc)
	$bmp.Save($Out)
	"$Out ($($rect.R)x$($rect.B))"
	return
}

if ($Wasm) {
	$chrome = @(
		"C:\Program Files\Google\Chrome\Application\chrome.exe",
		"C:\Program Files (x86)\Microsoft\Edge\Application\msedge.exe"
	) | Where-Object { Test-Path $_ } | Select-Object -First 1
	if (-not $chrome) { throw "Chrome or Edge not found" }

	$log = [System.IO.Path]::ChangeExtension($Out, ".console.log")
	$server = Start-Process python -ArgumentList "-m", "http.server", "$Port" -WorkingDirectory $Wasm -PassThru -WindowStyle Hidden
	try {
		Start-Sleep -Milliseconds 500
		# --timeout (not --virtual-time-budget): the emscripten main loop never lets virtual time run out.
		$chromeArgs = @(
			"--headless=new", "--enable-unsafe-swiftshader", "--use-angle=swiftshader", "--window-size=1280,800",
			"--timeout=$WaitMs", "--enable-logging=stderr", "--screenshot=$Out",
			"--user-data-dir=$env:TEMP\screenshot-chrome", "http://localhost:$Port/$Page"
		)
		$browser = Start-Process $chrome -ArgumentList $chromeArgs -RedirectStandardError $log -PassThru -WindowStyle Hidden
		$browser | Wait-Process -Timeout ([Math]::Ceiling($WaitMs / 1000) + 30) -ErrorAction SilentlyContinue
		if (-not $browser.HasExited) { $browser | Stop-Process -Force }
	}
	finally {
		$server | Stop-Process -Force -ErrorAction SilentlyContinue
	}
	if (-not (Test-Path $Out)) { throw "No screenshot written; see $log" }
	"$Out (console: $log)"
	return
}

throw "Pass -Id <pid>, -Process <name> or -Wasm <build dir>"
