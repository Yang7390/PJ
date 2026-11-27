#include "cpu_common.h"

/* 每个阶段通过 CPU 结构体中的暂存值传递数据。请补全以下六个阶段。 */
void fetch(CPU *cpu)
{
    uint64_t pc = cpu->pc;
    uint64_t byte0 = read_mem(cpu, pc, 1);
    if (cpu->stat != AOK)
        return;

    cpu->icode = (int)(byte0 >> 4);
    cpu->ifun  = (int)(byte0 & 0x0f);
    cpu->rA = RNONE;
    cpu->rB = RNONE;

    unsigned size = 1;
    int has_regs = 0;
    int has_imm  = 0;

    switch (cpu->icode) {
    case I_HALT:
    case I_NOP:
        if (cpu->ifun != 0) { cpu->stat = INS; return; }
        break;

    case I_RRMOVQ:
    case I_ALU:
        if (cpu->ifun > 6) { cpu->stat = INS; return; }
        size = 2;
        has_regs = 1;
        break;

    case I_JMP:
        if (cpu->ifun > 6) { cpu->stat = INS; return; }
        size = 9;
        has_imm = 1;
        break;

    case I_IRMOVQ:
    case I_RMMOVQ:
    case I_MRMOVQ:
        if (cpu->ifun != 0) { cpu->stat = INS; return; }
        size = 10;
        has_regs = 1;
        has_imm = 1;
        break;

    case I_PUSHQ:
    case I_POPQ:
        if (cpu->ifun != 0) { cpu->stat = INS; return; }
        size = 2;
        has_regs = 1;
        break;

    case I_CALL:
        if (cpu->ifun != 0) { cpu->stat = INS; return; }
        size = 9;
        has_imm = 1;
        break;

    case I_RET:
        if (cpu->ifun != 0) { cpu->stat = INS; return; }
        break;

    default:
        cpu->stat = INS;
        return;
    }

    if (has_regs) {
        uint64_t regs = read_mem(cpu, pc + 1, 1);
        if (cpu->stat != AOK)
            return;
        cpu->rA = (int)(regs >> 4);
        cpu->rB = (int)(regs & 0x0f);
    }

    if (has_imm) {
        cpu->valC = read_mem(cpu, pc + 2, 8);
        if (cpu->stat != AOK)
            return;
    }

    cpu->valP = pc + size;

    if (cpu->icode == I_HALT)
        cpu->stat = HLT;
}

void decode(CPU *cpu)
{
    cpu->valA = 0;
    cpu->valB = 0;

    switch (cpu->icode) {
    case I_RRMOVQ:
    case I_ALU:
    case I_RMMOVQ:
        cpu->valA = cpu->reg[cpu->rA];
        break;

    case I_MRMOVQ:
        cpu->valB = cpu->reg[cpu->rB];
        break;

    case I_PUSHQ:
        cpu->valA = cpu->reg[cpu->rA];   
        cpu->valB = cpu->reg[RSP];     
        break;

    case I_POPQ:
    case I_RET:
        cpu->valA = cpu->reg[RSP];      
        cpu->valB = cpu->reg[RSP];
        break;

    case I_CALL:
        cpu->valB = cpu->reg[RSP];       
        break;

    default:
        break;
    }
}

void execute(CPU *cpu)
{
    /* TODO: 选择 ALU 操作数并计算 valE。OPq 更新 ZF、SF、OF。
       条件传送和跳转使用 check_cond 计算 cnd。使用无符号运算实现 64 位回绕。 */
    cpu->valE = 0;
    cpu->cnd = 0;
}

void memory_stage(CPU *cpu)
{
    /* TODO: 根据指令访问内存。pushq 写入 valA，call 写入 valP。
       读取操作将结果写入 valM，使用 read_mem 和 write_mem 检查地址。 */
    cpu->valM = 0;
}

void write_back(CPU *cpu)
{
    /* TODO: 选择 dstE、dstM 并写入寄存器。
       若两者均为 RSP，valM 应覆盖 valE。 */
    (void)cpu;
}

void update_pc(CPU *cpu)
{
    /* TODO: 按 call、条件跳转、ret 或顺序执行选择下一条指令的地址。 */
    cpu->pc = cpu->valP;
}
