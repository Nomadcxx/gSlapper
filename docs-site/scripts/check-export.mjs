import assert from 'node:assert/strict';
import { readFileSync, readdirSync, statSync } from 'node:fs';
import { join } from 'node:path';
import { fileURLToPath } from 'node:url';

const output = new URL('../out/', import.meta.url);
const basePath = process.env.GITHUB_ACTIONS === 'true' ? '/gSlapper' : '';
const rootHtml = readFileSync(new URL('index.html', output), 'utf8');
const docsHtml = readFileSync(new URL('docs/index.html', output), 'utf8');
const articleHtml = readFileSync(new URL('docs/user-guide/ipc-control/index.html', output), 'utf8');
const installationHtml = readFileSync(new URL('docs/getting-started/installation/index.html', output), 'utf8');
const searchPath = new URL('api/search', output);
const chunkRoot = fileURLToPath(new URL('_next/static/chunks/', output));

function files(directory) {
  return readdirSync(directory, { withFileTypes: true }).flatMap((entry) => {
    const path = join(directory, entry.name);
    return entry.isDirectory() ? files(path) : [path];
  });
}

assert.match(docsHtml, /brand\/gsp-mark\.png/, 'exported header is missing the gsp mark');
assert.ok(
  docsHtml.includes(`src="${basePath}/brand/gsp-mark.png"`),
  'exported gsp mark is missing the deployment base path',
);
assert.doesNotMatch(docsHtml, /Toggle Theme|light theme/i, 'dark-only export contains a theme toggle');
assert.match(
  docsHtml,
  /https:\/\/github\.com\/Nomadcxx\/gSlapper/,
  'exported header is missing the repository link',
);
assert.match(
  articleHtml,
  /blob\/master\/docs-site\/content\/docs\/user-guide\/ipc-control\.(md|mdx)/,
  'article source link does not target the migrated content',
);
assert.ok(statSync(searchPath).size > 100, 'static search payload is empty');
assert.ok(
  files(chunkRoot).some(
    (path) => path.endsWith('.js') && readFileSync(path, 'utf8').includes(`${basePath}/api/search`),
  ),
  'search client is missing the deployment base path',
);
assert.match(docsHtml, /brand\/gslapper-wordmark\.png/, 'docs home is missing the full wordmark');
for (const option of ['AUR', 'Deb / RPM', 'Nix', 'Source build']) {
  assert.ok(docsHtml.includes(`data-install-option="${option}"`), `docs home is missing ${option}`);
}
for (const stage of ['install', 'wallpaper', 'multi-monitor', 'ipc', 'persist']) {
  assert.match(docsHtml, new RegExp(`data-migration-stage="${stage}"`), `docs home is missing ${stage}`);
}
for (const route of [
  '/docs/getting-started/installation/',
  '/docs/getting-started/quick-start/',
  '/docs/user-guide/command-line-options/',
  '/docs/user-guide/ipc-control/',
  '/docs/advanced/troubleshooting/',
]) {
  assert.ok(docsHtml.includes(`href="${basePath}${route}"`), `docs home is missing ${route}`);
}
assert.ok(
  rootHtml.includes(`url=${basePath}/docs/`),
  'site root does not redirect to the documentation home',
);

// Install anchors must point at real heading ids (double-hyphen collapses spaces)
assert.match(
  docsHtml,
  new RegExp(`href="${basePath}/docs/getting-started/installation/#debian--ubuntu--fedora-packages"`),
  'docs home Debian/RPM anchor is wrong (expected double-hyphen id)',
);
assert.match(
  docsHtml,
  new RegExp(`href="${basePath}/docs/getting-started/nix-installation/"`),
  'docs home Nix option should point at its own page, not an installation anchor',
);

// Callouts must render as real fumadocs Callout components, not literal [!NOTE]/[!IMPORTANT] text
assert.doesNotMatch(articleHtml, /\[!NOTE\]|\[!IMPORTANT\]|\[!WARNING\]/, 'unrendered blockquote callout in ipc-control export');
assert.match(articleHtml, /callout/, 'ipc-control is missing rendered Callout markup');
assert.doesNotMatch(installationHtml, /\[!NOTE\]|\[!IMPORTANT\]|\[!WARNING\]/, 'unrendered blockquote callout in installation export');
assert.match(installationHtml, /callout/, 'installation is missing rendered Callout markup');

// Brand images must carry intrinsic dimensions matching the PNG aspect ratio
assert.match(docsHtml, /brand\/gslapper-wordmark\.png[^>]*width="1128"/, 'wordmark img width does not match PNG (1128)');
assert.match(docsHtml, /brand\/gslapper-wordmark\.png[^>]*height="259"/, 'wordmark img height does not match PNG (259)');
assert.match(docsHtml, /brand\/gsp-mark\.png[^>]*width="551"/, 'gsp mark img width does not match PNG (551)');
assert.match(docsHtml, /brand\/gsp-mark\.png[^>]*height="259"/, 'gsp mark img height does not match PNG (259)');

// Version badge must match meson.build
assert.match(docsHtml, /v1\.5\.0/, 'version badge is stale (expected v1.5.0)');

console.log('export check passed');