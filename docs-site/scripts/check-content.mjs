import assert from 'node:assert/strict';
import { readFileSync, readdirSync, existsSync } from 'node:fs';
import { extname, join, dirname } from 'node:path';
import { fileURLToPath } from 'node:url';

const root = fileURLToPath(new URL('../content/docs/', import.meta.url));
const repoRoot = fileURLToPath(new URL('../../', import.meta.url));

const expectedPages = [
  'index.mdx',
  'getting-started/installation.mdx',
  'getting-started/quick-start.md',
  'getting-started/configuration.md',
  'getting-started/nix-installation.md',
  'user-guide/video-wallpapers.mdx',
  'user-guide/static-images.md',
  'user-guide/scaling-modes.md',
  'user-guide/ipc-control.mdx',
  'user-guide/command-line-options.md',
  'user-guide/persistent-wallpapers.md',
  'user-guide/systemd-service-setup.md',
  'user-guide/migration-guide.md',
  'advanced/index.md',
  'advanced/transitions.md',
  'advanced/multi-monitor.md',
  'advanced/performance.md',
  'advanced/troubleshooting.md',
  'development/building.md',
  'development/contributing.md',
  'development/api-reference.md',
  'development/architecture.md',
  'development/systemd-service.md',
];

function contentFiles(directory) {
  return readdirSync(directory, { withFileTypes: true }).flatMap((entry) => {
    const path = join(directory, entry.name);
    return entry.isDirectory() ? contentFiles(path) : [path];
  });
}

for (const page of expectedPages) {
  assert.doesNotThrow(() => readFileSync(join(root, page)), `missing documentation page: ${page}`);
}

for (const path of contentFiles(root).filter((file) => ['.md', '.mdx'].includes(extname(file)))) {
  const content = readFileSync(path, 'utf8');
  assert.doesNotMatch(content, /\]\([^)]*\.md(?:#[^)]*)?\)/, `stale .md link in ${path}`);
  assert.doesNotMatch(content, /docs-src|mkdocs/i, `stale documentation system reference in ${path}`);
  assert.doesNotMatch(content, /^!!!/m, `legacy admonition syntax in ${path}`);
  // Blockquote-callout form is also forbidden — must use <Callout>
  assert.doesNotMatch(content, /^>\s\[!/m, `unconverted blockquote callout in ${path} (use <Callout>)`);
}

// README must not reference deleted mkdocs paths
const readme = readFileSync(join(repoRoot, 'README.md'), 'utf8');
assert.doesNotMatch(readme, /docs\/assets\/images\/gslapper-logo\.png/, 'README points at deleted logo');
assert.doesNotMatch(readme, /\]\(docs\/nix-installation\.md/, 'README has stale docs/nix-installation.md link');

console.log(`content check passed (${expectedPages.length} pages)`);