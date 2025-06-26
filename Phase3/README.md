# RISC‑V Simulator – Phase 3 (Pipelined)

## Overview
This repository contains **Phase 3** of our RISC‑V tool‑chain: a cycle‑accurate,
five‑stage, in‑order pipeline simulator written in modern C++.  
Phase 1 (assembler) produces a machine‑code listing `input.mc`; Phase 3 consumes
that file, runs it through an **IF‑ID‑EX‑MEM‑WB** pipeline, and emits:

* `output.mc` – final register & data‑memory state  
* `stats.txt` – detailed performance counters  
* `logs.txt` - detailed logs of each stage
* **optional** GUI traces (see below)

---

## Team
| Name | Roll No. |
|------|----------|
| Aadit Mahajan | 2023CSB1091 |
| Aksh Sihag    | 2023CSB1097 |
| Rahul Goyal   | 2023CSB1150 |

---

## Table of Contents
1. [Build & Run](#build--run)  
2. [Command‑line Knobs](#command‑line-knobs)  
3. [Pipeline Micro‑Architecture](#pipeline-micro-architecture)  
4. [Instruction Set Coverage](#instruction-set-coverage)  
5. [Memory Map & Registers](#memory-map--registers)  
6. [Output Files](#output-files)

---

## Build & Run

### 1. C++ SIMULATOR

```bash
# compile
g++ code.cpp -o code

# run
./code --pipeline --forwarding --print-pipeline --input input.mc
```

### 2. GUI SIMULATION

```bash
cd gui_simulator
npm install          # first time only
npm run dev          # http://localhost:5173
```

---
## Command‑line Knobs

| Flag | Effect |
|------|--------|
| `--pipeline / --no-pipeline` | enable/disable pipelined mode |
| `--forwarding / --no-forwarding` | enable data‑forwarding |
| `--print-registers` | dump register file each cycle |
| `--print-pipeline` | dump IF/ID/… pipeline registers |
| `--print-branch` | show 1‑bit branch predictor table |
| `--input <file>` | choose an alternate `input.mc` |
---

## Pipeline Micro‑Architecture

| Stage | Work |
|-------|------|
| **IF** | Fetch 32‑bit instruction, consult 1‑bit BPU + BTB |
| **ID** | Decode fields, hazard detection, generate control |
| **EX** | ALU / branch comparison / effective‑address |
| **MEM**| Data‑memory access, update BPU on branch resolve |
| **WB** | Write register file |

Features:

* Data forwarding (EX/MEM→ID & MEM/WB→ID)  
* Load‑use stall insertion  
* 1‑bit dynamic branch predictor with 16‑entry BTB  
* Precise pipeline flush on mis‑prediction or JAL/ JALR  
* Statistics counters for CPI, hazards, stalls & mis‑predictions

---

## Instruction Set Coverage

### R‑type  
`add` `sub` `and` `or` `xor` `sll` `srl` `sra` `slt` `mul` `div` `rem`

### I‑type  
`addi` `andi` `ori` `lb` `lh` `lw` `jalr`

### S‑type  
`sb` `sh` `sw`

### SB‑type  
`beq` `bne` `blt` `bge`

### U‑type  
`lui` `auipc`

### UJ‑type  
`jal`

---

## Memory Map & Registers

* **Register file** – 32 × 32‑bit, `x0` is hard‑wired to 0  
* **Text segment** – instructions, loaded 1 word per line from `input.mc`  
* **Data segment** – byte‑addressable, base `0x10000000`  
* **Stack** – grows down from `0x7FFFFFDC`, register `x2` (sp)

Only the addresses actually touched by the program are dumped to `output.mc`
to keep the file small.

---

## Output Files

| File | Description |
|------|-------------|
| `output.mc` | Final state of **registers** and **modified bytes** of data memory |
| `stats.txt` | 12 mandatory stats + bonus counters (CPI, hazard breakdown, etc.) |
| `cycle_snapshots.log` | Per‑cycle pipeline snapshot (if `--save-snapshots`) |
| `register.mem`, `D_Memory.mem`, `stack_mem.mem` | Raw dumps consumed by the GUI |

---

## Example session

```
$ ./CODE --pipeline --forwarding --input fib.mc
Cycles :  119
Instructions  :  53
CPI     :  2.25
B‑misp  :  3 (1‑bit predictor, 16‑entry BTB)
```

The GUI will highlight stalls, forwarding paths, branch flushes, and step
through the pipeline visually.

---

## Contributing / Issues

Feel free to open issues or pull‑requests for bug‑fixes, new ISA extensions, or
performance optimisations.  
For any question write to **any of the team members** or file an issue on
GitHub.
