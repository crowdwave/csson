// CSSON browser reader — drives a real Chrome over the DevTools Protocol
// (chrome-remote-interface). It injects the SHARED dependency-free reader
// (reader.js, browser built-ins only) and walks Chrome's authored `cssRules`
// tree. The Firefox driver (../firefox) injects the exact same reader.js, so the
// two browsers are tested with identical reader code (browser-parity invariant).
//
// Usage: node canon_browser.js <file-csson.css>
// Chrome binary: $CHROME_PATH or `google-chrome` on PATH.

const CDP = require('chrome-remote-interface');
const { spawn } = require('child_process');
const fs = require('fs');
const path = require('path');

const CHROME = process.env.CHROME_PATH || 'google-chrome';
const reader = fs.readFileSync(path.join(__dirname, 'reader.js'), 'utf8');

function launchChrome() {
  return new Promise((resolve, reject) => {
    const proc = spawn(CHROME, [
      '--headless=new', '--disable-gpu', '--no-sandbox',
      '--no-first-run', '--no-default-browser-check',
      '--remote-debugging-port=0', 'about:blank',
    ], { stdio: ['ignore', 'ignore', 'pipe'] });
    let buf = '';
    const onData = d => {
      buf += d.toString();
      const m = buf.match(/DevTools listening on (ws:\/\/\S+)/);
      if (m) { proc.stderr.removeListener('data', onData); resolve({ proc, wsUrl: m[1] }); }
    };
    proc.stderr.on('data', onData);
    proc.on('exit', code => reject(new Error('chrome exited early (code ' + code + ')')));
    setTimeout(() => reject(new Error('timed out waiting for DevTools endpoint')), 20000);
  });
}

(async () => {
  const css = fs.readFileSync(process.argv[2] || 'csson_v1-csson.css', 'utf8');
  const html = '<!DOCTYPE html><html><head><style id="s">' + css + '</style></head><body></body></html>';
  const { proc, wsUrl } = await launchChrome();
  const port = Number(new URL(wsUrl).port);
  let client;
  try {
    client = await CDP({ port });
    const { Page, Runtime } = client;
    await Page.enable();
    const { frameTree } = await Page.getFrameTree();
    await Page.setDocumentContent({ frameId: frameTree.frame.id, html });
    const { result, exceptionDetails } = await Runtime.evaluate({
      expression: reader + "\ncssonCanon(document.getElementById('s').sheet)",
      returnByValue: true,
    });
    if (exceptionDetails) throw new Error('in-page error: ' + JSON.stringify(exceptionDetails));
    console.log(result.value);
  } finally {
    if (client) await client.close();
    proc.kill();
  }
})().catch(e => { console.error(e); process.exit(1); });
