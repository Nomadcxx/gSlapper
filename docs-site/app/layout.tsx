import { Provider } from '@/components/provider';
import type { Metadata } from 'next';
import './global.css';

const basePath = process.env.NEXT_PUBLIC_BASE_PATH ?? '';

export const metadata: Metadata = {
  metadataBase: new URL('https://nomadcxx.github.io'),
  title: {
    default: 'gSlapper documentation',
    template: '%s | gSlapper',
  },
  description: 'Documentation for gSlapper, a Wayland wallpaper utility for static and video wallpapers.',
  icons: { icon: `${basePath}/brand/gsp-mark.png` },
};

export default function Layout({ children }: LayoutProps<'/'>) {
  return (
    <html lang="en" className="dark" style={{ colorScheme: 'dark' }}>
      <body className="flex flex-col min-h-screen">
        <Provider>{children}</Provider>
      </body>
    </html>
  );
}