interface Instruction {
    address: number
    raw: number
    disassembled: string
  }
  
  export class Simulator {
    private PC = 0
    private IR = 0
    private REG: number[] = new Array(32).fill(0)
    private clockCycles = 0
    private instrMap: Map<number, number> = new Map()
    private dataSegment: Map<number, number> = new Map()
    private instructions: Instruction[] = []
    private currentInstructionString = ""
    private initialState: {
      PC: number
      REG: number[]
      instrMap: Map<number, number>
      dataSegment: Map<number, number>
    }
  
    constructor() {
      // Initialize sp register (x2) with a stack pointer value
      this.REG[2] = 0x7fffffdc
  
      this.initialState = {
        PC: this.PC,
        REG: [...this.REG],
        instrMap: new Map(),
        dataSegment: new Map(),
      }
    }
  
    public loadMcFile(content: string): void {
      const lines = content.split("\n")
      let dataSection = false
  
      for (const line of lines) {
        if (line.trim() === "" || line.trim().startsWith("#")) {
          if (line.includes("#Data Segment")) {
            dataSection = true
          }
          continue
        }
  
        const parts = line.trim().split(/\s+/)
  
        if (!dataSection) {
          if (parts.length >= 2) {
            const pc = Number.parseInt(parts[0], 16)
            const instruction = Number.parseInt(parts[1], 16)
            this.instrMap.set(pc, instruction)
  
            this.instructions.push({
              address: pc,
              raw: instruction,
              disassembled: this.disassembleInstruction(instruction),
            })
          }
        } else {
          if (parts.length >= 2) {
            const addr = Number.parseInt(parts[0], 16)
            const value = Number.parseInt(parts[1], 16) & 0xff // Extract lower 8 bits
            this.dataSegment.set(addr, value)
          }
        }
      }
  
      // Save initial state for reset
      this.initialState = {
        PC: this.PC,
        REG: [...this.REG],
        instrMap: new Map(this.instrMap),
        dataSegment: new Map(this.dataSegment),
      }
    }
  
    public reset(): void {
      this.PC = this.initialState.PC
      this.REG = [...this.initialState.REG]
      this.instrMap = new Map(this.initialState.instrMap)
      this.dataSegment = new Map(this.initialState.dataSegment)
      this.clockCycles = 0
      this.currentInstructionString = ""
    }
  
    public step(logs: string[]): void {
      if (!this.hasNextInstruction()) {
        return
      }
  
      const initialPC = this.PC
  
      // Fetch
      this.fetch(logs)
  
      // Decode
      const { opcode, rd, rs1, rs2, funct3, funct7, imm } = this.decode(logs)
  
      // Execute
      const effectiveAddr = this.execute(opcode, rd, rs1, rs2, funct3, funct7, imm, logs)
  
      // Memory Access
      this.memoryAccess(opcode, rd, rs1, rs2, imm, effectiveAddr, funct3, logs)
  
      // Writeback
      this.writeback(opcode, rd, effectiveAddr, logs)
  
      this.clockCycles++
      logs.push(`Clock Cycle: ${this.clockCycles}`)
  
      // Update instruction string for display
      if (initialPC !== this.PC && this.instrMap.has(this.PC)) {
        const nextInstr = this.instrMap.get(this.PC) || 0
        this.currentInstructionString = this.disassembleInstruction(nextInstr)
      }
    }
  
    public hasNextInstruction(): boolean {
      return this.instrMap.has(this.PC)
    }
  
    public getRegisters(): number[] {
      return [...this.REG]
    }
  
    public getMemory(): Map<number, number> {
      return new Map(this.dataSegment)
    }
  
    public getInstructions(): Instruction[] {
      return [...this.instructions]
    }
  
    public getPC(): number {
      return this.PC
    }
  
    public getClockCycles(): number {
      return this.clockCycles
    }
  
    public getCurrentInstructionString(): string {
      return this.currentInstructionString
    }
  
    private fetch(logs: string[]): void {
      this.IR = this.instrMap.get(this.PC) || 0
      logs.push(
        `[FETCH] PC: 0x${this.PC.toString(16).padStart(8, "0")} -> Instruction: 0x${this.IR.toString(16).padStart(8, "0")}`,
      )
      this.PC += 4
    }
  
    private decode(logs: string[]): {
      opcode: number
      rd: number
      rs1: number
      rs2: number
      funct3: number
      funct7: number
      imm: number
    } {
      const opcode = this.IR & 0x7f
      const rd = (this.IR >> 7) & 0x1f
      const funct3 = (this.IR >> 12) & 0x07
      const rs1 = (this.IR >> 15) & 0x1f
      const rs2 = (this.IR >> 20) & 0x1f
      const funct7 = (this.IR >> 25) & 0x7f
  
      let imm = 0
  
      // Decode immediate based on instruction type
      switch (opcode) {
        case 0x33: // R-Type
          imm = 0
          break
  
        case 0x03: // I-Type (Load)
        case 0x13: // I-Type (ALU)
        case 0x67: // I-Type (JALR)
          imm = this.IR >> 20
          // Sign extend
          imm = imm | (imm & (1 << 11) ? 0xfffff000 : 0x00000000)
          break
  
        case 0x23: // S-Type (Store)
          imm = ((this.IR >> 25) << 5) | ((this.IR >> 7) & 0x1f)
          // Sign extend
          imm = imm | (imm & (1 << 11) ? 0xfffff000 : 0x00000000)
          break
  
        case 0x63: // SB-Type (Branch)
          imm =
            ((this.IR >> 31) << 12) | // Extract bit 31 to imm[12]
            (((this.IR >> 7) & 1) << 11) | // Extract bit 7 to imm[11]
            (((this.IR >> 25) & 0x3f) << 5) | // Extract bits 30-25 to imm[10:5]
            (((this.IR >> 8) & 0xf) << 1) // Extract bits 11-8 to imm[4:1]
  
          // Sign extend 13-bit immediate
          if (imm & (1 << 12)) {
            imm |= 0xffffe000 // Extend sign to 32 bits
          }
          imm /= 2
          break
  
        case 0x17: // U-Type (AUIPC)
        case 0x37: // U-Type (LUI)
          imm = this.IR & 0xfffff000 // 20-bit immediate
          break
  
        case 0x6f: // UJ-Type (JAL)
          imm =
            (((this.IR >> 31) & 1) << 20) | // imm[20]
            (((this.IR >> 21) & 0x3ff) << 1) | // imm[10:1]
            (((this.IR >> 20) & 1) << 11) | // imm[11]
            (((this.IR >> 12) & 0xff) << 12) // imm[19:12]
  
          // Sign extension for 21-bit immediate
          if (imm & (1 << 20)) {
            imm |= 0xffe00000 // Extend negative sign to 32-bit
          }
          break
      }
  
      logs.push(
        `[DECODE] Opcode: 0x${opcode.toString(16).padStart(2, "0")}, RD: ${rd}, RS1: ${rs1}, RS2: ${rs2}, Funct3: ${funct3}, Funct7: ${funct7}, Imm: 0x${imm.toString(16)}`,
      )
  
      return { opcode, rd, rs1, rs2, funct3, funct7, imm }
    }
  
    private execute(
      opcode: number,
      rd: number,
      rs1: number,
      rs2: number,
      funct3: number,
      funct7: number,
      imm: number,
      logs: string[],
    ): number {
      let effectiveAddr = 0
  
      switch (opcode) {
        case 0x33: // R-Type
          if (funct3 === 0x0 && funct7 === 0x00) {
            this.REG[rd] = this.REG[rs1] + this.REG[rs2] // ADD
            logs.push(
              `[EXECUTE] REG[${rd}] = REG[${rs1}] + REG[${rs2}] -> Updated REG[${rd}] = 0x${this.REG[rd].toString(16).padStart(8, "0")}`,
            )
          } else if (funct3 === 0x7 && funct7 === 0x00) {
            this.REG[rd] = this.REG[rs1] & this.REG[rs2] // AND
            logs.push(
              `[EXECUTE] REG[${rd}] = REG[${rs1}] & REG[${rs2}] -> Updated REG[${rd}] = 0x${this.REG[rd].toString(16).padStart(8, "0")}`,
            )
          } else if (funct3 === 0x6 && funct7 === 0x00) {
            this.REG[rd] = this.REG[rs1] | this.REG[rs2] // OR
            logs.push(
              `[EXECUTE] REG[${rd}] = REG[${rs1}] | REG[${rs2}] -> Updated REG[${rd}] = 0x${this.REG[rd].toString(16).padStart(8, "0")}`,
            )
          } else if (funct3 === 0x1 && funct7 === 0x00) {
            this.REG[rd] = this.REG[rs1] << this.REG[rs2] // SLL
            logs.push(
              `[EXECUTE] REG[${rd}] = REG[${rs1}] << REG[${rs2}] -> Updated REG[${rd}] = 0x${this.REG[rd].toString(16).padStart(8, "0")}`,
            )
          } else if (funct3 === 0x2 && funct7 === 0x00) {
            this.REG[rd] = this.REG[rs1] < this.REG[rs2] ? 1 : 0 // SLT
            logs.push(
              `[EXECUTE] REG[${rd}] = (REG[${rs1}] < REG[${rs2}]) ? 1 : 0 -> Updated REG[${rd}] = 0x${this.REG[rd].toString(16).padStart(8, "0")}`,
            )
          } else if (funct3 === 0x5 && funct7 === 0x20) {
            this.REG[rd] = this.REG[rs1] >> this.REG[rs2] // SRA (arithmetic shift)
            logs.push(
              `[EXECUTE] REG[${rd}] = REG[${rs1}] >> REG[${rs2}] (Arithmetic) -> Updated REG[${rd}] = 0x${this.REG[rd].toString(16).padStart(8, "0")}`,
            )
          } else if (funct3 === 0x5 && funct7 === 0x00) {
            this.REG[rd] = this.REG[rs1] >>> this.REG[rs2] // SRL (logical shift)
            logs.push(
              `[EXECUTE] REG[${rd}] = REG[${rs1}] >>> REG[${rs2}] (Logical) -> Updated REG[${rd}] = 0x${this.REG[rd].toString(16).padStart(8, "0")}`,
            )
          } else if (funct3 === 0x0 && funct7 === 0x20) {
            this.REG[rd] = this.REG[rs1] - this.REG[rs2] // SUB
            logs.push(
              `[EXECUTE] REG[${rd}] = REG[${rs1}] - REG[${rs2}] -> Updated REG[${rd}] = 0x${this.REG[rd].toString(16).padStart(8, "0")}`,
            )
          } else if (funct3 === 0x4 && funct7 === 0x00) {
            this.REG[rd] = this.REG[rs1] ^ this.REG[rs2] // XOR
            logs.push(
              `[EXECUTE] REG[${rd}] = REG[${rs1}] ^ REG[${rs2}] -> Updated REG[${rd}] = 0x${this.REG[rd].toString(16).padStart(8, "0")}`,
            )
          } else if (funct3 === 0x0 && funct7 === 0x01) {
            this.REG[rd] = this.REG[rs1] * this.REG[rs2] // MUL
            logs.push(
              `[EXECUTE] REG[${rd}] = REG[${rs1}] * REG[${rs2}] -> Updated REG[${rd}] = 0x${this.REG[rd].toString(16).padStart(8, "0")}`,
            )
          } else if (funct3 === 0x4 && funct7 === 0x01) {
            this.REG[rd] = this.REG[rs1] / this.REG[rs2] // DIV
            logs.push(
              `[EXECUTE] REG[${rd}] = REG[${rs1}] / REG[${rs2}] -> Updated REG[${rd}] = 0x${this.REG[rd].toString(16).padStart(8, "0")}`,
            )
          } else if (funct3 === 0x6 && funct7 === 0x01) {
            this.REG[rd] = this.REG[rs1] % this.REG[rs2] // REM
            logs.push(
              `[EXECUTE] REG[${rd}] = REG[${rs1}] % REG[${rs2}] -> Updated REG[${rd}] = 0x${this.REG[rd].toString(16).padStart(8, "0")}`,
            )
          }
          break
  
        case 0x13: // I-Type (ALU)
          if (funct3 === 0x0) {
            this.REG[rd] = this.REG[rs1] + imm // ADDI
            logs.push(
              `[EXECUTE] REG[${rd}] = REG[${rs1}] + 0x${imm.toString(16)} -> Updated REG[${rd}] = 0x${this.REG[rd].toString(16).padStart(8, "0")}`,
            )
          } else if (funct3 === 0x7) {
            this.REG[rd] = this.REG[rs1] & imm // ANDI
            logs.push(
              `[EXECUTE] REG[${rd}] = REG[${rs1}] & 0x${imm.toString(16)} -> Updated REG[${rd}] = 0x${this.REG[rd].toString(16).padStart(8, "0")}`,
            )
          } else if (funct3 === 0x6) {
            this.REG[rd] = this.REG[rs1] | imm // ORI
            logs.push(
              `[EXECUTE] REG[${rd}] = REG[${rs1}] | 0x${imm.toString(16)} -> Updated REG[${rd}] = 0x${this.REG[rd].toString(16).padStart(8, "0")}`,
            )
          }
          break
  
        case 0x03: // I-Type (Load)
          effectiveAddr = this.REG[rs1] + imm
          if (funct3 === 0x0) {
            logs.push(
              `[EXECUTE] LB Effective Address = 0x${effectiveAddr.toString(16).padStart(8, "0")} (rs1: 0x${this.REG[rs1].toString(16).padStart(8, "0")} + imm: 0x${imm.toString(16)})`,
            )
          } else if (funct3 === 0x3) {
            logs.push(
              `[EXECUTE] LD Effective Address = 0x${effectiveAddr.toString(16).padStart(8, "0")} (rs1: 0x${this.REG[rs1].toString(16).padStart(8, "0")} + imm: 0x${imm.toString(16)})`,
            )
          } else if (funct3 === 0x1) {
            logs.push(
              `[EXECUTE] LH Effective Address = 0x${effectiveAddr.toString(16).padStart(8, "0")} (rs1: 0x${this.REG[rs1].toString(16).padStart(8, "0")} + imm: 0x${imm.toString(16)})`,
            )
          } else if (funct3 === 0x2) {
            logs.push(
              `[EXECUTE] LW Effective Address = 0x${effectiveAddr.toString(16).padStart(8, "0")} (rs1: 0x${this.REG[rs1].toString(16).padStart(8, "0")} + imm: 0x${imm.toString(16)})`,
            )
          }
          break
  
        case 0x67: // I-Type (JALR)
          effectiveAddr = (this.REG[rs1] + imm) & ~1 // Ensure LSB is cleared
          logs.push(
            `[EXECUTE] JALR Effective Address = 0x${effectiveAddr.toString(16).padStart(8, "0")} (rs1: 0x${this.REG[rs1].toString(16).padStart(8, "0")} + imm: 0x${imm.toString(16)})`,
          )
          break
  
        case 0x23: // S-Type (Store)
          effectiveAddr = this.REG[rs1] + imm
          if (funct3 === 0x0) {
            logs.push(
              `[EXECUTE] SB Effective Address = 0x${effectiveAddr.toString(16).padStart(8, "0")} (rs1: 0x${this.REG[rs1].toString(16).padStart(8, "0")} + imm: 0x${imm.toString(16)})`,
            )
          } else if (funct3 === 0x2) {
            logs.push(
              `[EXECUTE] SW Effective Address = 0x${effectiveAddr.toString(16).padStart(8, "0")} (rs1: 0x${this.REG[rs1].toString(16).padStart(8, "0")} + imm: 0x${imm.toString(16)})`,
            )
          } else if (funct3 === 0x3) {
            logs.push(
              `[EXECUTE] SD Effective Address = 0x${effectiveAddr.toString(16).padStart(8, "0")} (rs1: 0x${this.REG[rs1].toString(16).padStart(8, "0")} + imm: 0x${imm.toString(16)})`,
            )
          } else if (funct3 === 0x1) {
            logs.push(
              `[EXECUTE] SH Effective Address = 0x${effectiveAddr.toString(16).padStart(8, "0")} (rs1: 0x${this.REG[rs1].toString(16).padStart(8, "0")} + imm: 0x${imm.toString(16)})`,
            )
          }
          break
  
        case 0x63: // SB-Type (Branch)
          {
            const offset = imm << 1
            if (funct3 === 0x0 && this.REG[rs1] === this.REG[rs2]) {
              // BEQ
              this.PC += offset - 4
              logs.push(`[EXECUTE] BEQ: PC = 0x${this.PC.toString(16).padStart(8, "0")}`)
            } else if (funct3 === 0x1 && this.REG[rs1] !== this.REG[rs2]) {
              // BNE
              this.PC += offset - 4
              logs.push(`[EXECUTE] BNE: PC = 0x${this.PC.toString(16).padStart(8, "0")}`)
            } else if (funct3 === 0x5 && this.REG[rs1] >= this.REG[rs2]) {
              // BGE
              this.PC += offset - 4
              logs.push(`[EXECUTE] BGE: PC = 0x${this.PC.toString(16).padStart(8, "0")}`)
            } else if (funct3 === 0x4 && this.REG[rs1] < this.REG[rs2]) {
              // BLT
              this.PC += offset - 4
              logs.push(`[EXECUTE] BLT: PC = 0x${this.PC.toString(16).padStart(8, "0")}`)
            }
          }
          break
  
        case 0x17: // U-Type (AUIPC)
          this.REG[rd] = this.PC + imm - 4 // PC has already been incremented in fetch
          logs.push(`[EXECUTE] AUIPC: REG[${rd}] = 0x${this.REG[rd].toString(16).padStart(8, "0")}`)
          break
  
        case 0x37: // U-Type (LUI)
          this.REG[rd] = imm
          logs.push(`[EXECUTE] LUI: REG[${rd}] = 0x${this.REG[rd].toString(16).padStart(8, "0")}`)
          break
  
        case 0x6f: // UJ-Type (JAL)
          this.REG[rd] = this.PC // PC has already been incremented in fetch
          this.PC += imm - 4
          logs.push(
            `[EXECUTE] JAL: REG[${rd}] = 0x${this.REG[rd].toString(16).padStart(8, "0")}, PC = 0x${this.PC.toString(16).padStart(8, "0")}`,
          )
          break
      }
  
      return effectiveAddr
    }
  
    private memoryAccess(
      opcode: number,
      rd: number,
      rs1: number,
      rs2: number,
      imm: number,
      effectiveAddr: number,
      funct3: number,
      logs: string[],
    ): void {
      if (opcode === 0x03) {
        // Load instructions
        switch (funct3) {
          case 0x0: // LB (Load Byte)
            this.REG[rd] = this.dataSegment.get(effectiveAddr) || 0
            // Sign extend from 8 bits to 32 bits
            if (this.REG[rd] & 0x80) {
              this.REG[rd] |= 0xffffff00
            }
            logs.push(
              `[MEMORY] Loaded REG[${rd}] = 0x${this.REG[rd].toString(16).padStart(8, "0")} (LB) from Address 0x${effectiveAddr.toString(16).padStart(8, "0")}`,
            )
            break
  
          case 0x1: // LH (Load Halfword)
            this.REG[rd] = this.dataSegment.get(effectiveAddr) || 0
            // Sign extend from 16 bits to 32 bits
            if (this.REG[rd] & 0x8000) {
              this.REG[rd] |= 0xffff0000
            }
            logs.push(
              `[MEMORY] Loaded REG[${rd}] = 0x${this.REG[rd].toString(16).padStart(8, "0")} (LH) from Address 0x${effectiveAddr.toString(16).padStart(8, "0")}`,
            )
            break
  
          case 0x2: // LW (Load Word)
            this.REG[rd] = this.dataSegment.get(effectiveAddr) || 0
            logs.push(
              `[MEMORY] Loaded REG[${rd}] = 0x${this.REG[rd].toString(16).padStart(8, "0")} (LW) from Address 0x${effectiveAddr.toString(16).padStart(8, "0")}`,
            )
            break
  
          case 0x3: // LD (Load Doubleword)
            this.REG[rd] = this.dataSegment.get(effectiveAddr) || 0
            logs.push(
              `[MEMORY] Loaded REG[${rd}] = 0x${this.REG[rd].toString(16).padStart(8, "0")} (LD) from Address 0x${effectiveAddr.toString(16).padStart(8, "0")}`,
            )
            break
        }
      } else if (opcode === 0x23) {
        // Store instructions
        switch (funct3) {
          case 0x0: // SB (Store Byte)
            this.dataSegment.set(effectiveAddr, this.REG[rs2] & 0xff)
            logs.push(
              `[MEMORY] Stored REG[${rs2}] = 0x${(this.REG[rs2] & 0xff).toString(16).padStart(2, "0")} (SB) to Address 0x${effectiveAddr.toString(16).padStart(8, "0")}`,
            )
            break
  
          case 0x1: // SH (Store Halfword)
            this.dataSegment.set(effectiveAddr, this.REG[rs2] & 0xffff)
            logs.push(
              `[MEMORY] Stored REG[${rs2}] = 0x${(this.REG[rs2] & 0xffff).toString(16).padStart(4, "0")} (SH) to Address 0x${effectiveAddr.toString(16).padStart(8, "0")}`,
            )
            break
  
          case 0x2: // SW (Store Word)
            this.dataSegment.set(effectiveAddr, this.REG[rs2])
            logs.push(
              `[MEMORY] Stored REG[${rs2}] = 0x${this.REG[rs2].toString(16).padStart(8, "0")} (SW) to Address 0x${effectiveAddr.toString(16).padStart(8, "0")}`,
            )
            break
  
          case 0x3: // SD (Store Doubleword)
            this.dataSegment.set(effectiveAddr, this.REG[rs2])
            logs.push(
              `[MEMORY] Stored REG[${rs2}] = 0x${this.REG[rs2].toString(16).padStart(8, "0")} (SD) to Address 0x${effectiveAddr.toString(16).padStart(8, "0")}`,
            )
            break
        }
      }
    }
  
    private writeback(opcode: number, rd: number, effectiveAddr: number, logs: string[]): void {
      if (opcode === 0x67) {
        // JALR
        const temp = this.REG[rd]
        this.REG[rd] = this.PC
        this.PC = effectiveAddr
        logs.push(
          `[WRITEBACK] REG[${rd}] = 0x${this.REG[rd].toString(16).padStart(8, "0")} (JALR), Updated PC = 0x${this.PC.toString(16).padStart(8, "0")}`,
        )
      } else if (opcode !== 0x23 && opcode !== 0x63) {
        // Skip writeback for store and branch instructions
        logs.push(`[WRITEBACK] REG[${rd}] = 0x${this.REG[rd].toString(16).padStart(8, "0")}`)
      }
  
      // Always ensure x0 is 0
      this.REG[0] = 0
    }
  
    private disassembleInstruction(instruction: number): string {
      const opcode = instruction & 0x7f
      const rd = (instruction >> 7) & 0x1f
      const funct3 = (instruction >> 12) & 0x07
      const rs1 = (instruction >> 15) & 0x1f
      const rs2 = (instruction >> 20) & 0x1f
      const funct7 = (instruction >> 25) & 0x7f
  
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
  
      const getReg = (index: number) => `x${index} (${registerNames[index]})`
  
      switch (opcode) {
        case 0x33: // R-Type
          if (funct3 === 0x0 && funct7 === 0x00) return `add ${getReg(rd)}, ${getReg(rs1)}, ${getReg(rs2)}`
          if (funct3 === 0x0 && funct7 === 0x20) return `sub ${getReg(rd)}, ${getReg(rs1)}, ${getReg(rs2)}`
          if (funct3 === 0x7 && funct7 === 0x00) return `and ${getReg(rd)}, ${getReg(rs1)}, ${getReg(rs2)}`
          if (funct3 === 0x6 && funct7 === 0x00) return `or ${getReg(rd)}, ${getReg(rs1)}, ${getReg(rs2)}`
          if (funct3 === 0x4 && funct7 === 0x00) return `xor ${getReg(rd)}, ${getReg(rs1)}, ${getReg(rs2)}`
          if (funct3 === 0x1 && funct7 === 0x00) return `sll ${getReg(rd)}, ${getReg(rs1)}, ${getReg(rs2)}`
          if (funct3 === 0x5 && funct7 === 0x00) return `srl ${getReg(rd)}, ${getReg(rs1)}, ${getReg(rs2)}`
          if (funct3 === 0x5 && funct7 === 0x20) return `sra ${getReg(rd)}, ${getReg(rs1)}, ${getReg(rs2)}`
          if (funct3 === 0x2 && funct7 === 0x00) return `slt ${getReg(rd)}, ${getReg(rs1)}, ${getReg(rs2)}`
          if (funct3 === 0x0 && funct7 === 0x01) return `mul ${getReg(rd)}, ${getReg(rs1)}, ${getReg(rs2)}`
          if (funct3 === 0x4 && funct7 === 0x01) return `div ${getReg(rd)}, ${getReg(rs1)}, ${getReg(rs2)}`
          if (funct3 === 0x6 && funct7 === 0x01) return `rem ${getReg(rd)}, ${getReg(rs1)}, ${getReg(rs2)}`
          break
  
        case 0x13: // I-Type (ALU)
          const imm_i = ((instruction >> 20) & 0xfff) | ((instruction >> 31) & 1 ? 0xfffff000 : 0)
          if (funct3 === 0x0) return `addi ${getReg(rd)}, ${getReg(rs1)}, ${imm_i}`
          if (funct3 === 0x7) return `andi ${getReg(rd)}, ${getReg(rs1)}, ${imm_i}`
          if (funct3 === 0x6) return `ori ${getReg(rd)}, ${getReg(rs1)}, ${imm_i}`
          break
  
        case 0x03: // I-Type (Load)
          const imm_load = ((instruction >> 20) & 0xfff) | ((instruction >> 31) & 1 ? 0xfffff000 : 0)
          if (funct3 === 0x0) return `lb ${getReg(rd)}, ${imm_load}(${getReg(rs1)})`
          if (funct3 === 0x1) return `lh ${getReg(rd)}, ${imm_load}(${getReg(rs1)})`
          if (funct3 === 0x2) return `lw ${getReg(rd)}, ${imm_load}(${getReg(rs1)})`
          if (funct3 === 0x3) return `ld ${getReg(rd)}, ${imm_load}(${getReg(rs1)})`
          break
  
        case 0x23: // S-Type (Store)
          const imm_s = (((instruction >> 25) & 0x7f) << 5) | ((instruction >> 7) & 0x1f)
          const imm_s_signed = imm_s | ((instruction >> 31) & 1 ? 0xfffff000 : 0)
          if (funct3 === 0x0) return `sb ${getReg(rs2)}, ${imm_s_signed}(${getReg(rs1)})`
          if (funct3 === 0x1) return `sh ${getReg(rs2)}, ${imm_s_signed}(${getReg(rs1)})`
          if (funct3 === 0x2) return `sw ${getReg(rs2)}, ${imm_s_signed}(${getReg(rs1)})`
          if (funct3 === 0x3) return `sd ${getReg(rs2)}, ${imm_s_signed}(${getReg(rs1)})`
          break
  
        case 0x63: // SB-Type (Branch)
          const imm_b =
            (((instruction >> 31) & 1) << 12) |
            (((instruction >> 7) & 1) << 11) |
            (((instruction >> 25) & 0x3f) << 5) |
            (((instruction >> 8) & 0xf) << 1)
          const imm_b_signed = imm_b | ((instruction >> 31) & 1 ? 0xffffe000 : 0)
          if (funct3 === 0x0) return `beq ${getReg(rs1)}, ${getReg(rs2)}, ${imm_b_signed}`
          if (funct3 === 0x1) return `bne ${getReg(rs1)}, ${getReg(rs2)}, ${imm_b_signed}`
          if (funct3 === 0x4) return `blt ${getReg(rs1)}, ${getReg(rs2)}, ${imm_b_signed}`
          if (funct3 === 0x5) return `bge ${getReg(rs1)}, ${getReg(rs2)}, ${imm_b_signed}`
          break
  
        case 0x17: // U-Type (AUIPC)
          const imm_u = instruction & 0xfffff000
          return `auipc ${getReg(rd)}, 0x${(imm_u >>> 12).toString(16)}`
  
        case 0x37: // U-Type (LUI)
          const imm_lui = instruction & 0xfffff000
          return `lui ${getReg(rd)}, 0x${(imm_lui >>> 12).toString(16)}`
  
        case 0x67: // I-Type (JALR)
          const imm_jalr = ((instruction >> 20) & 0xfff) | ((instruction >> 31) & 1 ? 0xfffff000 : 0)
          return `jalr ${getReg(rd)}, ${getReg(rs1)}, ${imm_jalr}`
  
        case 0x6f: // UJ-Type (JAL)
          const imm_j =
            (((instruction >> 31) & 1) << 20) |
            (((instruction >> 21) & 0x3ff) << 1) |
            (((instruction >> 20) & 1) << 11) |
            (((instruction >> 12) & 0xff) << 12)
          const imm_j_signed = imm_j | ((instruction >> 31) & 1 ? 0xffe00000 : 0)
          return `jal ${getReg(rd)}, ${imm_j_signed}`
      }
  
      return `Unknown instruction: 0x${instruction.toString(16)}`
    }
  }
  
  