// CSSON Firefox reader — drives stock Mozilla Firefox (Gecko) headless via
// geckodriver + the W3C WebDriver protocol (plain HTTP, Node built-in fetch, no
// npm deps). It injects the SAME dependency-free reader as the Chrome driver
// (../browser/reader.js) and runs it against Firefox's built-in CSSOM, to prove
// cross-browser parity (browser-parity invariant — see ../../../../CLAUDE.md).
//
// Usage: node canon_firefox.js <file.csson>
// Binaries: $GECKODRIVER / $FIREFOX_BIN, else the ones under tmp/tools/.
const { spawn } = require("child_process");
const fs = require("fs");
const path = require("path");

const PORT = Number(process.env.GECKO_PORT || 4444);
const HOST = `http://127.0.0.1:${PORT}`;
const GECKO = process.env.GECKODRIVER || path.resolve(__dirname, "../../../../tmp/tools/geckodriver");
const FIREFOX = process.env.FIREFOX_BIN || path.resolve(__dirname, "../../../../tmp/tools/firefox/firefox");
const reader = fs.readFileSync(path.join(__dirname, "..", "browser", "reader.js"), "utf8");
const sleep = (ms) => new Promise((r) => setTimeout(r, ms));

async function wd(method, p, body) {
  const res = await fetch(HOST + p, {
    method, headers: { "Content-Type": "application/json" },
    body: body ? JSON.stringify(body) : undefined,
  });
  const json = await res.json().catch(() => ({}));
  if (!res.ok) throw new Error(`WebDriver ${p}: ${JSON.stringify(json)}`);
  return json.value;
}
async function waitReady() {
  for (let i = 0; i < 150; i++) {
    try { const v = await (await fetch(HOST + "/status")).json(); if (v.value && v.value.ready) return; } catch {}
    await sleep(100);
  }
  throw new Error("geckodriver did not become ready");
}

(async () => {
  const css = fs.readFileSync(process.argv[2], "utf8");
  const gd = spawn(GECKO, ["--port", String(PORT)], { stdio: ["ignore", "ignore", "pipe"] });
  let sid;
  try {
    await waitReady();
    const session = await wd("POST", "/session", {
      capabilities: { alwaysMatch: { "moz:firefoxOptions": { args: ["-headless"], binary: FIREFOX } } },
    });
    sid = session.sessionId;
    // Inject the shared reader, build a stylesheet from the file text, walk it.
    const script = reader +
      "\nvar css = arguments[0];" +
      "var s = document.createElement('style'); s.textContent = css; document.head.appendChild(s);" +
      "return cssonCanon(s.sheet);";
    const out = await wd("POST", `/session/${sid}/execute/sync`, { script, args: [css] });
    console.log(out);
  } finally {
    if (sid) { try { await wd("DELETE", `/session/${sid}`); } catch {} }
    gd.kill();
  }
})().catch((e) => { console.error(e); process.exit(1); });
