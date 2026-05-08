import dynamic from "next/dynamic"

const WraithEngine = dynamic(
  () => import("@/components/wraith-engine").then((m) => m.WraithEngine),
  { ssr: false }
)

export default function Home() {
  return <WraithEngine />
}
