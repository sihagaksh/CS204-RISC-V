#include <bits/stdc++.h>
#include <iomanip>
using namespace std;

bool flush_if = false;
bool enableBranchPrediction = true;
ofstream logs("logs.txt");
bool flag_to_print_particular = false;
uint32_t instruction_number;
uint32_t pc_to_print_at;
// Structure of an instruction (for sequential simulation use)
struct Instruction {
    uint32_t opcode;
    uint32_t rd;
    uint32_t rs1;
    uint32_t rs2;
    uint32_t func3;
    uint32_t func7;
    int imm;
    uint32_t effective_addr;
};

// Pipeline inter-stage buffer structures
struct IF_ID {
    bool valid = false;
    uint32_t pc;
    uint32_t ir;
    int inst_num;
};
struct ID_EX {
    bool valid = false;
    uint32_t pc;
    uint32_t opcode;
    uint32_t func3;
    uint32_t func7;
    uint32_t rd;
    uint32_t rs1;
    uint32_t rs2;
    int imm;
    int inst_num;
    bool isBranch;
    bool isJump;
    bool memRead;
    bool memWrite;
    bool regWrite;
};
struct EX_MEM {
    bool valid = false;
    uint32_t pc;
    uint32_t rd;
    uint32_t alu_result;
    uint32_t mem_reg;
    int size;
    bool write_enable;
    bool mem_read;
    bool mem_write;
    bool branch_taken;
    bool jump_taken;
    bool is_ecall;
    uint32_t next_pc;
    int inst_num;
    bool misprediction_checked = false; 
};
struct MEM_WB {
    bool valid = false;
    uint32_t pc;
    uint32_t rd;
    uint32_t write_value;
    bool reg_write;
    bool is_ecall;
    int inst_num;
};

// Global state and memory
int stat_branch_predictions = 0;
uint32_t PC = 0;
uint32_t IR = 0;
uint32_t REG[32] = {0};
map<uint32_t, uint32_t> instr_map;
map<uint32_t, uint8_t> data_segment;

// Writeback flags (for sequential simulation)
bool writeBackFlag = false;
int writeBackReg = 0;
int writeBackValue = 0;

// Pipeline registers
IF_ID if_id;
ID_EX id_ex;
EX_MEM ex_mem;
MEM_WB mem_wb;

// Branch predictor (1-bit) and branch target buffer
struct BPUEntry { char prediction; uint32_t target; };
map<uint32_t, BPUEntry> branchPred;

// Knob flags (debugging features)
bool knob_pipeline = false;
bool knob_forwarding = false;
bool knob_print_regs = false;
// bool knob_print_particular_instruction = false;
bool knob_print_logs = false;
bool knob_print_pipeline = false;
bool knob_trace = false;
bool knob_print_branch = false;
int trace_inst_num = -1;

// Pipeline control flags
bool pipeline_stall = false;
bool stop_fetch = false;

// Statistics counters
unsigned long long stat_cycles = 0;
unsigned long long stat_instructions = 0;
unsigned long long stat_load_store = 0;
unsigned long long stat_alu = 0;
unsigned long long stat_control = 0;
unsigned long long stat_stalls = 0;
unsigned long long stat_data_hazards = 0;
unsigned long long stat_ctrl_hazards = 0;
unsigned long long stat_branch_misp = 0;
unsigned long long stat_stall_data = 0;
unsigned long long stat_stall_control = 0;

// Mapping from PC to instruction number (input program order)
map<uint32_t, int> inst_num_map;

// Simulator class (for loading and sequential execution)
class RiscVsimulator {
public:
    void load_mc_file(const string &filename) {
        ifstream file(filename);
        if (!file) {
            cerr << "Error: Could not open file " << filename << endl;
            exit(1);
        }
        string line;
        bool data_section = false;
        int inst_count = 0;
        while (getline(file, line)) {
            if (line.empty() || line[0] == '#') {
                if (line.find("#Data Segment") != string::npos) {
                    data_section = true;
                }
                continue;
            }
            istringstream iss(line);
            if (!data_section) {
                uint32_t pc, inst;
                if (iss >> std::hex >> pc >> inst) {
                    instr_map[pc] = inst;
                    inst_count++;
                    inst_num_map[pc] = inst_count;
                }
            } else {
                uint32_t addr, value;
                if (iss >> std::hex >> addr >> value) {
                    data_segment[addr] = static_cast<uint8_t>(value & 0xFF);
                }
            }
        }
        file.close();
        if (!instr_map.empty()) {
            PC = instr_map.begin()->first;
        }
    }

    void run_sequential() {
        REG[0] = 0;
        REG[2] = 0x7FFFFFDC;
        REG[3] = 0x7FFFFFDC;
        REG[3] = 0x10000000;
        REG[10] = 0x1;
        while (instr_map.find(PC) != instr_map.end()) {
            Instruction instr;
            fetch(instr);
            stat_cycles++; // Fetch

            decode(instr);
            stat_cycles++; // Decode

            execute(instr);
            stat_cycles++; // Execute

            // Memory access only for Load/Store
            if (instr.opcode == 0x03 || instr.opcode == 0x23) {
                memory_access(instr);
                stat_cycles++; // Memory access
            }

            // Writeback only for instructions that write to registers
            // Store (0x23) doesn't write back, rest might
            if (instr.opcode != 0x23 && instr.opcode != 0x63) {
                writeback(instr);
                stat_cycles++; // Writeback
            }

            stat_instructions++; // 1 instruction retired
            // Classify instruction type for stats
            if (instr.opcode == 0x03 || instr.opcode == 0x23) {
                stat_load_store++;
            } else if (instr.opcode == 0x33 || instr.opcode == 0x13 || instr.opcode == 0x37 || instr.opcode == 0x17) {
                stat_alu++;
            } else if (instr.opcode == 0x63 || instr.opcode == 0x6F || instr.opcode == 0x67) {
                stat_control++;
            }
            // Terminate on ECALL
            if (instr.opcode == 0x73) {
                break;
            }
            if (knob_print_regs) {
                cout << "Registers after cycle " << dec << stat_cycles << ":\n";
                for (int i = 0; i < 32; ++i) {
                    cout << "R[" << i << "] = 0x" << hex << REG[i] << "\n";
                }
            }
        }
    }

public:
    void save_final_state() {
        ofstream outFile("output.mc");
        outFile << "Final Registers:\n";
        REG[0] = 0;
        for (int i = 0; i < 32; ++i) {
            outFile << "R[" << dec << i << "]: 0x" << hex << REG[i] << "\n";
        }
        outFile << "\nFinal Memory State (Used Addresses):\n";
        for (auto &entry : data_segment) {
            outFile << "Mem[0x" << hex << entry.first << "] = 0x" << (int)entry.second << "\n";
        }
        outFile.close();
    }

private:
    // Phase 2 (non-pipelined) stage implementations
    void fetch(Instruction &instr) {
        IR = instr_map[PC];
        printf("[FETCH] PC: 0x%08X -> Instruction: 0x%08X\n", PC, IR);
        PC += 4;
        cout << endl;
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
        cout << endl;
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
        cout << endl;
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
        cout << endl;
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
        cout << endl;
    }
    // void save_final_state() {
    //     ofstream outFile("output.mc");
    //     outFile << "Final Registers:\n";
    //     for (int i = 0; i < 32; ++i) {
    //         outFile << "R[" << dec << i << "]: 0x" << hex << REG[i] << "\n";
    //     }
    //     outFile << "\nFinal Memory State (Used Addresses):\n";
    //     for (auto &entry : data_segment) {
    //         outFile << "Mem[0x" << hex << entry.first << "] = 0x" << (int)entry.second << "\n";
    //     }
    //     outFile.close();
    // }
} simulator;

// Forwarding helper for pipeline
uint32_t get_forwarded_value(uint32_t reg) {
    if (reg == 0) return 0;
    if (ex_mem.valid && ex_mem.write_enable && ex_mem.rd == reg && !ex_mem.mem_read) {
        return ex_mem.alu_result;
    }
    if (mem_wb.valid && mem_wb.reg_write && mem_wb.rd == reg) {
        return mem_wb.write_value;
    }
    return REG[reg];
}

// Pipeline stage functions
void pipeline_fetch() {
    if (pipeline_stall || if_id.valid || stop_fetch) {
        logs << "returned from fetch because pipeline_stall or if_id is valid or stop_fetch is true" << endl;
        return;
    }
    logs << "FETCH" << endl;
    auto it = instr_map.find(PC);
    if (it == instr_map.end()) {
        stop_fetch = true;
        return;
    }
    uint32_t inst = it->second;
    if_id.ir = inst;
    if_id.pc = PC;
    if_id.inst_num = inst_num_map[PC];
    if_id.valid = true;
    uint32_t opcode = inst & 0x7F;
    if (if_id.pc == pc_to_print_at && flag_to_print_particular){
        cout << endl;
        cout << "FETCH-DECODE BUFFER" << endl;
        cout << "intruction is: " << hex << inst << endl;
        cout << "PC is: " << hex << PC << endl;
        cout << "instruction number is: " << dec << if_id.inst_num << endl;
        cout << "valid is: " << if_id.valid << endl;
        cout <<"----------------------------------------"<<endl;
        cout << endl;
    }
    logs << "intruction is: " << hex << inst << endl;
    logs << "PC is: " << hex << PC << endl;
    logs << "instruction number is: " << dec << if_id.inst_num << endl;
    logs << "valid is: " << if_id.valid << endl;

    if (opcode == 0x63 && enableBranchPrediction) {  // Branch instruction
        // Extract the immediate value for the branch
        int32_t imm = 0;
        imm |= ((inst >> 31) & 0x1) << 12;    // imm[12]
        imm |= ((inst >> 25) & 0x3F) << 5;    // imm[10:5]
        imm |= ((inst >> 8) & 0xF) << 1;      // imm[4:1]
        imm |= ((inst >> 7) & 0x1) << 11;     // imm[11]
        if (imm & (1 << 12)) imm |= 0xFFFFE000;  // Sign-extend

        uint32_t target = PC + imm;  // Calculate branch target address

        // If the branch instruction has not been predicted before, set the default prediction
        if (branchPred.find(PC) == branchPred.end()) {
            branchPred[PC] = {'N', target};  // Default to 'N' (Not Taken) prediction
        }

        // Increment the branch prediction count
        stat_branch_predictions++;

        // Get the current prediction for the branch at the current PC
        char prediction = branchPred[PC].prediction;
        
        // Update the PC based on the prediction
        if (prediction == 'T') {  // Taken prediction
            PC = branchPred[PC].target;
        } else {  // Not Taken prediction
            PC += 4;  // Move to the next sequential instruction
        }

    } 
    else if (opcode == 0x6F) { // jal
        int imm20 = ((inst >> 31) & 1) << 20;
        int imm10_1 = ((inst >> 21) & 0x3FF) << 1;
        int imm11 = ((inst >> 20) & 1) << 11;
        int imm19_12 = ((inst >> 12) & 0xFF) << 12;
        int imm = imm20 | imm19_12 | imm11 | imm10_1;
        if (imm & (1 << 20)) imm |= 0xFFE00000; // Sign-extend
        PC += imm;
        stat_stall_control++;
        stat_stalls++;
    }
    else {
        PC += 4;
    }

    logs << "updated pc after fetch is: " << dec << PC << endl;
    logs << "----------------------------------------" << endl;
    logs << endl;
}


void pipeline_decode() {
    if (!if_id.valid) {
        id_ex.valid = false;
        logs<<"returned from decode because if_id is not valid"<<endl;
        return;
    }
    logs<<"DECODE"<<endl;
    uint32_t ir = if_id.ir;
    uint32_t pc = if_id.pc;
    uint32_t opcode = ir & 0x7F;
    uint32_t rd = (ir >> 7) & 0x1F;
    uint32_t func3 = (ir >> 12) & 0x07;
    uint32_t rs1 = (ir >> 15) & 0x1F;
    uint32_t rs2 = (ir >> 20) & 0x1F;
    uint32_t func7 = (ir >> 25) & 0x7F;
    int imm = 0;
    switch (opcode) {
        case 0x33: imm = 0; break;
        case 0x03: case 0x13: case 0x67: {
            imm = (int32_t)(ir >> 20);
            imm |= ((imm & (1 << 11)) ? 0xFFFFF000 : 0);
            if (opcode != 0x03) rs2 = 0;
            break;
        }
        case 0x23: {
            int imm4_0 = (ir >> 7) & 0x1F;
            int imm11_5 = (ir >> 25) & 0x7F;
            imm = (imm11_5 << 5) | imm4_0;
            if (imm & (1 << 11)) imm |= 0xFFFFF000;
            rd = 0;
            break;
        }
        case 0x63: {
            int imm11 = ((ir >> 7) & 0x1) << 11;
            int imm4_1 = ((ir >> 8) & 0xF) << 1;
            int imm10_5 = ((ir >> 25) & 0x3F) << 5;
            int imm12 = ((ir >> 31) & 0x1) << 12;
            imm = imm12 | imm11 | imm10_5 | imm4_1;
            if (imm & (1 << 12)) imm |= 0xFFFFE000;
            imm >>= 1;
            rd = 0;
            break;
        }
        case 0x37: case 0x17: {
            imm = ir & 0xFFFFF000;
            if (ir & 0x80000000) imm |= 0xFFF00000;
            rs1 = rs2 = 0;
            break;
        }
        case 0x6F: {
            int imm20 = ((ir >> 31) & 1) << 20;
            int imm10_1 = ((ir >> 21) & 0x3FF) << 1;
            int imm11 = ((ir >> 20) & 1) << 11;
            int imm19_12 = ((ir >> 12) & 0xFF) << 12;
            imm = imm20 | imm19_12 | imm11 | imm10_1;
            if (imm & (1 << 20)) imm |= 0xFFE00000;
            break;
        }
        case 0x73:
            imm = 0;
            break;
        default:
            imm = 0;
            break;
    }
    bool hazard = false;
    bool stall = false;
    if ((opcode == 0x33 || opcode == 0x63 || opcode == 0x23 || opcode == 0x13 || opcode == 0x03 || opcode == 0x67) && rs1 != 0) {
        if ((ex_mem.valid && ex_mem.write_enable && ex_mem.rd == rs1) ||
            (mem_wb.valid && mem_wb.reg_write && mem_wb.rd == rs1)) {
            logs<<"hazard detected"<<endl;
            hazard = true;
            if (!knob_forwarding){
                logs<<"stalling because knob_forwarding is false"<<endl;
                stall = true;
            }
        }
    }
    if ((opcode == 0x33 || opcode == 0x63 || opcode == 0x23) && rs2 != 0) {
        if ((ex_mem.valid && ex_mem.write_enable && ex_mem.rd == rs2) ||
            (mem_wb.valid && mem_wb.reg_write && mem_wb.rd == rs2)) {
            hazard = true;
            if (!knob_forwarding){
                logs<<"stalling because knob_forwarding is false"<<endl;
                stall = true;
            }
        }
    }
    if (ex_mem.valid && ex_mem.mem_read) {
        if (rs1 != 0 && ex_mem.rd == rs1) {
            hazard = true;
            stall = true;
        }
        if (rs2 != 0 && ex_mem.rd == rs2) {
            hazard = true;
            stall = true;
        }
    }
    if (hazard) {
        int count = 0;
        if ((opcode == 0x33 || opcode == 0x63 || opcode == 0x23 || opcode == 0x13 || opcode == 0x03 || opcode == 0x67) && rs1 != 0) {
            if ((ex_mem.valid && ex_mem.write_enable && ex_mem.rd == rs1) ||
                (mem_wb.valid && mem_wb.reg_write && mem_wb.rd == rs1)) count++;
        }
        if ((opcode == 0x33 || opcode == 0x63 || opcode == 0x23) && rs2 != 0) {
            if ((ex_mem.valid && ex_mem.write_enable && ex_mem.rd == rs2) ||
                (mem_wb.valid && mem_wb.reg_write && mem_wb.rd == rs2)) count++;
        }
        stat_data_hazards += count;
    }
    if (stall) {
        id_ex.valid = false;
        pipeline_stall = true;
        stat_stalls++;
        stat_stall_data++;
        return;
    }
    pipeline_stall = false;
    id_ex.valid = true;
    id_ex.pc = pc;
    id_ex.opcode = opcode;
    id_ex.func3 = func3;
    id_ex.func7 = func7;
    id_ex.rd = rd;
    id_ex.rs1 = rs1;
    id_ex.rs2 = rs2;
    id_ex.imm = imm;
    id_ex.inst_num = if_id.inst_num;
    id_ex.isBranch = (opcode == 0x63);
    id_ex.isJump   = (opcode == 0x6F || opcode == 0x67);
    id_ex.memRead  = (opcode == 0x03);
    id_ex.memWrite = (opcode == 0x23);
    id_ex.regWrite = false;
    if ((opcode == 0x33 || opcode == 0x13 || opcode == 0x37 || opcode == 0x17 ||
         opcode == 0x6F || opcode == 0x67 || opcode == 0x03) && rd != 0) {
        id_ex.regWrite = true;
    }
    if (opcode == 0x73) {
        stop_fetch = true;
    }
    if_id.valid = false;
    if (id_ex.pc == pc_to_print_at && flag_to_print_particular){
        cout <<"DECODE-EXECUTE BUFFER"<<endl;
        cout <<"instruction is: "<<hex<<ir<<endl;
        cout <<"pc is: "<<hex<<pc<<endl;
        cout <<"opcode is: "<<hex<<opcode<<endl;
        cout <<"rd is: "<<dec<<rd<<endl;
        cout <<"rs1 is: "<<dec<<rs1<<endl;
        cout <<"rs2 is: "<<dec<<rs2<<endl;
        cout <<"func3 is: "<<dec<<func3<<endl;
        cout <<"func7 is: "<<dec<<func7<<endl;
        cout <<"imm is: "<<dec<<imm<<endl;
        cout <<"valid is: "<<id_ex.valid<<endl;
        cout <<"isBranch is: "<<id_ex.isBranch<<endl;
        cout <<"isJump is: "<<id_ex.isJump<<endl;
        cout <<"memRead is: "<<id_ex.memRead<<endl;
        cout <<"memWrite is: "<<id_ex.memWrite<<endl;
        cout <<"regWrite is: "<<id_ex.regWrite<<endl;
        cout <<"inst_num is: "<<dec<<if_id.inst_num<<endl;
        cout <<"----------------------------------------"<<endl;
        cout <<endl;
    }
    logs<<"instruction is: "<<hex<<ir<<endl;
    logs<<"pc is: "<<hex<<pc<<endl;
    logs<<"opcode is: "<<hex<<opcode<<endl;
    logs<<"rd is: "<<dec<<rd<<endl;
    logs<<"rs1 is: "<<dec<<rs1<<endl;
    logs<<"rs2 is: "<<dec<<rs2<<endl;
    logs<<"func3 is: "<<dec<<func3<<endl;
    logs<<"func7 is: "<<dec<<func7<<endl;
    logs<<"imm is: "<<dec<<imm<<endl;
    logs<<"valid is: "<<id_ex.valid<<endl;
    logs<<"isBranch is: "<<id_ex.isBranch<<endl;
    logs<<"isJump is: "<<id_ex.isJump<<endl;
    logs<<"memRead is: "<<id_ex.memRead<<endl;
    logs<<"memWrite is: "<<id_ex.memWrite<<endl;
    logs<<"regWrite is: "<<id_ex.regWrite<<endl;
    logs<<"inst_num is: "<<dec<<if_id.inst_num<<endl;
    logs<<"----------------------------------------"<<endl;
    logs<<endl;
}

void pipeline_execute() {
    if (!id_ex.valid) {
        ex_mem.valid = false;
        logs<<"returned from execute because id_ex is not valid"<<endl;
        return;
    }
    logs<<"EXECUTE"<<endl;
    logs<<"PC while entering the execute is " << dec << id_ex.pc <<endl; 
    ex_mem.valid = true;
    ex_mem.write_enable = false;
    ex_mem.mem_read = false;
    ex_mem.mem_write = false;
    ex_mem.branch_taken = false;
    ex_mem.jump_taken = false;
    ex_mem.is_ecall = false;
    ex_mem.rd = 0;
    ex_mem.mem_reg = 0;
    ex_mem.size = 0;
    ex_mem.pc = id_ex.pc;
    ex_mem.inst_num = id_ex.inst_num;
    ex_mem.next_pc = id_ex.pc + 4;
    uint32_t op = id_ex.opcode;
    uint32_t rs1 = id_ex.rs1, rs2 = id_ex.rs2, rd = id_ex.rd;
    uint32_t func3 = id_ex.func3, func7 = id_ex.func7;
    int imm = id_ex.imm;
    uint32_t val1 = 0, val2 = 0;

    if (op == 0x33 || op == 0x13 || op == 0x03 || op == 0x67 || op == 0x63 || op == 0x23) { //R-type, I-type, S-type, B-type
        val1 = get_forwarded_value(rs1);
    }
    if (op == 0x33 || op == 0x63 || op == 0x23) { //R-type, B-type, S-type
        val2 = get_forwarded_value(rs2);
    }
    switch (op) {
        case 0x33:
            ex_mem.rd = rd;
            ex_mem.write_enable = true;
            switch (func3) {
                case 0x0:
                    ex_mem.alu_result = (func7 == 0x20) ? (int32_t)val1 - (int32_t)val2 : (int32_t)val1 + (int32_t)val2;
                    break;
                case 0x7: ex_mem.alu_result = val1 & val2; break;
                case 0x6: ex_mem.alu_result = val1 | val2; break;
                case 0x4: ex_mem.alu_result = val1 ^ val2; break;
                case 0x1: ex_mem.alu_result = val1 << (val2 & 0x1F); break;
                case 0x5:
                    ex_mem.alu_result = (func7 == 0x20) ? ((int32_t)val1 >> (val2 & 0x1F)) : (val1 >> (val2 & 0x1F));
                    break;
                case 0x2: ex_mem.alu_result = ((int32_t)val1 < (int32_t)val2) ? 1 : 0; break;
                case 0x3: ex_mem.alu_result = (val1 < val2) ? 1 : 0; break;
            }
            if (func7 == 0x01) {
                if (func3 == 0x0) ex_mem.alu_result = (int32_t)val1 * (int32_t)val2;
                if (func3 == 0x4) ex_mem.alu_result = ((int32_t)val2 != 0 ? (int32_t)val1 / (int32_t)val2 : 0);
                if (func3 == 0x6) ex_mem.alu_result = ((int32_t)val2 != 0 ? (int32_t)val1 % (int32_t)val2 : 0);
            }
            break;
        case 0x13:
            ex_mem.rd = rd;
            ex_mem.write_enable = true;
            switch (func3) {
                case 0x0: ex_mem.alu_result = (int32_t)val1 + imm; break;
                case 0x7: ex_mem.alu_result = val1 & imm; break;
                case 0x6: ex_mem.alu_result = val1 | imm; break;
                case 0x4: ex_mem.alu_result = val1 ^ imm; break;
                case 0x2: ex_mem.alu_result = ((int32_t)val1 < imm) ? 1 : 0; break;
                case 0x3: ex_mem.alu_result = ((uint32_t)val1 < (uint32_t)imm) ? 1 : 0; break;
                case 0x1: ex_mem.alu_result = val1 << (imm & 0x1F); break;
                case 0x5:
                    ex_mem.alu_result = ((imm & 0x400) ? (int32_t)val1 >> (imm & 0x1F) : (val1 >> (imm & 0x1F)));
                    break;
            }
            break;
        case 0x03:
            ex_mem.rd = rd;
            ex_mem.write_enable = true;
            ex_mem.mem_read = true;
            ex_mem.size = (func3 == 0x0 ? 1 : func3 == 0x1 ? 2 : 4);
            ex_mem.alu_result = val1 + imm;
            break;
        case 0x23:
            ex_mem.write_enable = false;
            ex_mem.mem_write = true;
            ex_mem.mem_reg = rs2;
            ex_mem.size = (func3 == 0x0 ? 1 : func3 == 0x1 ? 2 : 4);
            ex_mem.alu_result = val1 + imm;
            break;
        case 0x37:
            ex_mem.rd = rd;
            ex_mem.write_enable = true;
            ex_mem.alu_result = imm;
            break;
        case 0x17:
            ex_mem.rd = rd;
            ex_mem.write_enable = true;
            ex_mem.alu_result = id_ex.pc + imm;
            break;
        case 0x6F:
            ex_mem.rd = rd;
            ex_mem.write_enable = true;
            ex_mem.jump_taken = true;
            ex_mem.alu_result = id_ex.pc + 4;
            ex_mem.next_pc = id_ex.pc + imm;
            stat_ctrl_hazards++;
            break;
        case 0x67:
            ex_mem.rd = rd;
            ex_mem.write_enable = true;
            ex_mem.jump_taken = true;
            ex_mem.alu_result = id_ex.pc + 4;
            ex_mem.next_pc = (val1 + imm) & ~1;
            stat_ctrl_hazards++;
            break;
        case 0x63: {
            bool taken = false;
            switch (func3) {
                case 0x0: taken = (val1 == val2); break;
                case 0x1: taken = (val1 != val2); break;
                case 0x4: taken = ((int32_t)val1 < (int32_t)val2); break;
                case 0x5: taken = ((int32_t)val1 >= (int32_t)val2); break;
                case 0x6: taken = (val1 < val2); break;
                case 0x7: taken = (val1 >= val2); break;
            }

            ex_mem.branch_taken = taken;
            //if this is not the same as the prediction of map, increment mispredictions
            logs << "taken is : " << taken << endl;
            logs << "branch prediction is " << branchPred[ex_mem.pc].prediction << endl;
            int tempu;
            //map taken and not taken to 1 and 0 respectively
            if (branchPred[ex_mem.pc].prediction == 'T' ){
                tempu = 1;
            }
            else{
                tempu = 0;
            }
            // cout << "tempu is " << tempu << endl;
            if (taken != tempu){
                stat_branch_misp++;
                // cout << "misprediction count " << stat_branch_misp << endl;
            }

            if (taken) {
                ex_mem.next_pc = id_ex.pc + (id_ex.imm << 1);  // imm is already sign-extended
                // cout << "i am here and this branch is taken" << endl;
                // cout << "next pc is: " << hex << ex_mem.next_pc << endl;
            } else {
                ex_mem.next_pc = id_ex.pc + 4;
            }

            if (enableBranchPrediction) {
                if (branchPred.find(id_ex.pc) != branchPred.end()) {
                    char pred = branchPred[id_ex.pc].prediction;
                    bool wasPredictedTaken = (pred == 'T');

                    // ✅ NOTE: Misprediction count happens in main(), so don't do it here again!

                    // Update prediction to reflect actual outcome
                    branchPred[id_ex.pc].prediction = taken ? 'T' : 'N';
                    if (taken) {
                        branchPred[id_ex.pc].target = ex_mem.next_pc;
                    }
                }
                // if (taken){
                //     stat_stalls++;
                // }
                if (tempu != taken){
                    stat_ctrl_hazards++;
                    stat_stalls += 2;
                    stat_stall_control += 2;

                }
                // if (taken == 1 && tempu == 0){
                //     stat_stall_control += 2;
                // }
            }
            ex_mem.misprediction_checked = false; // ✅ reset here so main() can use it

            break;
        }


        case 0x73:
            ex_mem.is_ecall = true;
            ex_mem.write_enable = false;
            break;
        default:
            ex_mem.valid = false;
            break;
    }
    if (ex_mem.pc == pc_to_print_at && flag_to_print_particular){
        cout <<"EXECUTE-MEMORY BUFFER"<<endl;
        cout <<"opcode is: "<<hex<<op<<endl;
        cout <<"rs1 is: "<<dec<<rs1<<endl;
        cout <<"rs2 is: "<<dec<<rs2<<endl;
        cout <<"rd is: "<<dec<<rd<<endl;
        cout <<"func3 is: "<<dec<<func3<<endl;
        cout <<"func7 is: "<<dec<<func7<<endl;
        cout <<"imm is: "<<dec<<imm<<endl;
        cout <<"alu_result is: "<<hex<<ex_mem.alu_result<<endl;
        cout <<"write_enable is: "<<ex_mem.write_enable<<endl;
        cout <<"mem_read is: "<<ex_mem.mem_read<<endl;
        cout <<"mem_write is: "<<ex_mem.mem_write<<endl;
        cout <<"branch_taken is: "<<ex_mem.branch_taken<<endl;
        cout <<"jump_taken is: "<<ex_mem.jump_taken<<endl;
        cout <<"next_pc is: "<<hex<<ex_mem.next_pc<<endl;
        cout <<"is_ecall is: "<<ex_mem.is_ecall<<endl;
        cout <<"mem_reg is: "<<dec<<ex_mem.mem_reg<<endl;
        cout <<"size is (d, word, half, byte): "<<dec<<ex_mem.size<<endl;
        cout <<"inst_num is: "<<dec<<ex_mem.inst_num<<endl;
        cout <<"valid is: "<<ex_mem.valid<<endl;
        cout <<"pc here is "<<dec<<PC<<endl;
        cout <<"----------------------------------------"<<endl;
        cout <<endl;
    }
    logs<<"opcode is: "<<hex<<op<<endl;
    logs<<"rs1 is: "<<dec<<rs1<<endl;
    logs<<"rs2 is: "<<dec<<rs2<<endl;
    logs<<"rd is: "<<dec<<rd<<endl;
    logs<<"func3 is: "<<dec<<func3<<endl;
    logs<<"func7 is: "<<dec<<func7<<endl;
    logs<<"imm is: "<<dec<<imm<<endl;
    logs<<"alu_result is: "<<hex<<ex_mem.alu_result<<endl;
    logs<<"write_enable is: "<<ex_mem.write_enable<<endl;
    logs<<"mem_read is: "<<ex_mem.mem_read<<endl;
    logs<<"mem_write is: "<<ex_mem.mem_write<<endl;
    logs<<"branch_taken is: "<<ex_mem.branch_taken<<endl;
    logs<<"jump_taken is: "<<ex_mem.jump_taken<<endl;
    logs<<"next_pc is: "<<hex<<ex_mem.next_pc<<endl;
    logs<<"is_ecall is: "<<ex_mem.is_ecall<<endl;
    logs<<"mem_reg is: "<<dec<<ex_mem.mem_reg<<endl;
    logs<<"size is (d, word, half, byte): "<<dec<<ex_mem.size<<endl;
    logs<<"inst_num is: "<<dec<<ex_mem.inst_num<<endl;
    logs<<"valid is: "<<ex_mem.valid<<endl;
    logs<<"pc here is "<<dec<<PC<<endl;
    logs<<"----------------------------------------"<<endl;
    logs<<endl;
}

void pipeline_memory() {
    if (!ex_mem.valid) {
        logs<<"returned from memory because ex_mem is not valid"<<endl;
        mem_wb.valid = false;
        return;
    }
    logs<<"MEMORY"<<endl;
    mem_wb.valid = true;
    mem_wb.reg_write = ex_mem.write_enable;
    mem_wb.rd = ex_mem.rd;
    mem_wb.is_ecall = ex_mem.is_ecall;
    mem_wb.inst_num = ex_mem.inst_num;
    mem_wb.pc = ex_mem.pc;
    uint32_t addr = ex_mem.alu_result;
    if (ex_mem.mem_read) {
        uint32_t val = 0;
        for (int i = 0; i < ex_mem.size; ++i) {
            uint32_t a = addr + i;
            if (data_segment.find(a) == data_segment.end()) {
                data_segment[a] = 0;
            }
            val |= (data_segment[a] << (8 * i));
        }
        if (ex_mem.size == 1) val = (int8_t)(val & 0xFF);
        if (ex_mem.size == 2) val = (int16_t)(val & 0xFFFF);
        mem_wb.write_value = val;
    } else if (ex_mem.mem_write) {
        uint32_t store_val = get_forwarded_value(ex_mem.mem_reg);
        for (int i = 0; i < ex_mem.size; ++i) {
            uint32_t a = addr + i;
            uint8_t byte = (store_val >> (8 * i)) & 0xFF;
            data_segment[a] = byte;
        }
        mem_wb.reg_write = false;
        mem_wb.write_value = 0;
    } else {
        mem_wb.write_value = ex_mem.alu_result;
    }
    ex_mem.valid = false;
    if (mem_wb.pc == pc_to_print_at && flag_to_print_particular){
        cout <<"MEMORY-WRITEBACK BUFFER"<<endl;
        cout <<"alu_result is: "<<hex<<ex_mem.alu_result<<endl;
        cout <<"mem_read is: "<<ex_mem.mem_read<<endl;
        cout <<"mem_write is: "<<ex_mem.mem_write<<endl;
        cout <<"write_value is: "<<hex<<mem_wb.write_value<<endl;
        cout <<"reg_write is: "<<mem_wb.reg_write<<endl;
        cout <<"rd is: "<<dec<<mem_wb.rd<<endl;
        cout <<"is_ecall is: "<<mem_wb.is_ecall<<endl;
        cout <<"inst_num is: "<<dec<<mem_wb.inst_num<<endl;
        cout <<"pc is: "<<hex<<mem_wb.pc<<endl;
        cout <<"----------------------------------------"<<endl;
        cout <<endl;
    }
    logs<<"alu_result is: "<<hex<<ex_mem.alu_result<<endl;
    logs<<"mem_read is: "<<ex_mem.mem_read<<endl;
    logs<<"mem_write is: "<<ex_mem.mem_write<<endl;
    logs<<"write_value is: "<<hex<<mem_wb.write_value<<endl;
    logs<<"reg_write is: "<<mem_wb.reg_write<<endl;
    logs<<"rd is: "<<dec<<mem_wb.rd<<endl;
    logs<<"is_ecall is: "<<mem_wb.is_ecall<<endl;
    logs<<"inst_num is: "<<dec<<mem_wb.inst_num<<endl;
    logs<<"pc is: "<<hex<<mem_wb.pc<<endl;
    logs<<"----------------------------------------"<<endl;
    logs<<endl;
}

void pipeline_writeback() {
    if (!mem_wb.valid){
        logs<<"returned from writeback because mem_wb is not valid"<<endl;
        return;
    }
    logs<<"WRITEBACK"<<endl;
    if (mem_wb.reg_write && mem_wb.rd != 0) {
        REG[mem_wb.rd] = mem_wb.write_value;
    }
    // Count the committed instruction
    stat_instructions++;
    uint32_t committed_pc = mem_wb.pc;
    if (instr_map.find(committed_pc) != instr_map.end()) {
        uint32_t inst_bits = instr_map[committed_pc];
        uint32_t op = inst_bits & 0x7F;
        if (op == 0x03 || op == 0x23) stat_load_store++;
        else if (op == 0x33 || op == 0x13 || op == 0x37 || op == 0x17) stat_alu++;
        else if (op == 0x6F || op == 0x67 || op == 0x63) stat_control++; //jal jalr branch
    }
    REG[0] = 0;
    mem_wb.valid = false;
    logs<<"write_value is: "<<hex<<mem_wb.write_value<<endl;
    logs<<"reg_write is: "<<mem_wb.reg_write<<endl;
    logs<<"rd is: "<<dec<<mem_wb.rd<<endl;
    logs<<"is_ecall is: "<<mem_wb.is_ecall<<endl;
    logs<<"inst_num is: "<<dec<<mem_wb.inst_num<<endl;
    logs<<"pc is: "<<hex<<mem_wb.pc<<endl;
    logs<<"----------------------------------------"<<endl;
    logs<<endl;
}
int main(int argc, char* argv[]) {
    string input_file = "input.mc";
    
    // Parse command-line arguments for knobs and input file
    for (int i = 1; i < argc; ++i) {
        string arg = argv[i];
        if (arg == "--pipeline") knob_pipeline = true;
        else if (arg == "--no-pipeline") knob_pipeline = false; //knob 1
        else if (arg == "--forwarding") knob_forwarding = true; //knob 2
        else if (arg == "--no-forwarding") knob_forwarding = false;
        else if (arg == "--print-registers") knob_print_regs = true; //knob 3
        else if (arg == "--print-logs") knob_print_logs = true; //knob 4
        else if (arg == "--print-pipeline") knob_print_pipeline = true; //print the pipeline at a particular cycle
        else if (arg == "--print-particular"){ //knob 5
            flag_to_print_particular = true;
            cout << "Enter the instruction number to print the pipeline at: ";
            cin >> instruction_number;
            pc_to_print_at = instruction_number*4 - 4;
            // knob_print_particular_instruction = true;
        }
        else if (arg == "--trace") {
            if (i + 1 < argc) {
                knob_trace = true;
                trace_inst_num = stoi(argv[++i]);
            }
        }
        else if (arg == "--print-branch") knob_print_branch = true; //knob 6
        else if (arg == "--input") {
            if (i + 1 < argc) input_file = argv[++i];
        }
    }

    // Initialize registers
    REG[0] = 0;
    REG[2] = 0x7FFFFFDC;
    REG[11] = 0x7FFFFFDC;
    REG[3] = 0x10000000;
    REG[10] = 0x1;
    

    // Load machine code
    simulator.load_mc_file(input_file);

    if (!knob_pipeline) {
        simulator.run_sequential();
    } else {
        // Initialize pipeline registers and branch predictor
        if_id.valid = false;
        id_ex.valid = false;
        ex_mem.valid = false;
        mem_wb.valid = false;

        // for (auto &entry : instr_map) {
        //     uint32_t pc = entry.first;
        //     uint32_t op = entry.second & 0x7F;
        //     // if (op == 0x63) {
        //     //     branchPred[pc] = {'N', 0};  // or 'T' if you prefer to initialize as taken
        //     // }
        // }

        REG[0] = 0;

        // Begin pipeline loop
        while (true) {
            pipeline_writeback();
            pipeline_memory();
            pipeline_execute();

            // Handle branch/jump misprediction and flush
            if (ex_mem.valid && (ex_mem.branch_taken || ex_mem.jump_taken)) {
                logs << "i am in flushing pipeline where pc is " << dec << PC << endl;
                logs << "the contents of ex_mem are: " << endl;
                logs << "valid is: " << ex_mem.valid << endl;
                logs << "branch_taken is: " << ex_mem.branch_taken << endl;
                logs << "jump_taken is: " << ex_mem.jump_taken << endl;
                logs << "next_pc is: " << dec << ex_mem.next_pc << endl;
                logs << "if_id.pc is: " << dec << if_id.pc << endl;
                logs << "-----------------------------------------" << endl << endl;

                // if (!ex_mem.misprediction_checked &&
                //     ex_mem.branch_taken && !ex_mem.jump_taken &&
                //     branchPred.find(ex_mem.pc) != branchPred.end()) {

                    
                //     char predicted = branchPred[ex_mem.pc].prediction;
                //     cout << "PREDICTED : " << predicted << endl;
                //     bool wasPredictedTaken = (predicted == 'T');
                //     cout << "WAS PREDICT TAKEN " << wasPredictedTaken << endl;
                //     cout << "EX_MEM BRANCH TAKEN " << ex_mem.branch_taken << endl;
                //     bool mispredicted = (wasPredictedTaken != ex_mem.branch_taken);
                //     cout << "MISPREDICTED " << mispredicted << endl;

                //     if (mispredicted) {
                //         stat_branch_misp++;
                //     }
                //     cout << "STAT BRANCH MISPREDICTION " << stat_branch_misp << endl; 
                //     ex_mem.misprediction_checked = true;
                // }

                if (ex_mem.next_pc != if_id.pc) {
                    logs << "Flushing pipeline due to control hazard" << endl;
                    if_id.valid = false;
                    id_ex.valid = false;
                    PC = ex_mem.next_pc;
                    
                    // if(ex_mem.branch_taken){
                    //     stat_stalls++;
                    // }
                    
                }

                logs << "Control hazard detected, flushing pipeline" << endl;
                logs << "PC is: " << dec << PC << endl;
                logs << "----------------------------------------" << endl << endl;
            }

            pipeline_decode();
            pipeline_fetch();
            stat_cycles++;

            logs << "No of cycles: " << dec << stat_cycles << endl;
            logs << "----------------------------------------" << endl;

            if (knob_print_regs) {
                cout << "Registers after cycle " << dec << stat_cycles << ":\n";
                cout << endl;
                for (int i = 0; i < 32; ++i) {
                    cout << "R[" << i << "] = 0x" << hex << REG[i] << "\n";
                }
                cout << endl;
            }

            if (knob_print_pipeline || (knob_trace && !knob_print_pipeline)) {
                bool traceInPipeline = false;
                if (knob_trace && !knob_print_pipeline) {
                    if ((if_id.valid && if_id.inst_num == trace_inst_num) ||
                        (id_ex.valid && id_ex.inst_num == trace_inst_num) ||
                        (ex_mem.valid && ex_mem.inst_num == trace_inst_num) ||
                        (mem_wb.valid && mem_wb.inst_num == trace_inst_num)) {
                        traceInPipeline = true;
                    }
                }
                if (knob_print_pipeline || traceInPipeline) {
                    cout << "Cycle " << dec << stat_cycles << ":\n";
                    cout << "IF/ID: ";
                    if (if_id.valid) {
                        cout << "[PC=0x" << hex << if_id.pc << ", IR=0x" << hex << if_id.ir << ", inst#" << dec << if_id.inst_num << "]";
                    } else {
                        cout << "[empty]";
                    }
                    cout << "\nID/EX: ";
                    if (id_ex.valid) {
                        cout << "[PC=0x" << hex << id_ex.pc << ", opcode=0x" << hex << (unsigned)id_ex.opcode
                             << ", rs1=" << dec << id_ex.rs1 << ", rs2=" << id_ex.rs2
                             << ", rd=" << id_ex.rd << ", imm=0x" << hex << id_ex.imm
                             << ", inst#" << dec << id_ex.inst_num << "]";
                    } else {
                        cout << "[empty]";
                    }
                    cout << "\nEX/MEM: ";
                    if (ex_mem.valid) {
                        cout << "[PC=0x" << hex << ex_mem.pc << ", ALU=0x" << ex_mem.alu_result
                             << ", rd=" << dec << ex_mem.rd << ", we=" << ex_mem.write_enable
                             << ", mr=" << ex_mem.mem_read << ", mw=" << ex_mem.mem_write
                             << ", inst#" << ex_mem.inst_num << "]";
                    } else {
                        cout << "[empty]";
                    }
                    cout << "\nMEM/WB: ";
                    if (mem_wb.valid) {
                        cout << "[PC=0x" << hex << mem_wb.pc << ", value=0x" << mem_wb.write_value
                             << ", rd=" << dec << mem_wb.rd << ", rw=" << mem_wb.reg_write
                             << ", inst#" << mem_wb.inst_num << "]";
                    } else {
                        cout << "[empty]";
                    }
                    cout << "\n";
                }
            }

            if (knob_print_branch) {
                cout << "Branch Predictor State (end of cycle " << dec << stat_cycles << "):\n";
                for (auto &entry : branchPred) {
                    uint32_t bpc = entry.first;
                    char pred = entry.second.prediction;
                    cout << "PC 0x" << hex << bpc << ": Pred=" << pred;
                    if (pred == 'T') {
                        cout << ", Target=0x" << hex << entry.second.target;
                    }
                    cout << "\n";
                    cout << endl;
                }
            }

            if (mem_wb.is_ecall) {
                if (!if_id.valid && !id_ex.valid && !ex_mem.valid && !mem_wb.valid) {
                    break;
                }
            }
            if (stop_fetch && !if_id.valid && !id_ex.valid && !ex_mem.valid && !mem_wb.valid) {
                break;
            }
        }
    }

    // CPI Calculation
    double cpi = (stat_instructions == 0) ? 0.0 : (double)stat_cycles / stat_instructions;

    // Output stats
    ofstream stats("stats.txt");
    stats << "Number of clock cycles: " << stat_cycles << "\n"; //checked
    stats << "Number of instructions executed: " << stat_instructions << "\n"; //checked
    stats << "Cycles per instruction (CPI): " << fixed << setprecision(2) << cpi << "\n"; ////checked
    stats << "Number of load/store instructions: " << stat_load_store << "\n"; //checked
    stats << "Number of ALU instructions: " << stat_alu << "\n"; //checked
    stats << "Number of control instructions: " << stat_control << "\n"; //checked
    stats << "Number of stalls: " << stat_stalls << "\n"; //checked
    stats << "Number of data hazards: " << stat_data_hazards << "\n"; //checked
    stats << "Number of control hazards: " << stat_ctrl_hazards << "\n"; //checked
    // stats << "Number of branch predictions: " << stat_branch_predictions << "\n";
    stats << "Number of branch mispredictions: " << stat_branch_misp << "\n"; //checked
    stats << "Number of stalls due to data hazards: " << stat_stall_data << "\n"; //checked
    stats << "Number of stalls due to control hazards: " << stat_stall_control << "\n"; //checked
    stats.close();

    simulator.save_final_state();
    return 0;
}