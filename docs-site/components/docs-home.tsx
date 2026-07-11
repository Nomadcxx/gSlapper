import { basePath } from '@/lib/shared';
import { ArrowRight } from 'lucide-react';
import Link from 'next/link';

const stages = [
  ['01', 'install', 'Install via AUR, package, Nix, or source build.'],
  ['02', 'wallpaper', 'Set a video or static image wallpaper on any output.'],
  ['03', 'multi-monitor', 'Drive independent wallpapers per display.'],
  ['04', 'ipc', 'Control playback at runtime via Unix socket.'],
  ['05', 'persist', 'Keep wallpapers across reboots with systemd.'],
] as const;

const links = [
  ['Installation', 'AUR, packages, Nix, and source builds', '/docs/getting-started/installation'],
  ['Quick Start', 'Set your first wallpaper in under a minute', '/docs/getting-started/quick-start'],
  ['Command Line Options', 'Complete CLI reference', '/docs/user-guide/command-line-options'],
  ['IPC Control', 'Runtime control via Unix domain socket', '/docs/user-guide/ipc-control'],
  ['Troubleshooting', 'Common issues and fixes', '/docs/advanced/troubleshooting'],
] as const;

const installOptions = [
  [
    'AUR',
    'Install the gslapper package on Arch Linux with an AUR helper.',
    '/docs/getting-started/installation#arch-linux',
  ],
  [
    'Deb / RPM',
    'Prebuilt packages for Debian, Ubuntu, and Fedora from GitHub releases.',
    '/docs/getting-started/installation#debian--ubuntu--fedora-packages',
  ],
  [
    'Nix',
    'Build from the flake and configure on NixOS.',
    '/docs/getting-started/nix-installation',
  ],
  [
    'Source build',
    'Build from source with meson and ninja when a package is unavailable.',
    '/docs/getting-started/installation#manual-build',
  ],
] as const;

export function DocsHome() {
  return (
    <div className="docs-home">
      <h1 className="sr-only">gSlapper documentation</h1>
      <section className="docs-home-intro" aria-labelledby="docs-home-purpose">
        <img
          className="docs-wordmark"
          src={`${basePath}/brand/gslapper-wordmark.png`}
          alt="gSlapper"
          width="1128"
          height="259"
        />
        <p id="docs-home-purpose">
          A wallpaper utility for Wayland that plays video and static image wallpapers with
          GStreamer, multi-monitor support, and IPC control.
        </p>
        <nav className="install-options" aria-label="Install options">
          {installOptions.map(([title, description, href]) => (
            <Link key={title} href={href} data-install-option={title}>
              <strong>{title}</strong>
              <span>{description}</span>
              <ArrowRight aria-hidden="true" />
            </Link>
          ))}
        </nav>
      </section>

      <section className="migration-flow" aria-labelledby="operator-flow-title">
        <div className="section-heading">
          <p>OPERATOR FLOW</p>
          <h2 id="operator-flow-title">From install to persistent wallpapers</h2>
        </div>
        <ol>
          {stages.map(([number, stage, description]) => (
            <li key={stage} data-migration-stage={stage}>
              <span className="stage-number">{number}</span>
              <strong>{stage}</strong>
              <span>{description}</span>
            </li>
          ))}
        </ol>
      </section>

      <nav className="docs-home-links" aria-label="Documentation sections">
        <div className="section-heading">
          <p>DOCUMENTATION</p>
          <h2>Install, configure, and operate</h2>
        </div>
        {links.map(([title, description, href]) => (
          <Link key={href} href={href}>
            <strong>{title}</strong>
            <span>{description}</span>
            <ArrowRight aria-hidden="true" />
          </Link>
        ))}
      </nav>
    </div>
  );
}