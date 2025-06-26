export class RiscVAssembler {
    private programCounter = 0x00000000
    private dataAddress = 0x10000000
    private dataMode = false
    private currentPC = 0x00000000
    private labels: Map<string, number> = new Map()
    private dataLabels: Map<string, number> = new Map()
    private dataSegment: Array<[number, string]> = []
  
    private registerMap: Map<string, string> = new Map([
      ["x0", "00000"],
      ["zero", "00000"],
      ["x1", "00001"],
      ["ra", "00001"],
      ["x2", "00010"],
      ["sp", "00010"],
      ["x3", "00011"],
      ["gp", "00011"],
      ["x4", "00100"],
      ["tp", "00100"],
      ["x5", "00101"],
      ["t0", "00101"],
      ["x6", "00110"],
      ["t1", "00110"],
      ["x7", "00111"],
      ["t2", "00111"],
      ["x8", "01000"],
      ["s0", "01000"],
      ["fp", "01000"],
      ["x9", "01001"],
      ["s1", "01001"],
      ["x10", "01010"],
      ["a0", "01010"],
      ["x11", "01011"],
      ["a1", "01011"],
      ["x12", "01100"],
      ["a2", "01100"],
      ["x13", "01101"],
      ["a3", "01101"],
      ["x14", "01110"],
      ["a4", "01110"],
      ["x15", "01111"],
      ["a5", "01111"],
      ["x16", "10000"],
      ["a6", "10000"],
      ["x17", "10001"],
      ["a7", "10001"],
      ["x18", "10010"],
      ["s2", "10010"],
      ["x19", "10011"],
      ["s3", "10011"],
      ["x20", "10100"],
      ["s4", "10100"],
      ["x21", "10101"],
      ["s5", "10101"],
      ["x22", "10110"],
      ["s6", "10110"],
      ["x23", "10111"],
      ["s7", "10111"],
      ["x24", "11000"],
      ["s8", "11000"],
      ["x25", "11001"],
      ["s9", "11001"],
      ["x26", "11010"],
      ["s10", "11010"],
      ["x27", "11011"],
      ["s11", "11011"],
      ["x28", "11100"],
      ["t3", "11100"],
      ["x29", "11101"],
      ["t4", "11101"],
      ["x30", "11110"],
      ["t5", "11110"],
      ["x31", "11111"],
      ["t6", "11111"],
    ])
  
    private instructionMap: Map<string, { opcode: string; func3: string; func7: string; type: string }> = new Map([
      // R-type instructions
      ["add", { opcode: "0110011", func3: "000", func7: "0000000", type: "R" }],
      ["and", { opcode: "0110011", func3: "111", func7: "0000000", type: "R" }],
      ["or", { opcode: "0110011", func3: "110", func7: "0000000", type: "R" }],
      ["sll", { opcode: "0110011", func3: "001", func7: "0000000", type: "R" }],
      ["slt", { opcode: "0110011", func3: "010", func7: "0000000", type: "R" }],
      ["sra", { opcode: "0110011", func3: "101", func7: "0100000", type: "R" }],
      ["srl", { opcode: "0110011", func3: "101", func7: "0000000", type: "R" }],
      ["sub", { opcode: "0110011", func3: "000", func7: "0100000", type: "R" }],
      ["xor", { opcode: "0110011", func3: "100", func7: "0000000", type: "R" }],
      ["mul", { opcode: "0110011", func3: "000", func7: "0000001", type: "R" }],
      ["div", { opcode: "0110011", func3: "100", func7: "0000001", type: "R" }],
      ["rem", { opcode: "0110011", func3: "110", func7: "0000001", type: "R" }],
  
      // I-type instructions
      ["addi", { opcode: "0010011", func3: "000", func7: "", type: "I" }],
      ["andi", { opcode: "0010011", func3: "111", func7: "", type: "I" }],
      ["ori", { opcode: "0010011", func3: "110", func7: "", type: "I" }],
      ["lb", { opcode: "0000011", func3: "000", func7: "", type: "I" }],
      ["ld", { opcode: "0000011", func3: "011", func7: "", type: "I" }],
      ["lh", { opcode: "0000011", func3: "001", func7: "", type: "I" }],
      ["lw", { opcode: "0000011", func3: "010", func7: "", type: "I" }],
      ["jalr", { opcode: "1100111", func3: "000", func7: "", type: "I" }],
  
      // S-type instructions
      ["sb", { opcode: "0100011", func3: "000", func7: "", type: "S" }],
      ["sw", { opcode: "0100011", func3: "010", func7: "", type: "S" }],
      ["sd", { opcode: "0100011", func3: "011", func7: "", type: "S" }],
      ["sh", { opcode: "0100011", func3: "001", func7: "", type: "S" }],
  
      // SB-type instructions
      ["beq", { opcode: "1100011", func3: "000", func7: "", type: "SB" }],
      ["bne", { opcode: "1100011", func3: "001", func7: "", type: "SB" }],
      ["bge", { opcode: "1100011", func3: "101", func7: "", type: "SB" }],
      ["blt", { opcode: "1100011", func3: "100", func7: "", type: "SB" }],
  
      // U-type instructions
      ["auipc", { opcode: "0010111", func3: "", func7: "", type: "U" }],
      ["lui", { opcode: "0110111", func3: "", func7: "", type: "U" }],
  
      // UJ-type instructions
      ["jal", { opcode: "1101111", func3: "", func7: "", type: "UJ" }],
    ])
  
    private binaryToHex(binCode: string): string {
      let hexCode = "0x"
      for (let i = 0; i < binCode.length; i += 4) {
        const hexDigit = Number.parseInt(binCode.substr(i, 4), 2).toString(16).toUpperCase()
        hexCode += hexDigit
      }
      return hexCode
    }
  
    private generateRType(
      instr: { opcode: string; func3: string; func7: string },
      rd: string,
      rs1: string,
      rs2: string,
    ): string {
      return (
        instr.func7 +
        this.registerMap.get(rs2)! +
        this.registerMap.get(rs1)! +
        instr.func3 +
        this.registerMap.get(rd)! +
        instr.opcode
      )
    }
  
    private generateIType(instr: { opcode: string; func3: string }, rd: string, rs1: string, immediate: number): string {
      if (immediate < -2048 || immediate > 2047) {
        throw new Error("Immediate out of bound")
      }
  
      // Convert immediate to 12-bit binary with sign extension
      const immBinary = (immediate & 0xfff).toString(2).padStart(12, immediate < 0 ? "1" : "0")
  
      return immBinary + this.registerMap.get(rs1)! + instr.func3 + this.registerMap.get(rd)! + instr.opcode
    }
  
    private generateSType(instr: { opcode: string; func3: string }, rs1: string, rs2: string, offset: number): string {
      if (offset < -2048 || offset > 2047) {
        throw new Error("Offset out of bound")
      }
  
      // Convert offset to 12-bit binary with sign extension
      const offsetBinary = (offset & 0xfff).toString(2).padStart(12, offset < 0 ? "1" : "0")
  
      return (
        offsetBinary.substr(0, 7) +
        this.registerMap.get(rs2)! +
        this.registerMap.get(rs1)! +
        instr.func3 +
        offsetBinary.substr(7, 5) +
        instr.opcode
      )
    }
  
    private generateSBType(instr: { opcode: string; func3: string }, rs1: string, rs2: string, offset: number): string {
      // Convert offset to 13-bit binary with sign extension
      const immBinary = (offset & 0x1fff).toString(2).padStart(13, offset < 0 ? "1" : "0")
  
      return (
        immBinary[0] +
        immBinary.substr(2, 6) +
        this.registerMap.get(rs2)! +
        this.registerMap.get(rs1)! +
        instr.func3 +
        immBinary.substr(8, 4) +
        immBinary[1] +
        instr.opcode
      )
    }
  
    private generateUType(instr: { opcode: string }, rd: string, immediate: number): string {
      if (immediate < 0 || immediate > 1048575) {
        throw new Error("Immediate out of bound")
      }
  
      // Convert immediate to 20-bit binary
      const immBinary = immediate.toString(2).padStart(20, "0")
  
      return immBinary + this.registerMap.get(rd)! + instr.opcode
    }
  
    private generateUJType(instr: { opcode: string }, rd: string, offset: number): string {
      // Convert offset to 21-bit binary with sign extension
      const immBinary = (offset & 0x1fffff).toString(2).padStart(21, offset < 0 ? "1" : "0")
  
      return (
        immBinary[0] +
        immBinary.substr(10, 10) +
        immBinary[9] +
        immBinary.substr(1, 8) +
        this.registerMap.get(rd)! +
        immBinary.substr(11, 10) +
        immBinary[9] +
        immBinary.substr(1, 8) +
        this.registerMap.get(rd)! +
        instr.opcode
      )
    }
  
    private parseLine(line: string): string {
      const parts = line.trim().split(/\s+/)
      const instruction = parts[0]
  
      if (!this.instructionMap.has(instruction)) {
        throw new Error(`Invalid Instruction: ${instruction}`)
      }
  
      const instr = this.instructionMap.get(instruction)!
  
      if (instr.type === "R") {
        // Format: add rd, rs1, rs2
        let rd = parts[1]
        let rs1 = parts[2]
        const rs2 = parts[3]
  
        // Remove commas
        if (rd.endsWith(",")) rd = rd.slice(0, -1)
        if (rs1.endsWith(",")) rs1 = rs1.slice(0, -1)
  
        return this.generateRType(instr, rd, rs1, rs2)
      } else if (instr.type === "I") {
        if (instruction === "lw" || instruction === "lh" || instruction === "lb" || instruction === "ld") {
          // Format: lw rd, offset(rs1)
          let rd = parts[1]
          const offsetAndRs1 = parts[2]
  
          // Remove commas
          if (rd.endsWith(",")) rd = rd.slice(0, -1)
  
          let immediate: number
          let rs1: string
  
          // Parse offset(rs1) format
          const match = offsetAndRs1.match(/(-?\d+)$$([^)]+)$$/)
          if (match) {
            immediate = Number.parseInt(match[1])
            rs1 = match[2]
          } else {
            throw new Error(`Invalid format for ${instruction}: ${offsetAndRs1}`)
          }
  
          return this.generateIType(instr, rd, rs1, immediate)
        } else {
          // Format: addi rd, rs1, immediate
          let rd = parts[1]
          let rs1 = parts[2]
          const immediate = Number.parseInt(parts[3])
  
          // Remove commas
          if (rd.endsWith(",")) rd = rd.slice(0, -1)
          if (rs1.endsWith(",")) rs1 = rs1.slice(0, -1)
  
          return this.generateIType(instr, rd, rs1, immediate)
        }
      } else if (instr.type === "S") {
        // Format: sw rs2, offset(rs1)
        let rs2 = parts[1]
        const offsetAndRs1 = parts[2]
  
        // Remove commas
        if (rs2.endsWith(",")) rs2 = rs2.slice(0, -1)
  
        let offset: number
        let rs1: string
  
        // Parse offset(rs1) format
        const match = offsetAndRs1.match(/(-?\d+)$$([^)]+)$$/)
        if (match) {
          offset = Number.parseInt(match[1])
          rs1 = match[2]
        } else {
          throw new Error(`Invalid format for ${instruction}: ${offsetAndRs1}`)
        }
  
        return this.generateSType(instr, rs1, rs2, offset)
      } else if (instr.type === "SB") {
        // Format: beq rs1, rs2, label
        let rs1 = parts[1]
        let rs2 = parts[2]
        const label = parts[3]
  
        // Remove commas
        if (rs1.endsWith(",")) rs1 = rs1.slice(0, -1)
        if (rs2.endsWith(",")) rs2 = rs2.slice(0, -1)
  
        if (!this.labels.has(label)) {
          throw new Error(`Label not found: ${label}`)
        }
  
        const offset = this.labels.get(label)! - this.currentPC
        return this.generateSBType(instr, rs1, rs2, offset)
      } else if (instr.type === "U") {
        // Format: lui rd, immediate
        let rd = parts[1]
        let immediate: number
  
        // Remove commas
        if (rd.endsWith(",")) rd = rd.slice(0, -1)
  
        // Parse immediate (could be hex or decimal)
        if (parts[2].startsWith("0x")) {
          immediate = Number.parseInt(parts[2].substring(2), 16)
        } else {
          immediate = Number.parseInt(parts[2])
        }
  
        // Mask to 20 bits
        immediate &= 0xfffff
  
        return this.generateUType(instr, rd, immediate)
      } else if (instr.type === "UJ") {
        // Format: jal rd, label
        let rd = parts[1]
        const label = parts[2]
  
        // Remove commas
        if (rd.endsWith(",")) rd = rd.slice(0, -1)
  
        if (!this.labels.has(label)) {
          throw new Error(`Label not found: ${label}`)
        }
  
        const offset = this.labels.get(label)! - this.currentPC
        return this.generateUJType(instr, rd, offset)
      }
  
      throw new Error(`Unsupported instruction type: ${instr.type}`)
    }
  
    public assemble(code: string): string {
      // Reset state
      this.programCounter = 0x00000000
      this.dataAddress = 0x10000000
      this.dataMode = false
      this.currentPC = 0x00000000
      this.labels = new Map()
      this.dataLabels = new Map()
      this.dataSegment = []
  
      const lines = code.split("\n")
      const codeLines: string[] = []
  
      // First pass: collect labels and data
      for (const line of lines) {
        const trimmedLine = line.trim()
  
        // Skip empty lines and comments
        if (trimmedLine === "" || trimmedLine.startsWith("#")) {
          continue
        }
  
        // Check for section directives
        if (trimmedLine === ".data") {
          this.dataMode = true
          continue
        }
  
        if (trimmedLine === ".text") {
          this.dataMode = false
          continue
        }
  
        // Check for labels
        if (trimmedLine.includes(":")) {
          const labelParts = trimmedLine.split(":")
          const label = labelParts[0].trim()
  
          if (this.dataMode) {
            this.dataLabels.set(label, this.dataAddress)
          } else {
            this.labels.set(label, this.programCounter)
          }
  
          // If there's code after the label on the same line, process it
          const remainingCode = labelParts[1].trim()
          if (remainingCode && !this.dataMode) {
            codeLines.push(remainingCode)
            this.programCounter += 4
          } else if (remainingCode && this.dataMode) {
            // Process data directive after label
            this.processDataDirective(remainingCode)
          }
  
          continue
        }
  
        // Process data directives
        if (this.dataMode) {
          this.processDataDirective(trimmedLine)
        } else {
          // Store code lines for second pass
          codeLines.push(trimmedLine)
          this.programCounter += 4
        }
      }
  
      // Second pass: generate machine code
      let output = ""
      this.currentPC = 0x00000000
  
      for (const line of codeLines) {
        try {
          const machineCode = this.parseLine(line)
          const hexMachineCode = this.binaryToHex(machineCode)
          const hexPC = this.binaryToHex(this.currentPC.toString(2).padStart(32, "0"))
  
          output += `${hexPC} ${hexMachineCode} ${line} #${machineCode}\n`
          this.currentPC += 4
        } catch (error) {
          output += `# Error processing line "${line}": ${error}\n`
        }
      }
  
      // Add data segment
      output += "\n\n#Data Segment\n"
      for (const [address, value] of this.dataSegment) {
        const hexAddress = this.binaryToHex(address.toString(2).padStart(32, "0"))
        const hexValue = this.binaryToHex(Number.parseInt(value).toString(2).padStart(32, "0"))
        output += `${hexAddress} ${hexValue} ${value}\n`
      }
  
      return output
    }
  
    private processDataDirective(line: string): void {
      const parts = line.trim().split(/\s+/)
      const directive = parts[0]
  
      if (directive === ".word") {
        for (let i = 1; i < parts.length; i++) {
          let value: number
  
          if (parts[i].startsWith("'") && parts[i].endsWith("'") && parts[i].length === 3) {
            // Character value
            value = parts[i].charCodeAt(1)
          } else {
            value = Number.parseInt(parts[i])
          }
  
          this.dataSegment.push([this.dataAddress, value.toString()])
          this.dataAddress += 4
        }
      } else if (directive === ".half") {
        for (let i = 1; i < parts.length; i++) {
          let value: number
  
          if (parts[i].startsWith("'") && parts[i].endsWith("'") && parts[i].length === 3) {
            // Character value
            value = parts[i].charCodeAt(1)
          } else {
            value = Number.parseInt(parts[i])
          }
  
          if (value < -32768 || value > 32767) {
            throw new Error(`Value out of bound for .half: ${value}`)
          }
  
          this.dataSegment.push([this.dataAddress, value.toString()])
          this.dataAddress += 2
        }
      } else if (directive === ".byte") {
        for (let i = 1; i < parts.length; i++) {
          let value: number
  
          if (parts[i].startsWith("'") && parts[i].endsWith("'") && parts[i].length === 3) {
            // Character value
            value = parts[i].charCodeAt(1)
          } else {
            value = Number.parseInt(parts[i])
          }
  
          if (value < -128 || value > 127) {
            throw new Error(`Value out of bound for .byte: ${value}`)
          }
  
          this.dataSegment.push([this.dataAddress, value.toString()])
          this.dataAddress += 1
        }
      } else if (directive === ".double") {
        for (let i = 1; i < parts.length; i++) {
          let value: number
  
          if (parts[i].startsWith("'") && parts[i].endsWith("'") && parts[i].length === 3) {
            // Character value
            value = parts[i].charCodeAt(1)
          } else {
            value = Number.parseInt(parts[i])
          }
  
          this.dataSegment.push([this.dataAddress, value.toString()])
          this.dataAddress += 8
        }
      } else if (directive === ".asciiz") {
        // Extract the string between quotes
        const match = line.match(/"([^"]*)"/)
        if (match) {
          const str = match[1]
  
          // Store each character as a byte
          for (let i = 0; i < str.length; i++) {
            this.dataSegment.push([this.dataAddress, str.charCodeAt(i).toString()])
            this.dataAddress++
          }
  
          // Add null terminator
          this.dataSegment.push([this.dataAddress, "0"])
          this.dataAddress++
        } else {
          throw new Error("Invalid .asciiz format. Must be enclosed in double quotes.")
        }
      }
    }
  }
  
  