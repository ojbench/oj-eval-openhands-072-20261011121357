#include <iostream>
#include <vector>
#include <cstdint>
#include <string>
#include <sstream>
#include <iomanip>
#include <map>
#include <queue>

using namespace std;

// RISC-V Register indices
const int NUM_REGISTERS = 32;
const uint32_t PC_INIT = 0x00000000;

// Instruction types
enum class InstructionType {
    R_TYPE, I_TYPE, S_TYPE, B_TYPE, U_TYPE, J_TYPE, UNKNOWN
};

// Pipeline stages
enum class PipelineStage {
    IF, ID, EX, MEM, WB
};

// Instruction structure
struct Instruction {
    uint32_t raw;
    InstructionType type;
    uint32_t opcode;
    uint32_t rd, rs1, rs2;
    uint32_t funct3, funct7;
    int32_t imm;
    uint32_t address;
    
    Instruction() : raw(0), type(InstructionType::UNKNOWN), opcode(0), 
                   rd(0), rs1(0), rs2(0), funct3(0), funct7(0), imm(0), address(0) {}
};

// Pipeline register structure
struct PipelineRegister {
    Instruction instr;
    uint32_t pc;
    bool valid;
    bool stall;
    
    PipelineRegister() : pc(0), valid(false), stall(false) {}
};

// RISC-V Simulator class
class RISCVSimulator {
private:
    // Registers
    uint32_t registers[NUM_REGISTERS];
    uint32_t pc;
    
    // Memory (simplified - using vector)
    vector<uint8_t> memory;
    
    // Pipeline registers
    PipelineRegister if_id;
    PipelineRegister id_ex;
    PipelineRegister ex_mem;
    PipelineRegister mem_wb;
    
    // Control signals
    bool reg_write;
    bool mem_read;
    bool mem_write;
    bool mem_to_reg;
    bool alu_src;
    uint32_t alu_op;
    bool branch;
    
    // Statistics
    uint32_t cycle_count;
    uint32_t instruction_count;
    
    // Hazard detection
    bool hazard_detected;
    
public:
    RISCVSimulator() : pc(PC_INIT), cycle_count(0), instruction_count(0), hazard_detected(false) {
        // Initialize registers to 0
        for (int i = 0; i < NUM_REGISTERS; i++) {
            registers[i] = 0;
        }
        registers[2] = 0x7FFFFFFF; // Stack pointer initial value
        
        // Initialize memory (64KB)
        memory.resize(64 * 1024, 0);
        
        // Initialize control signals
        reg_write = false;
        mem_read = false;
        mem_write = false;
        mem_to_reg = false;
        alu_src = false;
        alu_op = 0;
        branch = false;
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
    
    // Decode instruction
    Instruction decodeInstruction(uint32_t raw_instr, uint32_t addr) {
        Instruction instr;
        instr.raw = raw_instr;
        instr.address = addr;
        instr.opcode = raw_instr & 0x7F;
        
        // Determine instruction type based on opcode
        switch (instr.opcode) {
            case 0x33: // R-type
                instr.type = InstructionType::R_TYPE;
                instr.rd = (raw_instr >> 7) & 0x1F;
                instr.funct3 = (raw_instr >> 12) & 0x7;
                instr.rs1 = (raw_instr >> 15) & 0x1F;
                instr.rs2 = (raw_instr >> 20) & 0x1F;
                instr.funct7 = (raw_instr >> 25) & 0x7F;
                break;
                
            case 0x13: // I-type (arith/logic)
            case 0x03: // I-type (load)
            case 0x67: // I-type (jalr)
                instr.type = InstructionType::I_TYPE;
                instr.rd = (raw_instr >> 7) & 0x1F;
                instr.funct3 = (raw_instr >> 12) & 0x7;
                instr.rs1 = (raw_instr >> 15) & 0x1F;
                instr.imm = (raw_instr >> 20) & 0xFFF;
                // Sign extend immediate
                if (instr.imm & 0x800) {
                    instr.imm |= 0xFFFFF000;
                }
                break;
                
            case 0x23: // S-type
                instr.type = InstructionType::S_TYPE;
                instr.imm = ((raw_instr >> 7) & 0x1F) | ((raw_instr >> 25) << 5);
                // Sign extend immediate
                if (instr.imm & 0x800) {
                    instr.imm |= 0xFFFFF000;
                }
                instr.funct3 = (raw_instr >> 12) & 0x7;
                instr.rs1 = (raw_instr >> 15) & 0x1F;
                instr.rs2 = (raw_instr >> 20) & 0x1F;
                break;
                
            case 0x63: // B-type
                instr.type = InstructionType::B_TYPE;
                instr.imm = ((raw_instr >> 8) & 0xF) | ((raw_instr >> 25) << 4) | 
                          (((raw_instr >> 7) & 0x1) << 11) | (((raw_instr >> 31) & 0x1) << 12);
                // Sign extend immediate
                if (instr.imm & 0x1000) {
                    instr.imm |= 0xFFFFE000;
                }
                instr.funct3 = (raw_instr >> 12) & 0x7;
                instr.rs1 = (raw_instr >> 15) & 0x1F;
                instr.rs2 = (raw_instr >> 20) & 0x1F;
                break;
                
            case 0x37: // U-type (lui)
            case 0x17: // U-type (auipc)
                instr.type = InstructionType::U_TYPE;
                instr.rd = (raw_instr >> 7) & 0x1F;
                instr.imm = raw_instr & 0xFFFFF000;
                break;
                
            case 0x6F: // J-type (jal)
                instr.type = InstructionType::J_TYPE;
                instr.rd = (raw_instr >> 7) & 0x1F;
                instr.imm = ((raw_instr >> 21) & 0x3FF) | ((raw_instr >> 10) & 0x1) << 10 |
                          ((raw_instr >> 1) & 0xFFF) << 11 | ((raw_instr >> 31) & 0x1) << 20;
                // Sign extend immediate
                if (instr.imm & 0x100000) {
                    instr.imm |= 0xFFE00000;
                }
                break;
                
            default:
                instr.type = InstructionType::UNKNOWN;
                break;
        }
        
        return instr;
    }
    
    // Execute ALU operation
    uint32_t executeALU(uint32_t op1, uint32_t op2, uint32_t funct3, uint32_t funct7, uint32_t opcode) {
        switch (opcode) {
            case 0x33: // R-type
                switch (funct3) {
                    case 0x0: // add/sub
                        if (funct7 == 0x20) {
                            return (int32_t)op1 - (int32_t)op2; // sub
                        } else {
                            return (int32_t)op1 + (int32_t)op2; // add
                        }
                    case 0x1: // sll
                        return op1 << (op2 & 0x1F);
                    case 0x2: // slt
                        return ((int32_t)op1 < (int32_t)op2) ? 1 : 0;
                    case 0x3: // sltu
                        return (op1 < op2) ? 1 : 0;
                    case 0x4: // xor
                        return op1 ^ op2;
                    case 0x5: // srl/sra
                        if (funct7 == 0x20) {
                            return (int32_t)op1 >> (op2 & 0x1F); // sra
                        } else {
                            return op1 >> (op2 & 0x1F); // srl
                        }
                    case 0x6: // or
                        return op1 | op2;
                    case 0x7: // and
                        return op1 & op2;
                    default:
                        return 0;
                }
                
            case 0x13: // I-type
                switch (funct3) {
                    case 0x0: // addi
                        return (int32_t)op1 + (int32_t)op2;
                    case 0x1: // slli
                        return op1 << (op2 & 0x1F);
                    case 0x2: // slti
                        return ((int32_t)op1 < (int32_t)op2) ? 1 : 0;
                    case 0x3: // sltiu
                        return (op1 < op2) ? 1 : 0;
                    case 0x4: // xori
                        return op1 ^ op2;
                    case 0x5: // srli/srai
                        if (funct7 == 0x20) {
                            return (int32_t)op1 >> (op2 & 0x1F); // srai
                        } else {
                            return op1 >> (op2 & 0x1F); // srli
                        }
                    case 0x6: // ori
                        return op1 | op2;
                    case 0x7: // andi
                        return op1 & op2;
                    default:
                        return 0;
                }
                
            case 0x03: // Load address calculation
                return op1 + op2;
                
            case 0x23: // Store address calculation
                return op1 + op2;
                
            case 0x37: // lui
                return op2;
                
            case 0x17: // auipc
                return op1 + op2;
                
            default:
                return 0;
        }
    }
    
    // Check for data hazards
    bool checkDataHazard() {
        // Check if ID stage needs a register that will be written by EX or MEM stage
        if (id_ex.valid && id_ex.instr.rd != 0) {
            if (if_id.valid && 
                ((if_id.instr.rs1 == id_ex.instr.rd) || 
                 (if_id.instr.rs2 == id_ex.instr.rd))) {
                return true;
            }
        }
        
        if (ex_mem.valid && ex_mem.instr.rd != 0) {
            if (if_id.valid && 
                ((if_id.instr.rs1 == ex_mem.instr.rd) || 
                 (if_id.instr.rs2 == ex_mem.instr.rd))) {
                return true;
            }
        }
        
        return false;
    }
    
    // Pipeline stages
    void stage_IF() {
        if (!hazard_detected) {
            uint32_t raw_instr = fetchInstruction(pc);
            if_id.instr = decodeInstruction(raw_instr, pc);
            if_id.pc = pc;
            if_id.valid = true;
            pc += 4;
        }
    }
    
    void stage_ID() {
        if (if_id.valid && !id_ex.stall) {
            id_ex.instr = if_id.instr;
            id_ex.pc = if_id.pc;
            id_ex.valid = true;
            
            // Set control signals based on instruction
            setControlSignals(id_ex.instr);
            
            if_id.valid = false;
        }
    }
    
    void stage_EX() {
        if (id_ex.valid) {
            ex_mem.instr = id_ex.instr;
            ex_mem.pc = id_ex.pc;
            ex_mem.valid = true;
            
            // Execute ALU operation
            uint32_t op1 = registers[id_ex.instr.rs1];
            uint32_t op2;
            
            if (alu_src) {
                op2 = id_ex.instr.imm;
            } else {
                op2 = registers[id_ex.instr.rs2];
            }
            
            uint32_t alu_result = executeALU(op1, op2, id_ex.instr.funct3, 
                                            id_ex.instr.funct7, id_ex.instr.opcode);
            
            // Store ALU result in instruction for later stages
            ex_mem.instr.imm = alu_result;
            
            id_ex.valid = false;
        }
    }
    
    void stage_MEM() {
        if (ex_mem.valid) {
            mem_wb.instr = ex_mem.instr;
            mem_wb.pc = ex_mem.pc;
            mem_wb.valid = true;
            
            // Memory operations
            if (mem_read) {
                // Load from memory
                uint32_t addr = ex_mem.instr.imm;
                if (addr < memory.size()) {
                    switch (ex_mem.instr.funct3) {
                        case 0x0: // lb
                            mem_wb.instr.imm = (int8_t)memory[addr];
                            break;
                        case 0x1: // lh
                            mem_wb.instr.imm = (int16_t)(memory[addr] | (memory[addr + 1] << 8));
                            break;
                        case 0x2: // lw
                            mem_wb.instr.imm = memory[addr] | 
                                            (memory[addr + 1] << 8) |
                                            (memory[addr + 2] << 16) |
                                            (memory[addr + 3] << 24);
                            break;
                        case 0x4: // lbu
                            mem_wb.instr.imm = memory[addr];
                            break;
                        case 0x5: // lhu
                            mem_wb.instr.imm = memory[addr] | (memory[addr + 1] << 8);
                            break;
                        default:
                            mem_wb.instr.imm = 0;
                            break;
                    }
                }
            }
            
            if (mem_write) {
                // Store to memory
                uint32_t addr = ex_mem.instr.imm;
                uint32_t data = registers[ex_mem.instr.rs2];
                if (addr < memory.size()) {
                    switch (ex_mem.instr.funct3) {
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
                        default:
                            break;
                    }
                }
            }
            
            ex_mem.valid = false;
        }
    }
    
    void stage_WB() {
        if (mem_wb.valid) {
            // Write back to register
            if (reg_write && mem_wb.instr.rd != 0) {
                uint32_t write_data;
                
                if (mem_to_reg) {
                    write_data = mem_wb.instr.imm; // Data from memory
                } else {
                    write_data = mem_wb.instr.imm; // ALU result
                }
                
                registers[mem_wb.instr.rd] = write_data;
            }
            
            // Handle branch/jump instructions
            if (branch) {
                // Branch target calculation would be handled in EX stage
                // For simplicity, we'll handle it here
                bool branch_taken = false;
                uint32_t rs1_val = registers[mem_wb.instr.rs1];
                uint32_t rs2_val = registers[mem_wb.instr.rs2];
                
                switch (mem_wb.instr.funct3) {
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
                    default:
                        break;
                }
                
                if (branch_taken) {
                    pc = mem_wb.pc + mem_wb.instr.imm;
                }
            }
            
            // Handle JAL and JALR
            if (mem_wb.instr.opcode == 0x6F) { // jal
                registers[mem_wb.instr.rd] = mem_wb.pc + 4;
                pc = mem_wb.pc + mem_wb.instr.imm;
            } else if (mem_wb.instr.opcode == 0x67) { // jalr
                registers[mem_wb.instr.rd] = mem_wb.pc + 4;
                pc = (registers[mem_wb.instr.rs1] + mem_wb.instr.imm) & ~1;
            }
            
            mem_wb.valid = false;
            instruction_count++;
        }
    }
    
    // Set control signals based on instruction
    void setControlSignals(const Instruction& instr) {
        // Reset all control signals
        reg_write = false;
        mem_read = false;
        mem_write = false;
        mem_to_reg = false;
        alu_src = false;
        alu_op = 0;
        branch = false;
        
        switch (instr.opcode) {
            case 0x33: // R-type
                reg_write = true;
                alu_src = false;
                break;
                
            case 0x13: // I-type (arith/logic)
                reg_write = true;
                alu_src = true;
                break;
                
            case 0x03: // I-type (load)
                reg_write = true;
                mem_read = true;
                mem_to_reg = true;
                alu_src = true;
                break;
                
            case 0x23: // S-type
                mem_write = true;
                alu_src = true;
                break;
                
            case 0x63: // B-type
                branch = true;
                break;
                
            case 0x37: // lui
                reg_write = true;
                break;
                
            case 0x17: // auipc
                reg_write = true;
                break;
                
            case 0x6F: // jal
                reg_write = true;
                branch = true;
                break;
                
            case 0x67: // jalr
                reg_write = true;
                mem_to_reg = true;
                alu_src = true;
                branch = true;
                break;
        }
    }
    
    // Run simulation
    void run() {
        // Simple simulation - run for a fixed number of cycles
        // In a real implementation, we'd run until program completion
        
        for (int i = 0; i < 1000; i++) {
            cycle_count++;
            
            // Check for hazards
            hazard_detected = checkDataHazard();
            
            // Execute pipeline stages (in reverse order for proper forwarding)
            stage_WB();
            stage_MEM();
            stage_EX();
            stage_ID();
            stage_IF();
            
            // Break if no valid instructions in pipeline
            if (!if_id.valid && !id_ex.valid && !ex_mem.valid && !mem_wb.valid) {
                break;
            }
        }
    }
    
    // Print final state (for debugging)
    void printState() {
        cout << "Simulation completed:" << endl;
        cout << "Cycles: " << cycle_count << endl;
        cout << "Instructions: " << instruction_count << endl;
        
        // Print register values
        for (int i = 0; i < NUM_REGISTERS; i++) {
            if (registers[i] != 0) {
                cout << "x" << i << " = 0x" << hex << setw(8) << setfill('0') << registers[i] << dec << endl;
            }
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