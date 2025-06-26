"use client"

import { useState, useRef } from "react"
import { Button } from "@/components/ui/button"
import { Card, CardContent } from "@/components/ui/card"
import { Download, Upload } from "lucide-react"
import dynamic from "next/dynamic"

// Dynamically import Monaco Editor to avoid SSR issues
const MonacoEditor = dynamic(() => import("@monaco-editor/react"), { ssr: false })

// RISC-V Assembler class (keeping the same implementation)
class RiscVAssembler {
  constructor() {
    this.programCounter = 0x00000000
    this.dataAddress = 0x10000000
    this.dataMode = false
    this.currentPC = 0x00000000
    this.labels = new Map()
    this.dataLabels = new Map()
    this.dataSegment = []

    this.registerMap = {
      x0: "00000",
      zero: "00000",
      x1: "00001",
      ra: "00001",
      x2: "00010",
      sp: "00010",
      x3: "00011",
      gp: "00011",
      x4: "00100",
      tp: "00100",
      x5: "00101",
      t0: "00101",
      x6: "00110",
      t1: "00110",
      x7: "00111",
      t2: "00111",
      x8: "01000",
      s0: "01000",
      fp: "01000",
      x9: "01001",
      s1: "01001",
      x10: "01010",
      a0: "01010",
      x11: "01011",
      a1: "01011",
      x12: "01100",
      a2: "01100",
      x13: "01101",
      a3: "01101",
      x14: "01110",
      a4: "01110",
      x15: "01111",
      a5: "01111",
      x16: "10000",
      a6: "10000",
      x17: "10001",
      a7: "10001",
      x18: "10010",
      s2: "10010",
      x19: "10011",
      s3: "10011",
      x20: "10100",
      s4: "10100",
      x21: "10101",
      s5: "10101",
      x22: "10110",
      s6: "10110",
      x23: "10111",
      s7: "10111",
      x24: "11000",
      s8: "11000",
      x25: "11001",
      s9: "11001",
      x26: "11010",
      s10: "11010",
      x27: "11011",
      s11: "11011",
      x28: "11100",
      t3: "11100",
      x29: "11101",
      t4: "11101",
      x30: "11110",
      t5: "11110",
      x31: "11111",
      t6: "11111",
    }

    this.instructionMap = {
      add: { opcode: "0110011", func3: "000", func7: "0000000", type: "R" },
      and: { opcode: "0110011", func3: "111", func7: "0000000", type: "R" },
      or: { opcode: "0110011", func3: "110", func7: "0000000", type: "R" },
      sll: { opcode: "0110011", func3: "001", func7: "0000000", type: "R" },
      slt: { opcode: "0110011", func3: "010", func7: "0000000", type: "R" },
      sra: { opcode: "0110011", func3: "101", func7: "0100000", type: "R" },
      srl: { opcode: "0110011", func3: "101", func7: "0000000", type: "R" },
      sub: { opcode: "0110011", func3: "000", func7: "0100000", type: "R" },
      xor: { opcode: "0110011", func3: "100", func7: "0000000", type: "R" },
      mul: { opcode: "0110011", func3: "000", func7: "0000001", type: "R" },
      div: { opcode: "0110011", func3: "100", func7: "0000001", type: "R" },
      rem: { opcode: "0110011", func3: "110", func7: "0000001", type: "R" },
      addi: { opcode: "0010011", func3: "000", func7: "", type: "I" },
      andi: { opcode: "0010011", func3: "111", func7: "", type: "I" },
      ori: { opcode: "0010011", func3: "110", func7: "", type: "I" },
      lb: { opcode: "0000011", func3: "000", func7: "", type: "I" },
      ld: { opcode: "0000011", func3: "011", func7: "", type: "I" },
      lh: { opcode: "0000011", func3: "001", func7: "", type: "I" },
      lw: { opcode: "0000011", func3: "010", func7: "", type: "I" },
      jalr: { opcode: "1100111", func3: "000", func7: "", type: "I" },
      sb: { opcode: "0100011", func3: "000", func7: "", type: "S" },
      sw: { opcode: "0100011", func3: "010", func7: "", type: "S" },
      sd: { opcode: "0100011", func3: "011", func7: "", type: "S" },
      sh: { opcode: "0100011", func3: "001", func7: "", type: "S" },
      beq: { opcode: "1100011", func3: "000", func7: "", type: "SB" },
      bne: { opcode: "1100011", func3: "001", func7: "", type: "SB" },
      bge: { opcode: "1100011", func3: "101", func7: "", type: "SB" },
      blt: { opcode: "1100011", func3: "100", func7: "", type: "SB" },
      auipc: { opcode: "0010111", func3: "", func7: "", type: "U" },
      lui: { opcode: "0110111", func3: "", func7: "", type: "U" },
      jal: { opcode: "1101111", func3: "", func7: "", type: "UJ" },
    }
  }

  binaryToHex(binCode) {
    let hexCode = "0x"
    for (let i = 0; i < binCode.length; i += 4) {
      const hexDigit = binCode.substr(i, 4)
      const hexMap = {
        "0000": "0",
        "0001": "1",
        "0010": "2",
        "0011": "3",
        "0100": "4",
        "0101": "5",
        "0110": "6",
        "0111": "7",
        "1000": "8",
        "1001": "9",
        "1010": "A",
        "1011": "B",
        "1100": "C",
        "1101": "D",
        "1110": "E",
        "1111": "F",
      }
      hexCode += hexMap[hexDigit] || "0"
    }
    return hexCode
  }

  toBinary(num, bits) {
    if (num < 0) {
      return (Math.pow(2, bits) + num).toString(2).padStart(bits, "0")
    }
    return num.toString(2).padStart(bits, "0")
  }

  generateRType(instr, rd, rs1, rs2) {
    return (
      instr.func7 + this.registerMap[rs2] + this.registerMap[rs1] + instr.func3 + this.registerMap[rd] + instr.opcode
    )
  }

  generateIType(instr, rd, rs1, immediate) {
    if (immediate < -2048 || immediate > 2047) return "Immediate out of bound"
    return this.toBinary(immediate, 12) + this.registerMap[rs1] + instr.func3 + this.registerMap[rd] + instr.opcode
  }

  generateSType(instr, rs1, rs2, offset) {
    if (offset < -2048 || offset > 2047) return "Offset out of bound"
    const offsetStr = this.toBinary(offset, 12)
    return (
      offsetStr.substr(0, 7) +
      this.registerMap[rs2] +
      this.registerMap[rs1] +
      instr.func3 +
      offsetStr.substr(7, 5) +
      instr.opcode
    )
  }

  generateSBType(instr, rs1, rs2, offset) {
    const imm = this.toBinary(offset, 13)
    return (
      imm[0] +
      imm.substr(2, 6) +
      this.registerMap[rs2] +
      this.registerMap[rs1] +
      instr.func3 +
      imm.substr(8, 4) +
      imm[1] +
      instr.opcode
    )
  }

  generateUType(instr, rd, immediate) {
    if (immediate < 0 || immediate > 1048575) return "Immediate out of bound"
    return this.toBinary(immediate, 20) + this.registerMap[rd] + instr.opcode
  }

  generateUJType(instr, rd, offset) {
    const imm = this.toBinary(offset, 21)
    return imm[0] + imm.substr(10, 10) + imm[9] + imm.substr(1, 8) + this.registerMap[rd] + instr.opcode
  }

  parseLine(line) {
    const parts = line.trim().split(/\s+/)
    const instruction = parts[0]

    if (!this.instructionMap[instruction]) return "Invalid Instruction"

    const instr = this.instructionMap[instruction]
    let rd,
      rs1,
      rs2,
      immediate = 0

    if (instr.type === "R") {
      rd = parts[1].replace(",", "")
      rs1 = parts[2].replace(",", "")
      rs2 = parts[3]
      return this.generateRType(instr, rd, rs1, rs2)
    } else if (instr.type === "I") {
      if (["lw", "lh", "lb", "ld"].includes(instruction)) {
        rd = parts[1].replace(",", "")
        const offsetPart = parts[2]
        if (offsetPart.includes("(")) {
          immediate = Number.parseInt(offsetPart.substr(0, offsetPart.indexOf("(")))
          rs1 = offsetPart.substr(offsetPart.indexOf("(") + 1).replace(")", "")
        } else {
          immediate = Number.parseInt(offsetPart.replace(",", ""))
          rs1 = parts[3]
        }
        return this.generateIType(instr, rd, rs1, immediate)
      } else {
        rd = parts[1].replace(",", "")
        rs1 = parts[2].replace(",", "")
        immediate = Number.parseInt(parts[3])
        return this.generateIType(instr, rd, rs1, immediate)
      }
    } else if (instr.type === "S") {
      rs2 = parts[1].replace(",", "")
      const offsetPart = parts[2]
      if (offsetPart.includes("(")) {
        immediate = Number.parseInt(offsetPart.substr(0, offsetPart.indexOf("(")))
        rs1 = offsetPart.substr(offsetPart.indexOf("(") + 1).replace(")", "")
      } else {
        immediate = Number.parseInt(offsetPart.replace(",", ""))
        rs1 = parts[3]
      }
      return this.generateSType(instr, rs1, rs2, immediate)
    } else if (instr.type === "SB") {
      rs1 = parts[1].replace(",", "")
      rs2 = parts[2].replace(",", "")
      const label = parts[3]
      if (!this.labels.has(label)) return "Label not found"
      const offset = this.labels.get(label) - this.currentPC
      return this.generateSBType(instr, rs1, rs2, offset)
    } else if (instr.type === "U") {
      rd = parts[1].replace(",", "")
      const offset = parts[2]
      if (offset.startsWith("0x")) {
        immediate = Number.parseInt(offset, 16)
      } else {
        immediate = Number.parseInt(offset)
      }
      immediate &= 0xfffff
      return this.generateUType(instr, rd, immediate)
    } else if (instr.type === "UJ") {
      rd = parts[1].replace(",", "")
      const label = parts[2]
      if (!this.labels.has(label)) return "Label not found"
      const offset = this.labels.get(label) - this.currentPC
      return this.generateUJType(instr, rd, offset)
    }

    return "Something went wrong"
  }

  assemble(input) {
    this.programCounter = 0x00000000
    this.dataAddress = 0x10000000
    this.dataMode = false
    this.currentPC = 0x00000000
    this.labels.clear()
    this.dataLabels.clear()
    this.dataSegment = []

    const lines = input.split("\n")
    const codeLines = []
    let output = ""

    // First pass: collect labels and process data section
    for (let line of lines) {
      line = line.trim()
      if (line === ".data") {
        this.dataMode = true
        continue
      }
      if (line === ".text") {
        this.dataMode = false
        continue
      }
      if (line.endsWith(":")) {
        const label = line.slice(0, -1)
        if (this.dataMode) {
          this.dataLabels.set(label, this.dataAddress)
        } else {
          this.labels.set(label, this.programCounter)
        }
        continue
      }
      if (this.dataMode) {
        if (line.startsWith(".word")) {
          const values = line.split(/\s+/).slice(1)
          for (const val of values) {
            this.dataSegment.push([this.dataAddress, val])
            this.dataAddress += 4
          }
        }
        continue
      }
      if (line && !line.startsWith("#")) {
        codeLines.push(line)
        this.programCounter += 4
      }
    }

    // Second pass: generate machine code
    this.currentPC = 0x00000000
    for (const line of codeLines) {
      const machineCode = this.parseLine(line)
      if (
        machineCode.includes("out of bound") ||
        machineCode.includes("Invalid") ||
        machineCode.includes("not found")
      ) {
        output += `Error: ${machineCode} in line: ${line}\n`
      } else {
        const pcHex = this.binaryToHex(this.toBinary(this.currentPC, 32))
        const codeHex = this.binaryToHex(machineCode)
        output += `${pcHex} ${codeHex} ${line} #${machineCode}\n`
      }
      this.currentPC += 4
    }

    if (this.dataSegment.length > 0) {
      output += "\n\n#Data Segment\n"
      for (const [addr, val] of this.dataSegment) {
        const addrHex = this.binaryToHex(this.toBinary(addr, 32))
        const valHex = this.binaryToHex(this.toBinary(Number.parseInt(val), 32))
        output += `${addrHex} ${valHex} ${val}\n`
      }
    }

    return output
  }
}

export default function RiscVAssemblerPage() {
  const [assemblyCode, setAssemblyCode] = useState(`# Sample RISC-V Assembly Code
.text
main:
    addi x1, x0, 10
    addi x2, x0, 20
    add x3, x1, x2
    sw x3, 0(x0)
    beq x1, x2, end
    addi x4, x0, 1
end:
    addi x5, x0, 0`)

  const [isConverting, setIsConverting] = useState(false)
  const fileInputRef = useRef(null)

  const handleEditorDidMount = (editor, monaco) => {
    // Define RISC-V Assembly language
    monaco.languages.register({ id: "riscv-assembly" })

    // Define syntax highlighting rules
    monaco.languages.setMonarchTokensProvider("riscv-assembly", {
      tokenizer: {
        root: [
          // Comments
          [/#.*$/, "comment"],

          // Labels
          [/^\s*[a-zA-Z_][a-zA-Z0-9_]*:/, "type"],

          // Directives
          [/\.(text|data|word|byte|half|double|asciiz)/, "keyword"],

          // Instructions
          [
            /\b(add|addi|and|andi|or|ori|xor|sub|sll|srl|sra|slt|mul|div|rem|lb|lh|lw|ld|sb|sh|sw|sd|beq|bne|bge|blt|jal|jalr|lui|auipc)\b/,
            "keyword",
          ],

          // Registers
          [/\b(x[0-9]|x[12][0-9]|x3[01]|zero|ra|sp|gp|tp|t[0-6]|s[0-9]|s1[01]|a[0-7]|fp)\b/, "variable"],

          // Numbers
          [/\b0x[0-9a-fA-F]+\b/, "number"],
          [/\b\d+\b/, "number"],

          // Strings
          [/".*?"/, "string"],

          // Punctuation
          [/[,()]/, "delimiter"],
        ],
      },
    })

    // Define theme colors
    monaco.editor.defineTheme("riscv-theme", {
      base: "vs",
      inherit: true,
      rules: [
        { token: "comment", foreground: "6a737d", fontStyle: "italic" },
        { token: "keyword", foreground: "d73a49", fontStyle: "bold" },
        { token: "variable", foreground: "005cc5" },
        { token: "number", foreground: "032f62" },
        { token: "string", foreground: "032f62" },
        { token: "type", foreground: "6f42c1", fontStyle: "bold" },
        { token: "delimiter", foreground: "24292e" },
      ],
      colors: {
        "editor.background": "#ffffff",
        "editor.foreground": "#24292e",
      },
    })

    monaco.editor.setTheme("riscv-theme")
  }

  const handleDownload = async () => {
    setIsConverting(true)
    try {
      const assembler = new RiscVAssembler()
      const result = assembler.assemble(assemblyCode)

      const blob = new Blob([result], { type: "text/plain" })
      const url = URL.createObjectURL(blob)
      const a = document.createElement("a")
      a.href = url
      a.download = "output.mc"
      document.body.appendChild(a)
      a.click()
      document.body.removeChild(a)
      URL.revokeObjectURL(url)
    } catch (error) {
      console.error("Error converting assembly:", error)
    }
    setIsConverting(false)
  }

  const handleFileUpload = (event) => {
    const file = event.target.files[0]
    if (file) {
      const reader = new FileReader()
      reader.onload = (e) => {
        setAssemblyCode(e.target.result)
      }
      reader.readAsText(file)
    }
  }

  return (
    <div className="min-h-screen bg-gray-50 p-6">
      <div className="max-w-7xl mx-auto">
        {/* Header */}
        <div className="mb-6">
          <h1 className="text-2xl font-bold text-gray-900 mb-4">RISC-V Assembler</h1>

          {/* Control Panel */}
          <div className="flex items-center gap-4 mb-4">
            <Button
              variant="outline"
              onClick={() => fileInputRef.current?.click()}
              className="bg-gray-800 text-white hover:bg-gray-700"
            >
              <Upload className="w-4 h-4 mr-2" />
              Upload .asm File
            </Button>
            <input
              ref={fileInputRef}
              type="file"
              accept=".asm,.s,.txt"
              onChange={handleFileUpload}
              className="hidden"
            />
            <span className="text-sm text-gray-600">No file loaded</span>
          </div>

          <div className="flex items-center gap-4">
            <Button
              onClick={handleDownload}
              disabled={isConverting || !assemblyCode.trim()}
              className="bg-blue-600 hover:bg-blue-700 text-white"
            >
              <Download className="w-4 h-4 mr-2" />
              {isConverting ? "Converting..." : "Download .mc File"}
            </Button>
          </div>
        </div>

        {/* Main Content */}
        <div className="grid grid-cols-1 lg:grid-cols-3 gap-6">
          {/* Editor Section */}
          <div className="lg:col-span-2">
            <Card>
              <CardContent className="p-0">
                <div className="h-[600px] border rounded-lg overflow-hidden">
                  <MonacoEditor
                    height="600px"
                    language="riscv-assembly"
                    value={assemblyCode}
                    onChange={(value) => setAssemblyCode(value || "")}
                    onMount={handleEditorDidMount}
                    options={{
                      minimap: { enabled: false },
                      fontSize: 14,
                      lineNumbers: "on",
                      roundedSelection: false,
                      scrollBeyondLastLine: false,
                      automaticLayout: true,
                      tabSize: 4,
                      insertSpaces: true,
                      wordWrap: "on",
                    }}
                  />
                </div>
              </CardContent>
            </Card>
          </div>

          {/* Info Panel */}
          <div className="lg:col-span-1">
            <Card>
              <CardContent className="p-6">
                <h3 className="text-lg font-semibold mb-4">Instructions</h3>
                <div className="space-y-4 text-sm">
                  <div>
                    <h4 className="font-medium mb-2">Supported Instructions:</h4>
                    <ul className="space-y-1 text-gray-600">
                      <li>
                        • <span className="font-mono">add, sub, and, or, xor</span>
                      </li>
                      <li>
                        • <span className="font-mono">addi, andi, ori</span>
                      </li>
                      <li>
                        • <span className="font-mono">lw, sw, lb, sb</span>
                      </li>
                      <li>
                        • <span className="font-mono">beq, bne, bge, blt</span>
                      </li>
                      <li>
                        • <span className="font-mono">jal, jalr</span>
                      </li>
                      <li>
                        • <span className="font-mono">lui, auipc</span>
                      </li>
                    </ul>
                  </div>

                  <div>
                    <h4 className="font-medium mb-2">Format:</h4>
                    <ul className="space-y-1 text-gray-600">
                      <li>
                        • Use <span className="font-mono">.text</span> for code section
                      </li>
                      <li>
                        • Use <span className="font-mono">.data</span> for data section
                      </li>
                      <li>
                        • Labels end with colon <span className="font-mono">:</span>
                      </li>
                      <li>
                        • Comments start with <span className="font-mono">#</span>
                      </li>
                    </ul>
                  </div>

                  <div>
                    <h4 className="font-medium mb-2">Registers:</h4>
                    <ul className="space-y-1 text-gray-600">
                      <li>
                        • <span className="font-mono">x0-x31</span> (numeric)
                      </li>
                      <li>
                        • <span className="font-mono">zero, ra, sp, gp, tp</span>
                      </li>
                      <li>
                        • <span className="font-mono">t0-t6, s0-s11</span>
                      </li>
                      <li>
                        • <span className="font-mono">a0-a7, fp</span>
                      </li>
                    </ul>
                  </div>
                </div>
              </CardContent>
            </Card>
          </div>
        </div>
      </div>
    </div>
  )
}
