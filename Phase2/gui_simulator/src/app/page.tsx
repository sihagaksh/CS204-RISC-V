import RiscVSimulator from "@/components/riscv-simulator"

export default function Home() {
  return (
    <main className="min-h-screen p-4 w-screen relative">
      <h1 className="text-2xl font-bold mb-4">RISC-V Simulator</h1>
      <RiscVSimulator />
    </main>
  )
}

