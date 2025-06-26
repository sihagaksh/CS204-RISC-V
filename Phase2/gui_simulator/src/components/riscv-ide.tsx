"use client"

import { useState } from "react"
import { Tabs, TabsContent, TabsList, TabsTrigger } from "@/components/ui/tabs"
import RiscVEditor from "./riscv-editor"
import RiscVSimulator from "./riscv-simulator"
import { RiscVAssembler } from "@/lib/assembler"

export default function RiscVIDE() {
  const [editorContent, setEditorContent] = useState<string>(`# Example RISC-V program
.data
var1: .word 10, 20, 30
str1: .asciiz "Hello, World!"

.text
main:
    addi a0, zero, 5
    addi a1, zero, 10
    add a2, a0, a1
    sw a2, 0(sp)
    lw a3, 0(sp)
    beq a2, a3, end
    jal ra, end
end:
    add zero, zero, zero
`)
  const [machineCode, setMachineCode] = useState<string>("")
  const [activeTab, setActiveTab] = useState<string>("editor")

  const handleAssemble = () => {
    try {
      const assembler = new RiscVAssembler()
      const result = assembler.assemble(editorContent)
      setMachineCode(result)
      setActiveTab("simulator") // Switch to simulator tab after assembling
    } catch (error) {
      console.error("Assembly error:", error)
      setMachineCode(`# Error during assembly:\n${error}`)
    }
  }

  return (
    <div className="flex flex-col h-screen">
      <Tabs value={activeTab} onValueChange={setActiveTab} className="flex-1 flex flex-col">
        <TabsList className="mx-4 mt-4">
          <TabsTrigger value="editor">Editor</TabsTrigger>
          <TabsTrigger value="simulator">Simulator</TabsTrigger>
        </TabsList>

        <TabsContent value="editor" className="flex-1 p-4">
          <RiscVEditor value={editorContent} onChange={setEditorContent} onAssemble={handleAssemble} />
        </TabsContent>

        <TabsContent value="simulator" className="flex-1 p-4">
          <RiscVSimulator machineCode={machineCode} />
        </TabsContent>
      </Tabs>
    </div>
  )
}

