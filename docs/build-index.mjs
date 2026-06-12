// Builds docs/index.html for the CSSON GitHub Pages site, embedding six real
// example documents copied verbatim from ../samples. Run: node docs/build-index.mjs
import { readFileSync, writeFileSync } from "node:fs";
import { fileURLToPath } from "node:url";
import { dirname, join } from "node:path";

const here = dirname(fileURLToPath(import.meta.url));
const showcase = join(here, "..", "samples", "showcase");

const esc = (s) => s.replace(/&/g, "&amp;").replace(/</g, "&lt;").replace(/>/g, "&gt;");

// Six examples, copied from the samples dir (simple -> sophisticated, varied domains).
const EXAMPLES = [
  ["hello-minimal", "the smallest valid CSSON"],
  ["recipe-lasagna", "a cooking recipe"],
  ["web-server", "reverse proxy + virtual hosts"],
  ["rpg-character", "a game character sheet"],
  ["design-tokens", "a design-system token set"],
  ["spacecraft-mission", "a deep-space mission profile"],
];

const exampleHtml = EXAMPLES.map(([name, desc]) => {
  const src = readFileSync(join(showcase, `${name}-csson.css`), "utf8").replace(/\s+$/, "");
  return `      <figure class="example">
        <figcaption class="cap"><b>${name}-csson.css</b> — ${desc}</figcaption>
        <pre><code>${esc(src)}</code></pre>
      </figure>`;
}).join("\n");

const html = `<!DOCTYPE html>
<html lang="en">
<head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width, initial-scale=1">
<title>CSSON — a data format which is a strict subset of CSS</title>
<meta name="description" content="CSSON is a data format which is a strict subset of CSS. Configuration data that is also a valid stylesheet — read, edit and validate it from the CLI, Node.js, Python or the browser.">
<link rel="stylesheet" href="site.css">
</head>
<body>
<header class="site"><div class="wrap">
  <span class="brand">CSSON</span>
  <nav>
    <a href="index.html" class="active">Home</a>
    <a href="cli.html">CLI</a>
    <a href="nodejs.html">Node.js</a>
    <a href="python.html">Python</a>
    <a href="browser.html">Browser</a>
    <a href="css-data-complete-guide.html">Guide</a>
    <a href="https://github.com/crowdwave/csson">GitHub</a>
  </nav>
</div></header>

<main>
  <section class="hero">
    <h1>CSSON</h1>
    <p class="tagline"><b>CSSON is a data format which is a strict subset of CSS.</b></p>
    <p class="sub">Your configuration <em>is</em> a valid stylesheet: records are CSS blocks,
    fields are custom properties (<code>--key</code>), schema is <code>@property</code>, and comments
    are native. Every CSSON engine — the C library, the browser, Node.js, Python — reads it the same
    way, byte for byte. Files use the <code>.css</code> extension, named <code>&lt;name&gt;-csson.css</code>.</p>
  </section>

  <h2 id="examples">Six examples</h2>
  <p class="muted">Copied verbatim from the <a href="https://github.com/crowdwave/csson/tree/main/samples">samples</a> — simple to sophisticated.</p>
  <div class="examples">
${exampleHtml}
  </div>

  <h2 id="use">Use it from your language</h2>
  <div class="cards">
    <a class="card" href="cli.html"><h3>CLI →</h3><p>The <code>csson</code> command: canon, get, set, patch, validate.</p></a>
    <a class="card" href="nodejs.html"><h3>Node.js →</h3><p><code>@csson/js</code> — runs the wasm core, no native build.</p></a>
    <a class="card" href="python.html"><h3>Python →</h3><p><code>ctypes</code> FFI over <code>libcsson</code>, zero dependencies.</p></a>
    <a class="card" href="browser.html"><h3>Browser →</h3><p>Read via the native CSSOM; edit via the wasm core.</p></a>
  </div>

  <h2 id="what">What CSSON is (and isn't)</h2>
  <p>CSSON reuses a real CSS parser everywhere instead of inventing a new format. From CSS it
  inherits comments, a typed value grammar, <code>@property</code> schema, and — crucially —
  browser-loadability: a CSSON file served as <code>text/css</code> parses natively in any browser.</p>
  <table>
    <tr><th>CSSON is</th><th>CSSON is not</th></tr>
    <tr><td>a config/data format that is valid CSS</td><td>a styling language or cascade system</td></tr>
    <tr><td>records (blocks) + fields (<code>--props</code>)</td><td>a full CSS engine</td></tr>
    <tr><td>deterministic + byte-identical across engines</td><td>JSON with different punctuation</td></tr>
  </table>
  <div class="note">A bare integer becomes a JSON number; a quoted string becomes a string;
  <b>everything else</b> (decimals like <code>1.5</code>, units like <code>30s</code>, colors,
  <code>url(...)</code>) is kept as an exact string. Every named block is a JSON <b>array</b> (a
  single block is a one-element list), mirroring the browser CSSOM. See the
  <a href="css-data-complete-guide.html">complete guide</a>.</div>
</main>

<footer class="site">CSSON — a data format which is a strict subset of CSS · <a href="https://github.com/crowdwave/csson">crowdwave/csson</a></footer>
</body>
</html>
`;

writeFileSync(join(here, "index.html"), html);
console.error(`index.html written (${EXAMPLES.length} examples embedded)`);
