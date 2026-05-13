import type { Metadata } from 'next'
import { Analytics } from '@vercel/analytics/next'
import { GeistSans } from 'geist/font/sans'
import { GeistMono } from 'geist/font/mono'
import './globals.css'

export const metadata: Metadata = {
  title: 'Wraith Engine',
  description: 'High performance streamed game engine',
  generator: 'wraithengine.com',
  icons: {
    icon: [
      {
        url: '/icon-light-50x50.png',
        media: '(prefers-color-scheme: light)',
      },
      {
        url: '/icon-dark-50x50.png',
        media: '(prefers-color-scheme: dark)',
      },
      {
        url: '/icon.svg',
        type: 'image/svg+xml',
      },
    ],
    apple: '/apple-icon.png',
  },
}

export default function RootLayout({
  children,
}: Readonly<{
  children: React.ReactNode
}>) {
  return (
    <html lang="en" className={`${GeistSans.variable} ${GeistMono.variable}`}>
      <body className={`${GeistSans.className} font-sans antialiased overflow-hidden`}>
        {children}
        {process.env.NODE_ENV === 'production' && <Analytics />}
      </body>
    </html>
  )
}
