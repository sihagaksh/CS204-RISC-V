#include<bits/stdc++.h>
using namespace std;

// Defining the structure of an instruction
struct Instruction{
    uint32_t opcode;
    uint32_t rd;
    uint32_t rs1;
    uint32_t rs2;
    uint32_t func3;
    uint32_t func7;
    int imm;
    uint32_t effective_addr;
};

//Global variables
uint32_t PC = 0;
uint32_t IR = 0;
uint32_t REG[32]={0};
uint32_t clock_cycles = 0;

//Maps to store the instructions and data
map<uint32_t, uint32_t> instr_map;
map<uint32_t, uint8_t> data_segment;


class RiscVsimulator{
private:
    // fetching the instruction
    void fetch(Instruction &instr) {
        IR = instr_map[PC];
        printf("[FETCH] PC: 0x%08X -> Instruction: 0x%08X\n", PC, IR);
        PC += 4;
    }

    void decode(Instruction &instr) {
        instr.opcode = IR & 0x7F; // Extract instr.opcode (7 bits)
        instr.rd = (IR >> 7) & 0x1F; // Extract instr.rd (5 bits)
        instr.func3 = (IR >> 12) & 0x07; // Extract instr.func3 (3 bits)
        instr.rs1 = (IR >> 15) & 0x1F; // Extract instr.rs1 (5 bits)
        instr.rs2 = (IR >> 20) & 0x1F; // Extract instr.rs2 (5 bits)
        instr.func7 = (IR >> 25) & 0x7F; // Extract instr.func7 (7 bits)
    
        // Decode instr.immediate based on instruction type
        switch (instr.opcode) {
            case 0x33: // R-Type (e.g., ADD, SUB, AND, OR, SLL, SLT, SRA, SRL, XOR, MUL, DIV, REM)
                instr.imm = 0; // No instr.immediate field in R-Type instructions
                // All fields (instr.rd, instr.rs1, instr.rs2) are used in R-Type instructions
                break;
    
            case 0x03: // I-Type (Load: LB, LH, LW, LD)
                instr.imm = (int32_t)(IR >> 20); // Extract bits 31:20
                instr.imm = instr.imm | ((instr.imm & (1 << 11)) ? 0xFFFFF000 : 0x00000000); // Sign extend
                // Shift left, then right to sign extend
                instr.rs2 = 0; // instr.rs2 is not used in I-Type instructions
                break;
            case 0x13: // I-Type (ALU: ADDI, ANDI, ORI)
                instr.imm = (int32_t)(IR >> 20); // Extract bits 31:20
                instr.imm = instr.imm | ((instr.imm & (1 << 11)) ? 0xFFFFF000 : 0x00000000); // Sign extend
                // Shift left, then right to sign extend
                instr.rs2 = 0; // instr.rs2 is not used in I-Type instructions
                break;
            case 0x67: // I-Type (JALR)
                instr.imm = (int32_t)(IR >> 20); // Sign-extended 12-bit instr.immediate
                instr.rs2 = 0; // instr.rs2 is not used in I-Type instructions
                break;
    
            case 0x23: // S-Type (Store: SB, SH, SW, SD)
                instr.imm = ((IR >> 25) << 5) | ((IR >> 7) & 0x1F); // Sign-extended 12-bit instr.immediate
                instr.rd = 0; // instr.rd is not used in S-Type instructions
                break;
    
            case 0x63: // SB-Type (Branch: BEQ, BNE, BGE, BLT)
                instr.imm = ((IR >> 31) << 12) | // Extract bit 31 to instr.imm[12]
                      (((IR >> 7) & 1) << 11) | // Extract bit 7 to instr.imm[11]
                      (((IR >> 25) & 0x3F) << 5) | // Extract bits 30-25 to instr.imm[10:5]
                      ((IR >> 8) & 0xF) << 1;  // Extract bits 11-8 to instr.imm[4:1]
            
                // Sign extend 13-bit instr.immediate
                if (instr.imm & (1 << 12)) {
                    instr.imm |= 0xFFFFE000; // Extend sign to 32 bits
                }
                instr.imm/=2;
                // cout << "instr.immediate: " << instr.imm << endl;
                instr.rd = 0; // instr.rd is not used in SB-Type instructions
                break;
            
            
            case 0x17: // U-Type (AUIPC)
            case 0x37: // U-Type (LUI)
            instr.imm = (IR & 0xFFFFF000);  // Extract bits 31:12
            // Sign-extend the 20-bit immediate to 32 bits
            if (IR & 0x80000000) {  // Check if sign bit is set
                instr.imm |= 0xFFF00000;
            }
            instr.rs1 = 0;
            instr.rs2 = 0;
            break;

    
            case 0x6F: // UJ-Type (JAL)
            instr.imm = ((IR >> 31) & 1) << 20 | // instr.imm[20]
                  ((IR >> 21) & 0x3FF) << 1 | // instr.imm[10:1]
                  ((IR >> 20) & 1) << 11 | // instr.imm[11]
                  ((IR >> 12) & 0xFF) << 12; // instr.imm[19:12]
        
            // Sign extension for 21-bit instr.immediate
            if (instr.imm & (1 << 20)) {
                instr.imm |= 0xFFE00000; // Extend negative sign to 32-bit
            }
        
            // instr.imm <<= 1; // JAL instr.immediate is always even, so shift left by 1
        
            printf("[EXECUTE] JAL with instr.imm: 0x%08X\n", instr.imm);
            break;        
    
            default:
                instr.imm = 0; // Unknown instr.opcode
                instr.rd = 0; // Default to 0 if unknown
                instr.rs1 = 0; // Default to 0 if unknown
                instr.rs2 = 0; // Default to 0 if unknown
                break;
        }
    
        printf("[DECODE] Opcode: 0x%02X, RD: %d, instr.rs1: %d, instr.rs2: %d, instr.func3: %d, instr.func7: %d, instr.imm: 0x%X\n", instr.opcode, instr.rd, instr.rs1, instr.rs2, instr.func3, instr.func7, instr.imm);
    }

    void execute(Instruction &instr) {
        switch (instr.opcode) {
            case 0x33: // R-Type
                if (instr.func3 == 0x0 && instr.func7 == 0x00) {
                    REG[instr.rd] = REG[instr.rs1] + REG[instr.rs2]; // ADD
                    printf("[EXECUTE] REG[%d] = REG[%d] + REG[%d] -> Updated REG[%d] = 0x%08X\n", instr.rd, instr.rs1, instr.rs2, instr.rd, REG[instr.rd]);
                } else if (instr.func3 == 0x7 && instr.func7 == 0x00) {
                    REG[instr.rd] = REG[instr.rs1] & REG[instr.rs2]; // AND
                    printf("[EXECUTE] REG[%d] = REG[%d] & REG[%d] -> Updated REG[%d] = 0x%08X\n", instr.rd, instr.rs1, instr.rs2, instr.rd, REG[instr.rd]);
                } else if (instr.func3 == 0x6 && instr.func7 == 0x00) {
                    REG[instr.rd] = REG[instr.rs1] | REG[instr.rs2]; // OR
                    printf("[EXECUTE] REG[%d] = REG[%d] | REG[%d] -> Updated REG[%d] = 0x%08X\n", instr.rd, instr.rs1, instr.rs2, instr.rd, REG[instr.rd]);
                } else if (instr.func3 == 0x1 && instr.func7 == 0x00) {
                    REG[instr.rd] = REG[instr.rs1] << REG[instr.rs2]; // SLL
                    printf("[EXECUTE] REG[%d] = REG[%d] << REG[%d] -> Updated REG[%d] = 0x%08X\n", instr.rd, instr.rs1, instr.rs2, instr.rd, REG[instr.rd]);
                } else if (instr.func3 == 0x2 && instr.func7 == 0x00) {
                    REG[instr.rd] = (int32_t)REG[instr.rs1] < (int32_t)REG[instr.rs2] ? 1 : 0; // SLT
                    printf("[EXECUTE] REG[%d] = (REG[%d] < REG[%d]) ? 1 : 0 -> Updated REG[%d] = 0x%08X\n", instr.rd, instr.rs1, instr.rs2, instr.rd, REG[instr.rd]);
                } else if (instr.func3 == 0x5 && instr.func7 == 0x20) {
                    REG[instr.rd] = (int32_t)REG[instr.rs1] >> REG[instr.rs2]; // SRA
                    printf("[EXECUTE] REG[%d] = REG[%d] >> REG[%d] (Arithmetic) -> Updated REG[%d] = 0x%08X\n", instr.rd, instr.rs1, instr.rs2, instr.rd, REG[instr.rd]);
                } else if (instr.func3 == 0x5 && instr.func7 == 0x00) {
                    REG[instr.rd] = REG[instr.rs1] >> REG[instr.rs2]; // SRL
                    printf("[EXECUTE] REG[%d] = REG[%d] >> REG[%d] (Logical) -> Updated REG[%d] = 0x%08X\n", instr.rd, instr.rs1, instr.rs2, instr.rd, REG[instr.rd]);
                } else if (instr.func3 == 0x0 && instr.func7 == 0x20) {
                    REG[instr.rd] = REG[instr.rs1] - REG[instr.rs2]; // SUB
                    printf("[EXECUTE] REG[%d] = REG[%d] - REG[%d] -> Updated REG[%d] = 0x%08X\n", instr.rd, instr.rs1, instr.rs2, instr.rd, REG[instr.rd]);
                } else if (instr.func3 == 0x4 && instr.func7 == 0x00) {
                    REG[instr.rd] = REG[instr.rs1] ^ REG[instr.rs2]; // XOR
                    printf("[EXECUTE] REG[%d] = REG[%d] ^ REG[%d] -> Updated REG[%d] = 0x%08X\n", instr.rd, instr.rs1, instr.rs2, instr.rd, REG[instr.rd]);
                } else if (instr.func3 == 0x0 && instr.func7 == 0x01) {
                    REG[instr.rd] = REG[instr.rs1] * REG[instr.rs2]; // MUL
                    printf("[EXECUTE] REG[%d] = REG[%d] * REG[%d] -> Updated REG[%d] = 0x%08X\n", instr.rd, instr.rs1, instr.rs2, instr.rd, REG[instr.rd]);
                } else if (instr.func3 == 0x4 && instr.func7 == 0x01) {
                    REG[instr.rd] = REG[instr.rs1] / REG[instr.rs2]; // DIV
                    printf("[EXECUTE] REG[%d] = REG[%d] / REG[%d] -> Updated REG[%d] = 0x%08X\n", instr.rd, instr.rs1, instr.rs2, instr.rd, REG[instr.rd]);
                } else if (instr.func3 == 0x6 && instr.func7 == 0x01) {
                    REG[instr.rd] = REG[instr.rs1] % REG[instr.rs2]; // REM
                    printf("[EXECUTE] REG[%d] = REG[%d] %% REG[%d] -> Updated REG[%d] = 0x%08X\n", instr.rd, instr.rs1, instr.rs2, instr.rd, REG[instr.rd]);
                }
                break;
                case 0x13: // I-Type: ADDI, ANDI, ORI (instr.opcode: 0010011)
                if (instr.func3 == 0x0) {
                    REG[instr.rd] = REG[instr.rs1] + instr.imm; // ADDI
                    printf("[EXECUTE] REG[%d] = REG[%d] + 0x%X -> Updated REG[%d] = 0x%08X\n", instr.rd, instr.rs1, instr.imm, instr.rd, REG[instr.rd]);
                } else if (instr.func3 == 0x7) {
                    REG[instr.rd] = REG[instr.rs1] & instr.imm; // ANDI
                    printf("[EXECUTE] REG[%d] = REG[%d] & 0x%X -> Updated REG[%d] = 0x%08X\n", instr.rd, instr.rs1, instr.imm, instr.rd, REG[instr.rd]);
                } else if (instr.func3 == 0x6) {
                    REG[instr.rd] = REG[instr.rs1] | instr.imm; // ORI
                    printf("[EXECUTE] REG[%d] = REG[%d] | 0x%X -> Updated REG[%d] = 0x%08X\n", instr.rd, instr.rs1, instr.imm, instr.rd, REG[instr.rd]);
                }
                break;
            
            case 0x03: // I-Type: LB, LD, LH, LW (instr.opcode: 0000011)
                instr.effective_addr = REG[instr.rs1] + instr.imm;
                if (instr.func3 == 0x0) {
                    printf("[EXECUTE] LB Effective Address = 0x%08X (instr.rs1: 0x%08X + instr.imm: 0x%08X)\n", instr.effective_addr, REG[instr.rs1], instr.imm);
                } else if (instr.func3 == 0x3) {
                    printf("[EXECUTE] LD Effective Address = 0x%08X (instr.rs1: 0x%08X + instr.imm: 0x%08X)\n", instr.effective_addr, REG[instr.rs1], instr.imm);
                } else if (instr.func3 == 0x1) {
                    printf("[EXECUTE] LH Effective Address = 0x%08X (instr.rs1: 0x%08X + instr.imm: 0x%08X)\n", instr.effective_addr, REG[instr.rs1], instr.imm);
                } else if (instr.func3 == 0x2) {
                    printf("[EXECUTE] LW Effective Address = 0x%08X (instr.rs1: 0x%08X + instr.imm: 0x%08X)\n", instr.effective_addr, REG[instr.rs1], instr.imm);
                }
                break;
            
            case 0x67: // I-Type: JALR (instr.opcode: 1100111)
                instr.effective_addr = (REG[instr.rs1] + instr.imm) & ~1; // Ensure LSB is cleared
                printf("[EXECUTE] JALR Effective Address = 0x%08X (instr.rs1: 0x%08X + instr.imm: 0x%08X)\n", instr.effective_addr, REG[instr.rs1], instr.imm);
                break;
            
            case 0x23: // S-Type
                if (instr.func3 == 0x0) {
                    instr.effective_addr = REG[instr.rs1] + instr.imm; // SB
                    printf("[EXECUTE] Effective Address = 0x%08X (instr.rs1: 0x%08X + instr.imm: 0x%08X)\n", instr.effective_addr, REG[instr.rs1], instr.imm);
                } else if (instr.func3 == 0x2) {
                    instr.effective_addr = REG[instr.rs1] + instr.imm; // SW
                    printf("[EXECUTE] Effective Address = 0x%08X (instr.rs1: 0x%08X + instr.imm: 0x%08X)\n", instr.effective_addr, REG[instr.rs1], instr.imm);
                } else if (instr.func3 == 0x3) {
                    instr.effective_addr = REG[instr.rs1] + instr.imm; // SD
                    printf("[EXECUTE] Effective Address = 0x%08X (instr.rs1: 0x%08X + instr.imm: 0x%08X)\n", instr.effective_addr, REG[instr.rs1], instr.imm);
                } else if (instr.func3 == 0x1) {
                    instr.effective_addr = REG[instr.rs1] + instr.imm; // SH
                    printf("[EXECUTE] Effective Address = 0x%08X (instr.rs1: 0x%08X + instr.imm: 0x%08X)\n", instr.effective_addr, REG[instr.rs1], instr.imm);
                }
                break;
            case 0x63: // SB-Type
                {
                    int32_t offset = instr.imm << 1;
                    if (instr.func3 == 0x0 && REG[instr.rs1] == REG[instr.rs2]) { // BEQ
                        PC += offset - 4;
                        printf("[EXECUTE] BEQ: PC = 0x%08X\n", PC);
                    } else if (instr.func3 == 0x1 && REG[instr.rs1] != REG[instr.rs2]) { // BNE
                        cout<<"i am here"<<endl;
                        cout<<REG[instr.rs1]<<" "<<REG[instr.rs2]<<endl;
                        PC += offset - 4;
                        printf("[EXECUTE] BNE: PC = 0x%08X\n", PC);
                    } else if (instr.func3 == 0x5 && (int32_t)REG[instr.rs1] >= (int32_t)REG[instr.rs2]) { // BGE
                        PC += offset - 4;
                        printf("[EXECUTE] BGE: PC = 0x%08X\n", PC);
                    } else if (instr.func3 == 0x4 && (int32_t)REG[instr.rs1] < (int32_t)REG[instr.rs2]) { // BLT
                        PC += offset - 4;
                        printf("[EXECUTE] BLT: PC = 0x%08X\n", PC);
                    }
                }
                break;
            case 0x17: // U-Type (AUIPC)
                REG[instr.rd] = PC + instr.imm;
                printf("[EXECUTE] AUIPC: REG[%d] = 0x%08X\n", instr.rd, REG[instr.rd]);
                break;
            case 0x37: // U-Type (LUI)
                REG[instr.rd] = instr.imm;  // Immediate is already properly shifted
                printf("[EXECUTE] LUI: REG[%d] = 0x%08X\n", instr.rd, REG[instr.rd]);
                break;
            case 0x6F: // UJ-Type (JAL)
                REG[instr.rd] = PC;
                PC += instr.imm - 4;
                printf("[EXECUTE] JAL: REG[%d] = 0x%08X, PC = 0x%08X\n", instr.rd, REG[instr.rd], PC);
                break;
        }
    }

    void memory_access(Instruction &instr) {
        if (instr.opcode == 0x03) { // Load instructions (I-Type: LB, LH, LW, LD)
            switch (instr.func3) {
                case 0x0: // LB (Load Byte)
                    instr.effective_addr = REG[instr.rs1] + instr.imm;
                    REG[instr.rd] = (int8_t)data_segment[instr.effective_addr]; // Sign-extend to 32 bits
                    printf("[MEMORY] Loaded REG[%d] = 0x%08X (LB) from Address 0x%08X\n", instr.rd, REG[instr.rs2], instr.effective_addr);
                    break;
                case 0x1: // LH (Load Halfword)
                    instr.effective_addr = REG[instr.rs1] + instr.imm;
                    REG[instr.rd] = (int16_t)data_segment[instr.effective_addr]; // Sign-extend to 32 bits
                    printf("[MEMORY] Loaded REG[%d] = 0x%08X (LH) from Address 0x%08X\n", instr.rd, REG[instr.rs2], instr.effective_addr);
                    break;
                case 0x2: // LW (Load Word)
                    instr.effective_addr = REG[instr.rs1] + instr.imm;
                    REG[instr.rd] = data_segment[instr.effective_addr]; // Directly load 32 bits
                    printf("[MEMORY] Loaded REG[%d] = 0x%08X (LW) from Address 0x%08X\n", instr.rd, REG[instr.rs2], instr.effective_addr);
                    break;
                case 0x3: // LD (Load Doubleword) - Not supported in 32-bit, but included for completeness
                    instr.effective_addr = REG[instr.rs1] + instr.imm;
                    REG[instr.rd] = data_segment[instr.effective_addr]; // Directly load 32 bits (truncate for 32-bit)
                    printf("[MEMORY] Loaded REG[%d] = 0x%08X (LD) from Address 0x%08X\n", instr.rd, REG[instr.rs2], instr.effective_addr);
                    break;
                default:
                    printf("[MEMORY] Unknown load instruction (instr.func3) { = 0x%X)\n", instr.func3);
                    break;
            }
        } else if (instr.opcode == 0x23) { // Store instructions (S-Type: SB, SH, SW, SD)
            switch (instr.func3) {
                case 0x0: // SB (Store Byte)
                    data_segment[instr.effective_addr] = (uint8_t)REG[instr.rs2]; // Store lower 8 bits
                    printf("[MEMORY] Stored REG[%d] = 0x%02X (SB) to Address 0x%08X\n", instr.rs2, (uint8_t)REG[instr.rs2], instr.effective_addr);
                    break;
                case 0x1: // SH (Store Halfword)
                    data_segment[instr.effective_addr] = (uint16_t)REG[instr.rs2]; // Store lower 16 bits
                    printf("[MEMORY] Stored REG[%d] = 0x%04X (SH) to Address 0x%08X\n", instr.rs2, (uint16_t)REG[instr.rs2], instr.effective_addr);
                    break;
                case 0x2: // SW (Store Word)
                    data_segment[instr.effective_addr] = REG[instr.rs2]; // Store full 32 bits
                    printf("[MEMORY] Stored REG[%d] = 0x%08X (SW) to Address 0x%08X\n", instr.rs2, REG[instr.rs2], instr.effective_addr);
                    break;
                case 0x3: // SD (Store Doubleword) - Not supported in 32-bit, but included for completeness
                    data_segment[instr.effective_addr] = REG[instr.rs2]; // Store full 32 bits (truncate for 32-bit)
                    printf("[MEMORY] Stored REG[%d] = 0x%08X (SD) to Address 0x%08X\n", instr.rs2, REG[instr.rs2], instr.effective_addr);
                    break;
                default:
                    printf("[MEMORY] Unknown store instruction (instr.func3) { = 0x%X)\n", instr.func3);
                    break;
            }
        }
    }
    
    void writeback(Instruction &instr) {
        if (instr.opcode == 0x67) { // JALR
            REG[0] = 0;
            REG[instr.rd] = PC; // Save return address (PC + 4)
            REG[0] = 0;
            PC = instr.effective_addr & ~1; // Jump to effective address (clear LSB for alignment)
            printf("[WRITEBACK] REG[%d] = 0x%08X (JALR), Updated PC = 0x%08X\n", instr.rd, REG[instr.rd], PC);
        } else if (instr.opcode != 0x23) { // Skip writeback for SW only
            printf("[WRITEBACK] REG[%d] = 0x%08X\n", instr.rd, REG[instr.rd]);
        }
    }

    void save_final_state() {
        ofstream outFile("output.mc");
        outFile << "Final Registers:\n";
        for (int i = 0; i < 32; i++) {
            outFile << "R[" << dec << i << "]: 0x" << hex << REG[i] << "\n"; // Register index in decimal, value in hex
        }
        outFile << "\nFinal Memory State (Used Addresses):\n";
        
        // Print only the memory addresses that are used in the program
        for (const auto &entry : data_segment) {
            outFile << "Mem[0x" << hex << entry.first << "] = 0x" << entry.second << "\n";
        }
        
        outFile.close();
    }


public:
    void load_mc_file(const string& filename) {
        ifstream file(filename);
        string line;
        uint32_t pc, instruction;
        uint32_t addr, value;
        bool data_section = false;

        if (!file) {
            cerr << "Error: Could not open file " << filename << endl;
            return;
        }

        while (getline(file, line)) {
            istringstream iss(line);
            if (line.empty() || line[0] == '#') {
                if (line.find("#Data Segment") != string::npos) {
                    data_section = true;
                }
                continue;
            }

            if (!data_section) {
                if (iss >> hex >> pc >> instruction) {
                    instr_map[pc] = instruction;
                }
            } else {
                if (iss >> hex >> addr >> value) {
                    data_segment[addr] = static_cast<uint8_t>(value & 0xFF); // Extract lower 8 bits
                }
            }
        }
    }

    void run_simulator(){
        while(instr_map.find(PC) != instr_map.end()){
            Instruction instr;
            fetch(instr);
            decode(instr);
            execute(instr);
            memory_access(instr);
            writeback(instr);
            clock_cycles++;
            printf("Clock Cycle: %d\n\n", clock_cycles);
        }
        save_final_state();
        printf("Final Clock Cycles: %d\n", clock_cycles);
    }
};


int main(){
    REG[0] = 0;
    REG[2] = 0x7FFFFFDC;
    RiscVsimulator simulator;
    simulator.load_mc_file("input.mc");
    simulator.run_simulator();
    return 0;
}





