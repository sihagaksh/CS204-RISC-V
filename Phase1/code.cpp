#include<bits/stdc++.h>
using namespace std;

//struct to hold the instruction details
struct Instruction{
    string func3;
    string func7;
    string opcode;
    string type;
}

//class RiscVAssembler to be worked on
class RiscVAssembler{
private:
    //map to hold the register names with their binary values
    unordered_map<string,string> registerMap;

    //map to hold different types of instructions
    unordered_map<string,Instruction> instructionMap; 

public:

}


int main(){
    RiscVAssembler assembler;

    ifstream inputFile("input.asm");
    ofstream outputFile("output.mc");

    return 0;
}