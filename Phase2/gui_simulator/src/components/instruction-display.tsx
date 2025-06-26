import { Table, TableBody, TableCell, TableHead, TableHeader, TableRow } from "@/components/ui/table"

interface Instruction {
  address: number
  raw: number
  disassembled: string
}

interface InstructionDisplayProps {
  instructions: Instruction[]
  currentPC: number
}

export default function InstructionDisplay({ instructions, currentPC }: InstructionDisplayProps) {
  return (
    <Table>
      <TableHeader>
        <TableRow>
          <TableHead className="w-1/4">Address</TableHead>
          <TableHead className="w-1/4">Raw</TableHead>
          <TableHead className="w-1/2">Instruction</TableHead>
        </TableRow>
      </TableHeader>
      <TableBody>
        {instructions.length > 0 ? (
          instructions.map((instr) => (
            <TableRow key={instr.address} className={currentPC === instr.address ? "bg-primary/20" : ""}>
              <TableCell className="font-mono">0x{instr.address.toString(16).padStart(8, "0").toUpperCase()}</TableCell>
              <TableCell className="font-mono">0x{instr.raw.toString(16).padStart(8, "0").toUpperCase()}</TableCell>
              <TableCell className="font-mono">{instr.disassembled}</TableCell>
            </TableRow>
          ))
        ) : (
          <TableRow>
            <TableCell colSpan={3} className="text-center">
              No instructions available
            </TableCell>
          </TableRow>
        )}
      </TableBody>
    </Table>
  )
}

