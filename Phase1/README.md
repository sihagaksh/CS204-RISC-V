# RISC-V ASSEMBLER

The **Phase-I** of RISC-V ASSEMBLER is a C++ program designed to convert RISC-V code into machine code. It reads an assembly program from `input.asm` and generate the corresponding machine code in `output.mc`, following the standard RISC-V format.

---

## Team Details
- **[Aadit Mahajan]**
- **[Aksh Sihag]**
- **[Rahul Goyal]**

---

## Table of Contents
- [Installation and Compilation](#installation-and-compilation)
- [Usage Instructions](#usage-instructions)
- [Supported Instructions](#supported-instructions)
- [Assembler Directives](#assembler-directives)
- [Label definition rule](#label-definition-rule)

## Installation and Compiation

### Compiling the code
To compile the source code, use the following command in your terminal:
```bash
g++ code.cpp -o code
```
Run the executable with the following command:
```bash
./code input.asm
```

## Usage Instructions

1. **Prepare Assembly Code**
    - Write RISC-V assembly instruction in `input.asm`.
2. **Run Assembly Code**
    - Execute the program to convert assembly to machine code.
3. **View Output**
    - The machine code is saved in `output.mc`.

## Supported Instructions

### R-Format Instructions
```bash
add, and, or, sll, slt, sra, srl, sub, xor, mul, div, rem
```

### I-Format Instructions
```bash
addi, andi, ori, lb, ld, lh, lw, jalr
```

### S-Format Instructions
```bash
sb, sw, sd, sh
```

### SB-Format Instructions
```bash
beq, bne, bge, blt
```

### U-Format Instructions
```bash
auipc, lui
```

### UJ-Format Instructions
```bash
jal
```

## Assembler Directives
The assembler supports the following directives:
- `.text` - Defines the code section.
- `.data` - Defines the data section.
- `.byte .half .word .double` - Declares memory storage.
- `.asciiz` - Stores null terminated strings.

## Label definition rule
While defining a **label**, ensure that the corresponding instructions or data appear on the next line.
This is necessary for correct parsing by the assembler.

### Correct Way:
```bash
array:
.word 10 20

loop:
    add x1, x2, x3
```

### Wrong Way:
```bash
var: .word 10 20

loop: add x1, x2, x3
```
