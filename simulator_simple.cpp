#include <iostream>
#include <vector>
#include <stdint.h>
#include <string>
#include <sstream>
#include <iomanip>

using namespace std;

// RISC-V Register indices
const int NUM_REGISTERS = 32;
const uint32_t PC_INIT = 0x00000000;

// Simple RISC-V Simulator class
class RISCVSimulator {
private:
    // Registers
    uint32_t registers[NUM_REGISTERS];
    uint32_t pc;
    
    // Memory (simplified - using vector)
    vector<uint8_t> memory;
    
    // Statistics
    uint32_t cycle_count;
    uint32_t instruction_count;
    
public:
    RISCVSimulator() : pc(PC_INIT), cycle_count(0), instruction_count(0) {
        // Initialize registers to 0
        for (int i = 0; i < NUM_REGISTERS; i++) {
            registers[i] = 0;
        }
        registers[2] = 0x7FFFFFFF; // Stack pointer initial value
        
        // Initialize memory (64KB)
        memory.resize(64 * 1024, 0);
    }
    
    // Load program from input
    void loadProgram() {
        string line;
        uint32_t address = PC_INIT;
        
        while (getline(cin, line)) {
            // Parse hex instruction
            uint32_t instruction;
            stringstream ss;
            ss << hex << line;
            ss >> instruction;
            
            // Store in memory (little-endian)
            memory[address] = instruction & 0xFF;
            memory[address + 1] = (instruction >> 8) & 0xFF;
            memory[address + 2] = (instruction >> 16) & 0xFF;
            memory[address + 3] = (instruction >> 24) & 0xFF;
            
            address += 4;
        }
    }
    
    // Fetch instruction from memory
    uint32_t fetchInstruction(uint32_t addr) {
        if (addr + 3 >= memory.size()) {
            return 0;
        }
        
        uint32_t instr = memory[addr] |
                        (memory[addr + 1] << 8) |
                        (memory[addr + 2] << 16) |
                        (memory[addr + 3] << 24);
        return instr;
    }
    
    // Execute a single instruction
    void executeInstruction(uint32_t instr) {
        uint32_t opcode = instr & 0x7F;
        uint32_t rd = (instr >> 7) & 0x1F;
        uint32_t funct3 = (instr >> 12) & 0x7;
        uint32_t rs1 = (instr >> 15) & 0x1F;
        uint32_t rs2 = (instr >> 20) & 0x1F;
        uint32_t funct7 = (instr >> 25) & 0x7F;
        
        int32_t imm = 0;
        
        switch (opcode) {
            case 0x33: // R-type
                switch (funct3) {
                    case 0x0: // add/sub
                        if (funct7 == 0x20) {
                            registers[rd] = (int32_t)registers[rs1] - (int32_t)registers[rs2]; // sub
                        } else {
                            registers[rd] = (int32_t)registers[rs1] + (int32_t)registers[rs2]; // add
                        }
                        break;
                    case 0x4: // xor
                        registers[rd] = registers[rs1] ^ registers[rs2];
                        break;
                    case 0x6: // or
                        registers[rd] = registers[rs1] | registers[rs2];
                        break;
                    case 0x7: // and
                        registers[rd] = registers[rs1] & registers[rs2];
                        break;
                }
                break;
                
            case 0x13: // I-type (arith/logic)
                imm = (instr >> 20) & 0xFFF;
                // Sign extend immediate
                if (imm & 0x800) {
                    imm |= 0xFFFFF000;
                }
                
                switch (funct3) {
                    case 0x0: // addi
                        registers[rd] = (int32_t)registers[rs1] + imm;
                        break;
                    case 0x4: // xori
                        registers[rd] = registers[rs1] ^ imm;
                        break;
                    case 0x6: // ori
                        registers[rd] = registers[rs1] | imm;
                        break;
                    case 0x7: // andi
                        registers[rd] = registers[rs1] & imm;
                        break;
                }
                break;
                
            case 0x03: // I-type (load)
                imm = (instr >> 20) & 0xFFF;
                // Sign extend immediate
                if (imm & 0x800) {
                    imm |= 0xFFFFF000;
                }
                
                {
                    uint32_t addr = registers[rs1] + imm;
                    if (addr < memory.size()) {
                        switch (funct3) {
                            case 0x0: // lb
                                registers[rd] = (int8_t)memory[addr];
                                break;
                            case 0x1: // lh
                                registers[rd] = (int16_t)(memory[addr] | (memory[addr + 1] << 8));
                                break;
                            case 0x2: // lw
                                registers[rd] = memory[addr] | 
                                              (memory[addr + 1] << 8) |
                                              (memory[addr + 2] << 16) |
                                              (memory[addr + 3] << 24);
                                break;
                            case 0x4: // lbu
                                registers[rd] = memory[addr];
                                break;
                            case 0x5: // lhu
                                registers[rd] = memory[addr] | (memory[addr + 1] << 8);
                                break;
                        }
                    }
                }
                break;
                
            case 0x23: // S-type
                imm = ((instr >> 7) & 0x1F) | ((instr >> 25) << 5);
                // Sign extend immediate
                if (imm & 0x800) {
                    imm |= 0xFFFFF000;
                }
                
                {
                    uint32_t addr = registers[rs1] + imm;
                    uint32_t data = registers[rs2];
                    if (addr < memory.size()) {
                        switch (funct3) {
                            case 0x0: // sb
                                memory[addr] = data & 0xFF;
                                break;
                            case 0x1: // sh
                                memory[addr] = data & 0xFF;
                                memory[addr + 1] = (data >> 8) & 0xFF;
                                break;
                            case 0x2: // sw
                                memory[addr] = data & 0xFF;
                                memory[addr + 1] = (data >> 8) & 0xFF;
                                memory[addr + 2] = (data >> 16) & 0xFF;
                                memory[addr + 3] = (data >> 24) & 0xFF;
                                break;
                        }
                    }
                }
                break;
                
            case 0x37: // lui
                imm = instr & 0xFFFFF000;
                registers[rd] = imm;
                break;
                
            case 0x17: // auipc
                imm = instr & 0xFFFFF000;
                registers[rd] = pc + imm;
                break;
                
            case 0x6F: // jal
                imm = ((instr >> 21) & 0x3FF) | ((instr >> 10) & 0x1) << 10 |
                      ((instr >> 1) & 0xFFF) << 11 | ((instr >> 31) & 0x1) << 20;
                // Sign extend immediate
                if (imm & 0x100000) {
                    imm |= 0xFFE00000;
                }
                registers[rd] = pc + 4;
                pc = pc + imm;
                break;
                
            case 0x67: // jalr
                imm = (instr >> 20) & 0xFFF;
                // Sign extend immediate
                if (imm & 0x800) {
                    imm |= 0xFFFFF000;
                }
                registers[rd] = pc + 4;
                pc = (registers[rs1] + imm) & ~1;
                break;
                
            case 0x63: // B-type
                imm = ((instr >> 8) & 0xF) | ((instr >> 25) << 4) | 
                      (((instr >> 7) & 0x1) << 11) | (((instr >> 31) & 0x1) << 12);
                // Sign extend immediate
                if (imm & 0x1000) {
                    imm |= 0xFFFFE000;
                }
                
                {
                    bool branch_taken = false;
                    uint32_t rs1_val = registers[rs1];
                    uint32_t rs2_val = registers[rs2];
                    
                    switch (funct3) {
                        case 0x0: // beq
                            branch_taken = (rs1_val == rs2_val);
                            break;
                        case 0x1: // bne
                            branch_taken = (rs1_val != rs2_val);
                            break;
                        case 0x4: // blt
                            branch_taken = ((int32_t)rs1_val < (int32_t)rs2_val);
                            break;
                        case 0x5: // bge
                            branch_taken = ((int32_t)rs1_val >= (int32_t)rs2_val);
                            break;
                        case 0x6: // bltu
                            branch_taken = (rs1_val < rs2_val);
                            break;
                        case 0x7: // bgeu
                            branch_taken = (rs1_val >= rs2_val);
                            break;
                    }
                    
                    if (branch_taken) {
                        pc = pc + imm;
                    }
                }
                break;
        }
        
        // x0 should always be 0
        registers[0] = 0;
        
        instruction_count++;
    }
    
    // Run simulation
    void run() {
        // Simple simulation - run for a fixed number of instructions
        // In a real implementation, we'd run until program completion
        
        for (int i = 0; i < 10000; i++) {
            if (pc >= memory.size()) {
                break;
            }
            
            uint32_t instr = fetchInstruction(pc);
            if (instr == 0) {
                break; // End of program
            }
            
            executeInstruction(instr);
            
            // Increment PC unless it was changed by jump/branch
            if ((instr & 0x7F) != 0x6F && (instr & 0x7F) != 0x67 && (instr & 0x7F) != 0x63) {
                pc += 4;
            }
            
            cycle_count++;
        }
    }
    
    // Get result for output (as specified in problem)
    uint32_t getResult() {
        // Return the value in register x10 (a0) as the result
        // This is a common convention for function return values
        return registers[10];
    }
};

int main() {
    RISCVSimulator simulator;
    
    // Load program from standard input
    simulator.loadProgram();
    
    // Run simulation
    simulator.run();
    
    // Output result
    uint32_t result = simulator.getResult();
    cout << result << endl;
    
    return 0;
}