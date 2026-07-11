import type { ComponentProps } from 'react';

const basePath = process.env.NEXT_PUBLIC_BASE_PATH ?? '';

export function Brand({ className, ...props }: ComponentProps<'span'>) {
  return (
    <span className={`gsp-brand ${className ?? ''}`} {...props}>
      <img src={`${basePath}/brand/gsp-mark.png`} alt="" width="551" height="259" />
      <span>gSlapper</span>
    </span>
  );
}