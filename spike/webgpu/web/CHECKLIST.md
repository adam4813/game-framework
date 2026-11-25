# Safari and phone checklist (best effort)

`dist/` is the Release web build. WebGPU needs a secure page: `localhost` or https.

- **Mac**: check out `spike/webgpu`, run `python3 -m http.server 8000` in `spike/webgpu/web/dist`, open
  `http://localhost:8000/webgpu_spike.html?--imgui&--frames&0`.
- **iPhone or Android**: the same files over https (any static host; the repo is public, so raw.githack.com serves them).

Expected: a field of small colored triangles that pulse, with the imgui demo window, and a green log in the corner.
Report, with a screenshot of the log:

1. Does it draw? If not, what does the message say? (macOS 15 and older: expected to say WebGPU is missing.)
2. The `adapter` and `adapter features` lines, and whether `timestamp-query=1`.
3. `?--offscreen` instead: does it print `PASS` with image hash `6f88cd2790c7da65`?
4. Resize the window or rotate the phone: does the picture stay sharp, and does `reconfigure` appear?
5. Mac: click, then P (pointer lock: `motion delta` lines as the mouse moves), Esc, then F (fullscreen, and F again).
6. Mouse wheel, keys, trackpad. Phone: touch drags the imgui window, `finger down` appears, the page doesn't scroll or zoom.
7. Leave the tab for a minute and come back: does it go on drawing? `device lost` in the log?
