"use client"

export const DEFAULT_AXIOM_SERVER_ORIGIN = "http://127.0.0.1:8080"

export function getServerOrigin() {
  const configuredOrigin = process.env.NEXT_PUBLIC_AXIOM_SERVER_ORIGIN?.trim()
  return configuredOrigin && configuredOrigin.length > 0
    ? configuredOrigin
    : DEFAULT_AXIOM_SERVER_ORIGIN
}
