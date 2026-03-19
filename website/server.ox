// server.ox — Oxide Language Showcase Website
//
// This is the official Oxide website, written entirely in Oxide itself.
// It demonstrates the language's HTTP server capabilities.
//
// Run with:  oxc --run website/server.ox
// Or:        ox run website/server.ox --port 6003

// ─────────────────────────────────────────────────────────────
// Shared CSS (inlined for zero-dependency serving)
// ─────────────────────────────────────────────────────────────

fn get_css() -> string {
    return ":root{--bg:#0a0a0b;--bg2:#111114;--card:#16161a;--border:#2a2a30;--text:#e8e8ec;--muted:#6b6b7a;--accent:#e05e3a;--accent2:#c94f2d;--glow:rgba(224,94,58,0.15);--code-bg:#13131a;--mono:'JetBrains Mono','Fira Code',monospace;}" +
    "*,*::before,*::after{box-sizing:border-box;margin:0;padding:0;}" +
    "html{font-size:16px;scroll-behavior:smooth;}" +
    "body{background:var(--bg);color:var(--text);font-family:'Inter',system-ui,sans-serif;line-height:1.6;min-height:100vh;}" +
    "a{color:var(--accent);text-decoration:none;}a:hover{text-decoration:underline;}" +
    "code,pre,tt{font-family:var(--mono);}" +
    "pre{background:var(--code-bg);border:1px solid var(--border);border-radius:8px;padding:1.25rem 1.5rem;overflow-x:auto;font-size:.875rem;line-height:1.7;}" +
    "code{background:var(--code-bg);border:1px solid var(--border);border-radius:4px;padding:.15em .4em;font-size:.875em;color:#a8b4c8;}" +
    "pre code{background:none;border:none;padding:0;font-size:inherit;color:inherit;}" +
    "h1,h2,h3,h4{line-height:1.25;font-weight:700;letter-spacing:-.02em;}" +
    "h1{font-size:clamp(2rem,5vw,3.5rem);}h2{font-size:clamp(1.4rem,3vw,2rem);margin-bottom:.75rem;}" +
    "h3{font-size:1.15rem;margin-bottom:.5rem;}p{margin-bottom:1rem;color:var(--text);}" +
    "ul,ol{padding-left:1.5rem;margin-bottom:1rem;}li{margin-bottom:.35rem;}" +
    ".container{max-width:1100px;margin:0 auto;padding:0 1.5rem;}" +
    ".hero{padding:4rem 0;text-align:center;background:linear-gradient(135deg,rgba(224,94,58,0.1) 0%,rgba(201,79,45,0.05) 100%);}" +
    ".nav{display:flex;gap:2rem;justify-content:center;padding:1.5rem 0;background:#111114;border-bottom:1px solid var(--border);}" +
    ".nav a{padding:.5rem 1rem;border-radius:4px;}" +
    ".nav a:hover{background:rgba(224,94,58,0.1);}" +
    ".section{padding:3rem 0;}" +
    ".feature-grid{display:grid;grid-template-columns:repeat(auto-fit,minmax(300px,1fr));gap:2rem;margin:2rem 0;}" +
    ".feature-card{background:var(--card);border:1px solid var(--border);border-radius:8px;padding:1.5rem;transition:all .3s ease;}" +
    ".feature-card:hover{border-color:var(--accent);transform:translateY(-4px);}" +
    ".code-example{background:var(--code-bg);border:1px solid var(--border);border-radius:8px;padding:1.5rem;margin:1rem 0;overflow-x:auto;}" +
    "footer{padding:2rem 0;border-top:1px solid var(--border);margin-top:4rem;text-align:center;color:var(--muted);}";
}

// ─────────────────────────────────────────────────────────────
// Main page renderer
// ─────────────────────────────────────────────────────────────

fn render_index() -> string {
    return "<!doctype html><html lang=en><head>" +
        "<meta charset=utf-8><meta name=viewport content='width=device-width,initial-scale=1'>" +
        "<title>Oxide — Fast, Safe, Modern Programming Language</title>" +
        "<style>" + get_css() + "</style>" +
        "</head><body><div class=container>" +
        "<div class=nav><a href=/>⌧ Oxide</a><a href=/docs>Docs</a><a href=/examples>Examples</a><a href=/playground>Playground</a></div>" +
        "<div class=hero><h1>Oxide</h1>" +
        "<p>Fast, safe, modern. Systems programming for the 21st century.</p>" +
        "</div><div class=section><h2>Why Oxide?</h2>" +
        "<div class=feature-grid>" +
        "<div class=feature-card><h3>⚡ Fast</h3><p>Compiled to native code. Zero runtime overhead. Speed of C.</p></div>" +
        "<div class=feature-card><h3>✔ Safe</h3><p>Type-safe by default. No null pointer panics. Immutability by default.</p></div>" +
        "<div class=feature-card><h3>📦 Modern</h3><p>Clean syntax. Strong type system. Expressive error handling.</p></div>" +
        "</div></div><div class=section><h2>Quick Example</h2>" +
        "<div class=code-example><pre>fn greet(name: string) -> void {
    print("Hello, " + name + "!");
}

let msg: string = "Oxide";
greet(msg);</pre></div>" +
        "</div><footer><p>Made with ❤️ in 2024 | <a href=https://github.com/ppenter/oxide-lang>GitHub</a></p></footer>" +
        "</div></body></html>";
}

fn render_docs() -> string {
    return "<!doctype html><html lang=en><head>" +
        "<meta charset=utf-8><title>Oxide — Documentation</title>" +
        "<style>" + get_css() + "</style>" +
        "</head><body><div class=container>" +
        "<div class=nav><a href=/>⌧ Oxide</a><a href=/docs>Docs</a><a href=/examples>Examples</a></div>" +
        "<h1>Documentation</h1>" +
        "<h2>Language Basics</h2>" +
        "<p>Learn the fundamentals of Oxide programming.</p>" +
        "<h3>Variables</h3><p>Use <code>let</code> for mutable variables and <code>const</code> for immutable ones.</p>" +
        "<h3>Functions</h3><p>Declare functions with <code>fn</code>. Specify parameter and return types explicitly.</p>" +
        "<h3>Control Flow</h3><p>Oxide supports if/else, while loops, and for-in range loops.</p>" +
        "<footer><p>Made with ❤️ in 2024</p></footer>" +
        "</div></body></html>";
}

fn render_examples() -> string {
    return "<!doctype html><html lang=en><head>" +
        "<meta charset=utf-8><title>Oxide — Examples</title>" +
        "<style>" + get_css() + "</style>" +
        "</head><body><div class=container>" +
        "<div class=nav><a href=/>⌧ Oxide</a><a href=/docs>Docs</a><a href=/examples>Examples</a></div>" +
        "<h1>Examples</h1>" +
        "<h2>Hello World</h2><div class=code-example><pre>fn greet(name: string) -> void { print("Hello, " + name); }
greet("Oxide");</pre></div>" +
        "<h2>Loops & Ranges</h2><div class=code-example><pre>for i in 0..10 { print(i); }</pre></div>" +
        "<h2>Structs</h2><div class=code-example><pre>struct Point { x: int; y: int; }
let p = Point { x: 3, y: 4 };
print(p.x);</pre></div>" +
        "<footer><p>Made with ❤️ in 2024</p></footer>" +
        "</div></body></html>";
}

// ─────────────────────────────────────────────────────────────
// Router definition
// ─────────────────────────────────────────────────────────────

let port: int = 6003;

router_get("/", "render_index");
router_get("/docs", "render_docs");
router_get("/examples", "render_examples");
router_start("0.0.0.0", port);
