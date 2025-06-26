import { Table, TableBody, TableCell, TableHead, TableHeader, TableRow } from "@/components/ui/table"

interface MemoryDisplayProps {
  memory: Map<number, number>
}

export default function MemoryDisplay({ memory }: MemoryDisplayProps) {
  const sortedEntries = Array.from(memory.entries()).sort((a, b) => a[0] - b[0])

  return (
    <Table>
      <TableHeader>
        <TableRow>
          <TableHead className="w-1/2">Address</TableHead>
          <TableHead className="w-1/2">Value</TableHead>
        </TableRow>
      </TableHeader>
      <TableBody>
        {sortedEntries.length > 0 ? (
          sortedEntries.map(([address, value]) => (
            <TableRow key={address}>
              <TableCell className="font-mono">0x{address.toString(16).padStart(8, "0").toUpperCase()}</TableCell>
              <TableCell className="font-mono">0x{value.toString(16).padStart(2, "0").toUpperCase()}</TableCell>
            </TableRow>
          ))
        ) : (
          <TableRow>
            <TableCell colSpan={2} className="text-center">
              No memory data available
            </TableCell>
          </TableRow>
        )}
      </TableBody>
    </Table>
  )
}

