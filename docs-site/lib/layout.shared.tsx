import type { BaseLayoutProps } from 'fumadocs-ui/layouts/shared';
import { gitConfig } from './shared';

export function baseOptions(): BaseLayoutProps {
  return {
    nav: {
      title: (
        <span className="sidebar-brand-mark">
          <span aria-hidden="true">////////</span>
          <span className="sr-only">gSlapper documentation home</span>
        </span>
      ),
      url: '/docs',
    },
    githubUrl: `https://github.com/${gitConfig.user}/${gitConfig.repo}`,
    themeSwitch: { enabled: false },
  };
}
