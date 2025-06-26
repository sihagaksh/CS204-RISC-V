import type { Metadata } from 'next'
import './globals.css'

export const metadata: Metadata = {
  title: 'Phase 1 GUI Assembler',
  description: 'Phase 1 GUI Assembler',
}

export default function RootLayout({
  children,
}: Readonly<{
  children: React.ReactNode
}>) {
  return (
    <html lang="en">
      <body>{children}</body>
    </html>
  )
}
