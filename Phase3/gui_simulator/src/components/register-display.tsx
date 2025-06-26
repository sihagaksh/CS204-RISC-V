import { Card } from "@/components/ui/card"

interface RegisterDisplayProps {
  registers: number[]
}

export default function RegisterDisplay({ registers }: RegisterDisplayProps) {
  const registerNames = [
    "zero",
    "ra",
    "sp",
    "gp",
    "tp",
    "t0",
    "t1",
    "t2",
    "s0/fp",
    "s1",
    "a0",
    "a1",
    "a2",
    "a3",
    "a4",
    "a5",
    "a6",
    "a7",
    "s2",
    "s3",
    "s4",
    "s5",
    "s6",
    "s7",
    "s8",
    "s9",
    "s10",
    "s11",
    "t3",
    "t4",
    "t5",
    "t6",
  ]

  return (
    <div className="grid grid-cols-1 gap-2 h-[400px] overflow-auto pr-2">
      {registers.map((value, index) => (
        <Card key={index} className="p-2 flex justify-between items-center">
          <div className="flex items-center gap-2">
            <span className="text-xs font-mono italic w-8">x{index}</span> {/* Italic for register number */}
            <span className="text-xs text-muted-foreground">{registerNames[index]}</span>
          </div>
          <div className="font-mono text-xs tracking-wider"> {/* Monospace + spacing */}
            0x{value.toString(16).padStart(8, "0").toUpperCase()}
          </div>
        </Card>
      ))}
    </div>
  )
}

