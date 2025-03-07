#include<bits/stdc++.h>
using namespace std;

//variables for address of instructions in .text and variables in .data respectively
long long programCounter = 0x00000000; //starting address of .text section
long long dataAddress = 0x10000000; //starting address of .data section

//boolean for checking if we are in .data section
bool dataMode = false;

//maintaining the current program counter which will help in the calculation of offset for branch instructions
long long currentPC= 0x0000000;

//Maps for storing address corresponding to labels
unordered_map <string , long long> labels;

//map for storing memory addresses and the values stored at them
unordered_map <string , long long> dataLabels;

//vector to store all the variable of data section
vector <pair <long long, string>> dataSegment;

//struct to hold the instruction details
struct Instruction{
    string opcode;
    string func3;
    string func7;
    string type;
};

string binaryToHex(string binCode){
    string hexCode = "0x";
    for(int i=0; i<binCode.size(); i+=4){
        string hexDigit = binCode.substr(i, 4);
        if(hexDigit == "0000") hexCode += "0";
        else if(hexDigit == "0001") hexCode += "1";
        else if(hexDigit == "0010") hexCode += "2";
        else if(hexDigit == "0011") hexCode += "3";
        else if(hexDigit == "0100") hexCode += "4";
        else if(hexDigit == "0101") hexCode += "5";
        else if(hexDigit == "0110") hexCode += "6";
        else if(hexDigit == "0111") hexCode += "7";
        else if(hexDigit == "1000") hexCode += "8";
        else if(hexDigit == "1001") hexCode += "9";
        else if(hexDigit == "1010") hexCode += "A";
        else if(hexDigit == "1011") hexCode += "B";
        else if(hexDigit == "1100") hexCode += "C";
        else if(hexDigit == "1101") hexCode += "D";
        else if(hexDigit == "1110") hexCode += "E";
        else if(hexDigit == "1111") hexCode += "F";
    }
    return hexCode;

}

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
    string generateIType(const Instruction &instr, const string &rd, const string &rs1, const long long immediate){
        //if immediate is out of bound then return error
        if(immediate < -2048 || immediate > 2047){
            return "Immediate out of bound";
        }
        return bitset<12>(immediate).to_string() + registerMap[rs1] + instr.func3 + registerMap[rd] + instr.opcode;
    };

    //function for generating S-type instructions
    string generateSType(const Instruction &instr, const string &rs1, const string &rs2, const long long offset){
        //if offset is out of bound then return error
        if(offset < -2048 || offset > 2047){
            return "Offset out of bound";
        }
        string offsetStr = bitset<12>(offset).to_string();
        return offsetStr.substr(0, 7) + registerMap[rs2] + registerMap[rs1] + instr.func3 + offsetStr.substr(7,5) + instr.opcode;
    };
    string generateSBType(const Instruction &instr, const string &rs1, const string &rs2, const long long offset){
        string imm = bitset<13>(offset).to_string();
        return imm[0] + imm.substr(2,6) + registerMap[rs2] + registerMap[rs1] + instr.func3 + imm.substr(8,4) + imm[1] + instr.opcode; 
    };
    string generateUType(const Instruction &instr, const string &rd, const long long immediate){
        //if immediate is out of bound then return error
        //immediate's lower bound is set to 0 because -ve mem address do not exist
        if(immediate < 0 || immediate > 1048575){
            return "Immediate out of bound";
        }
        return bitset<20>(immediate).to_string() + registerMap[rd] + instr.opcode;
    };
    string generateUJType(const Instruction &instr, const string &rd, const long long offset){
        string imm = bitset<21>(offset).to_string();
        return imm[0] + imm.substr(10,10) + imm[9] + imm.substr(1,8) + registerMap[rd] + instr.opcode;
    };

public:

    string parseLine(const string &line){
        stringstream ss(line);
        string instruction;
        string rd;
        string rs1;
        string rs2;
        long long immediate=0;

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
            string offsetS;
            ss >> rd >> offsetS >> rs1;
            if(rs1.size()==0){
                immediate = stoi(offsetS.substr(0, offsetS.find('(')));
                rs1 = offsetS.substr(offsetS.find('(')+1);
                rs1.pop_back();
            }
            else{
                offsetS.pop_back();
                immediate = stoi(offsetS);
            }
            rd.pop_back();
            return generateIType(instr, rd, rs1, immediate);
        } 
        else if (instr.type == "S") {
            string offsetS;
            ss >> rs2 >> offsetS >> rs1;
            if(rs1.size()==0){
                immediate = stoi(offsetS.substr(0, offsetS.find('(')));
                rs1 = offsetS.substr(offsetS.find('(')+1);
                rs1.pop_back();
            }
            else{
                offsetS.pop_back();
                immediate = stoi(offsetS);
            }
            rs2.pop_back();
            return generateSType(instr, rs1, rs2, immediate);
        } 
        else if (instr.type == "SB") {
            string label;
            ss>>rs1>>rs2>>label;
            if(labels.find(label)==labels.end()){
                return "Label not found";
            }
            rs1.pop_back();
            rs2.pop_back();
            long long offset = (labels[label] - currentPC);
            return generateSBType(instr, rs1, rs2, offset);
        } 
        else if (instr.type == "U") {
            ss >> rd >> immediate;
            rd.pop_back();
            return generateUType(instr, rd, immediate);
        } 
        else if (instr.type == "UJ") {
            string label;
            ss>>rd>>label;
            if(labels.find(label)==labels.end()){
                return "Label not found";
            }
            rd.pop_back();
            long long offset = (labels[label] - currentPC);
            return generateUJType(instr, rd,offset);
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
            if (line == ".data"){
                dataMode = true;
                continue;
            }
            if (line == ".text"){
                dataMode = false;
                continue;
            }

            stringstream ss(line);
            string word;
            ss>>word;
            if (word.back() == ':'){
                word.pop_back();
                if (dataMode){
                    dataLabels[word] = dataAddress;
                }
                else{
                    labels[word] = programCounter;
                }
                continue;
            }

            if (dataMode){
                if (word == ".word"){
                    long long val;
                    while(ss>>val){
                        dataSegment.push_back({dataAddress, to_string(val)});
                        dataAddress += 4;
                    }   
                }
                else if (word == ".byte"){
                    string token;
                    while (ss >> token) {
                    long long val;
                    if (token[0] == '\'' && token.size() == 3 && token[2] == '\'') { 
                        // Character case
                        val = token[1];
                    } else {
                        try {
                            val = stoi(token);
                        } catch (...) {
                            cout << "Invalid .byte value: " << token << endl;
                            break;
                        }
                    }

                    if (val < -128 || val > 127) {
                        cout << "Value out of bound for .byte: " << val << endl;
                        break;
                    }

                    dataSegment.push_back({dataAddress, to_string(val)});
                    dataAddress += 1;
                }   
                }
                else if (word == ".double"){
                    long long val;
                    ss>>val;
                    while(val){
                        dataSegment.push_back({dataAddress, to_string(val)});
                        dataAddress += 8;
                        ss>>val;
                    }   
                }
                else if (word == ".asciiz") {
                    string str;
                    getline(ss, str);
                    if (str.size() >= 2 && str[1] == '"' && str.back() == '"') {
                        for (int i=2;i<str.size()-1;i++) {
                            dataSegment.push_back({dataAddress, to_string(str[i])});
                            dataAddress++;
                        }
                        // Null terminator
                        dataSegment.push_back({dataAddress, "0"});
                        dataAddress++;
                    } else {
                        cout << "Invalid .asciiz format. Must be enclosed in double quotes." << endl;
                        continue;
                    }
                
                }
                
                continue;

            }

            //trim leading and trailing whitespaces
            line.erase(line.find_last_not_of("\n\r\t")+1);
            line.erase(0, line.find_first_not_of(" \n\r\t"));

            //if the line is neither empty nor a comment
            if(!line.empty() && line[0] != '#'){ 
                lines.push_back(line);  
                programCounter += 4;              
            }

        }
        //parse each line and write the machine code to the output file
        for(const auto &line:lines){
            string machineCode = parseLine(line);
            outputFile<<binaryToHex(bitset<32>(currentPC).to_string()) <<" " << binaryToHex(machineCode) <<" "<< line <<" #"<<machineCode<<endl;
            currentPC += 4;
        }
        outputFile<<endl;
        outputFile<<endl;
        outputFile<<"#Data Segment"<<endl;
        for(auto it:dataSegment){
            outputFile<<binaryToHex(bitset<32>(it.first).to_string()) <<" " << binaryToHex(bitset<32>(stoi(it.second)).to_string()) <<" "<< it.second <<endl;
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

    //print all the unoreded maps labels, dataLabels and dataSegment
    // cout<<"Labels: "<<endl;
    // for(auto it:labels){
    //     cout<<it.first<<" "<<it.second<<endl;
    // }
    // cout<<"Data Labels: "<<endl;
    // for(auto it:dataLabels){
    //     cout<<it.first<<" "<<it.second<<endl;
    // }
    // cout<<"Data Segment: "<<endl;
    // for(auto it:dataSegment){
    //     cout<<it.first<<" "<<(char)stoi(it.second)<<endl;
    // }
    inputFile.close();
    outputFile.close();
    return 0;
}