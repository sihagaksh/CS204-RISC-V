#include<bits/stdc++.h>
using namespace std;

//struct to hold the instruction details
struct Instruction{
    string func3;
    string func7;
    string opcode;
    string type;
};

//class RiscVAssembler to be worked on
class RiscVAssembler{
private:
    //map to hold the register names with their binary values
    unordered_map<string, string> registerMap = {
        {"x0", "00000"}, {"x1", "00001"}, {"x2", "00010"}, {"x3", "00011"},
        {"x4", "00100"}, {"x5", "00101"}, {"x6", "00110"}, {"x7", "00111"}, 
        {"x8", "01000"}, {"x9", "01001"}, {"x10", "01010"}, {"x11", "01011"}, 
        {"x12", "01100"}, {"x13", "01101"}, {"x14", "01110"}, {"x15", "01111"}, 
        {"x16", "10000"}, {"x17", "10001"}, {"x18", "10010"}, {"x19", "10011"}, 
        {"x20", "10100"}, {"x21", "10101"}, {"x22", "10110"}, {"x23", "10111"}, 
        {"x24", "11000"}, {"x25", "11001"}, {"x26", "11010"}, {"x27", "11011"}, 
        {"x28", "11100"}, {"x29", "11101"}, {"x30", "11110"}, {"x31", "11111"}
    };

    //map to hold different types of instructions
    unordered_map<string, Instruction> instructionMap = {
        // R-type instructions
        {"add", {"0110011", "000", "0000000", "R"}},
        {"and", {"0110011", "111", "0000000", "R"}},
        {"or", {"0110011", "110", "0000000", "R"}},
        {"sll", {"0110011", "001", "0000000", "R"}},
        {"slt", {"0110011", "010", "0000000", "R"}},
        {"sra", {"0110011", "101", "0100000", "R"}},
        {"srl", {"0110011", "101", "0000000", "R"}},
        {"sub", {"0110011", "000", "0100000", "R"}},
        {"xor", {"0110011", "100", "0000000", "R"}},
        {"mul", {"0110011", "000", "0000001", "R"}},
        {"div", {"0110011", "100", "0000001", "R"}},
        {"rem", {"0110011", "110", "0000001", "R"}},
    
        // I-type instructions
        {"addi", {"0010011", "000", "", "I"}},
        {"andi", {"0010011", "111", "", "I"}},
        {"ori", {"0010011", "110", "", "I"}},
        {"lb", {"0000011", "000", "", "I"}},
        {"ld", {"0000011", "011", "", "I"}},
        {"lh", {"0000011", "001", "", "I"}},
        {"lw", {"0000011", "010", "", "I"}},
        {"jalr", {"1100111", "000", "", "I"}},
    
        // S-type instructions (store)
        {"sb", {"0100011", "000", "", "S"}},
        {"sw", {"0100011", "010", "", "S"}},
        {"sd", {"0100011", "011", "", "S"}},
        {"sh", {"0100011", "001", "", "S"}},
    
        // SB-type instructions (branch)
        {"beq", {"1100011", "000", "", "SB"}},
        {"bne", {"1100011", "001", "", "SB"}},
        {"bge", {"1100011", "101", "", "SB"}},
        {"blt", {"1100011", "100", "", "SB"}},
    
        // U-type instructions
        {"auipc", {"0010111", "", "", "U"}},
        {"lui", {"0110111", "", "", "U"}},
    
        // UJ-type instructions
        {"jal", {"1101111", "", "", "UJ"}}
    };

public:

};

int main(){
    RiscVAssembler assembler;

    ifstream inputFile("input.asm");
    ofstream outputFile("output.mc");

    return 0;
}