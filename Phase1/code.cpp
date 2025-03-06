#include<bits/stdc++.h>
using namespace std;

//struct to hold the instruction details
struct Instruction{
    string opcode;
    string func3;
    string func7;
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

    //function for generating R-type instructions
    string generateRType(const Instruction &instr, const string &rd, const string &rs1, const string &rs2){
        return instr.func7 + registerMap[rs2] + registerMap[rs1] + instr.func3 + registerMap[rd] + instr.opcode;
    };

    //function for generating I-type instructions
    string generateIType(const Instruction &instr, const string &rd, const string &rs1, const int immediate){
        //if immediate is out of bound then return error
        if(immediate < -2048 || immediate > 2047){
            return "Immediate out of bound";
        }
        return bitset<12>(immediate).to_string() + registerMap[rs1] + instr.func3 + registerMap[rd] + instr.opcode;
    };

    //function for generating S-type instructions
    string generateSType(const Instruction &instr, const string &rs1, const string &rs2, const int offset){
        //if offset is out of bound then return error
        if(offset < -2048 || offset > 2047){
            return "Offset out of bound";
        }
        string offsetStr = bitset<12>(offset).to_string();
        return offsetStr.substr(0, 7) + registerMap[rs2] + registerMap[rs1] + instr.func3 + offsetStr.substr(7,5) + instr.opcode;
    };
    string generateSBType(const Instruction &instr, const string &rs1, const string &rs2, const int offset){
        //to be implemented
    };
    string generateUType(const Instruction &instr, const string &rd, const int immediate){
        //if immediate is out of bound then return error
        //immediate's lower bound is set to 0 because -ve mem address do not exist
        if(immediate < 0 || immediate > 1048575){
            return "Immediate out of bound";
        }
        return bitset<20>(immediate).to_string() + registerMap[rd] + instr.opcode;
    };
    string generateUJType(const Instruction &instr, const string &rd, const int offset){
        //to be implemented
    };

public:

    string parseLine(const string &line){
        stringstream ss(line);
        string instruction;
        string rd;
        string rs1;
        string rs2;
        int immediate;

        ss >> instruction;
        if(instructionMap.find(instruction) == instructionMap.end()){
            return "Invalid Instruction";
        }
        const Instruction &instr = instructionMap[instruction];

        if (instr.type == "R") {
            ss >> rd >> rs1 >> rs2;
            rd.pop_back();
            rs1.pop_back();
            return generateRType(instr, rd, rs1, rs2);
        } 
        else if (instr.type == "I") {
            ss >> rd >> rs1 >> immediate;
            rd.pop_back();
            rs1.pop_back();
            return generateIType(instr, rd, rs1, immediate);
        } 
        else if (instr.type == "S") {
            ss >> rs1 >> rs2 >> immediate;
            rs1.pop_back();
            rs2.pop_back();
            return generateSType(instr, rs1, rs2, immediate);
        } 
        else if (instr.type == "SB") {
            //to be implemented
        } 
        else if (instr.type == "U") {
            ss >> rd >> immediate;
            rd.pop_back();
            return generateUType(instr, rd, immediate);
        } 
        else if (instr.type == "UJ") {
            //to be implemented
        } 
        else {
            return "Invalid Instruction";
        }        

        return "Something went wrong";
    }

    void parseFile(ifstream &inputFile, ofstream &outputFile){
        vector<string> lines;
        string line;
        while(getline(inputFile, line)){
            stringstream ss(line);
            lines.push_back(line);
        }
        for(const auto &line:lines){
            string machineCode = parseLine(line);
            outputFile<<machineCode<<endl;
        }
    }
};

int main(){
    RiscVAssembler assembler;

    ifstream inputFile("input.asm");
    ofstream outputFile("output.mc");

    if(!inputFile){
        cout<<"Error in opening the input file"<<endl;
        return 0;
    }
    if(!outputFile){
        cout<<"Error in opening the output file"<<endl;
        return 0;
    }
    
    assembler.parseFile(inputFile, outputFile);

    inputFile.close();
    outputFile.close();
    return 0;
}