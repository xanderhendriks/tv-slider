const esbuild = require("esbuild");
const fs = require("fs");
const path = require("path");

const root = process.cwd();
const srcDir = path.join(root, "web_src");
const outDir = path.join(root, "main", "assets");

function ensureDir(dir) {
  fs.mkdirSync(dir, { recursive: true });
}

function readFile(filePath) {
  return fs.readFileSync(filePath, "utf8");
}

function copyFileToOut(srcPath, fileName) {
  const destPath = path.join(outDir, fileName);
  fs.copyFileSync(srcPath, destPath);
}

function mimeFor(filePath) {
  const ext = path.extname(filePath).toLowerCase();
  if (ext === ".png") return "image/png";
  if (ext === ".jpg" || ext === ".jpeg") return "image/jpeg";
  if (ext === ".svg") return "image/svg+xml";
  if (ext === ".ico") return "image/x-icon";
  return "application/octet-stream";
}

function dataUriFor(filePath) {
  const data = fs.readFileSync(filePath);
  const mime = mimeFor(filePath);
  return `data:${mime};base64,${data.toString("base64")}`;
}

function inlineCssUrls(css) {
  return css.replace(/url\\(([^)]+)\\)/g, (match, rawUrl) => {
    const cleaned = rawUrl.trim().replace(/^['"]|['"]$/g, "");
    if (cleaned.startsWith("data:") || cleaned.startsWith("http")) {
      return match;
    }
    const imgPath = path.join(srcDir, cleaned);
    if (!fs.existsSync(imgPath)) {
      return match;
    }
    return `url(${dataUriFor(imgPath)})`;
  });
}

async function buildAll() {
  ensureDir(outDir);

  const jsResult = await esbuild.build({
    entryPoints: [path.join(srcDir, "javascript", "tv-slider.js")],
    bundle: true,
    minify: true,
    sourcemap: false,
    treeShaking: false,
    write: false,
    logLevel: "info",
  });
  const appJs = jsResult.outputFiles[0].text;

  const cssResult = await esbuild.build({
    entryPoints: [path.join(srcDir, "stylesheets", "tv-slider.css")],
    bundle: true,
    minify: true,
    sourcemap: false,
    write: false,
    logLevel: "info",
    loader: { ".css": "css" },
  });
  let appCss = cssResult.outputFiles[0].text;
  appCss = inlineCssUrls(appCss);

  const jquery = readFile(path.join(srcDir, "javascript", "jquery-1.12.4.min.js"));
  const faviconSrc = path.join(srcDir, "favicon.ico");

  let html = readFile(path.join(srcDir, "index.html"));
  html = html.replace(
    /<link[^>]*href=["']stylesheets\/tv-slider\.css["'][^>]*>/i,
    "__INLINE_CSS__"
  );
  html = html.replace(
    /<script[^>]*src=["']javascript\/jquery-1\.12\.4\.min\.js["'][^>]*><\/script>/i,
    "__INLINE_JQUERY__"
  );
  html = html.replace(
    /<script[^>]*src=["']javascript\/tv-slider\.js["'][^>]*><\/script>/i,
    "__INLINE_APP__"
  );

  html = html.replace(/src=["']images\/([^"']+)["']/g, (match, file) => {
    const imgPath = path.join(srcDir, "images", file);
    if (!fs.existsSync(imgPath)) {
      return match;
    }
    return `src="${dataUriFor(imgPath)}"`;
  });
  html = html.replace(
    /href=["']favicon\.ico["']/i,
    `href="${dataUriFor(faviconSrc)}"`
  );

  const safeJquery = jquery.replace(/<\/script>/gi, "<\\/script>");
  const safeAppJs = appJs.replace(/<\/script>/gi, "<\\/script>");

  html = html.replace("__INLINE_CSS__", () => `<style>${appCss}</style>`);
  html = html.replace("__INLINE_JQUERY__", () => `<script>${safeJquery}</script>`);
  html = html.replace("__INLINE_APP__", () => `<script>${safeAppJs}</script>`);

  const hasStaticRefs =
    /<script[^>]*src=["']javascript\/tv-slider\.js["'][^>]*><\/script>/i.test(html) ||
    /<script[^>]*src=["']javascript\/jquery-1\.12\.4\.min\.js["'][^>]*><\/script>/i.test(html) ||
    /<link[^>]*href=["']stylesheets\/tv-slider\.css["'][^>]*>/i.test(html);

  if (html.includes("__INLINE_") || hasStaticRefs) {
    throw new Error("Inline replacement failed; static references still present in index.html");
  }

  fs.writeFileSync(path.join(outDir, "index.html"), html);
  if (fs.existsSync(faviconSrc)) {
    copyFileToOut(faviconSrc, "favicon.ico");
  }
}

const watch = process.argv.includes("--watch");

if (watch) {
  buildAll()
    .then(() => {
      console.log("Watching for changes...");
      fs.watch(srcDir, { recursive: true }, () => {
        buildAll().catch((err) => console.error(err));
      });
    })
    .catch((err) => {
      console.error(err);
      process.exit(1);
    });
} else {
  buildAll().catch((err) => {
    console.error(err);
    process.exit(1);
  });
}
