#include "cpu_common.h"

/* ================= 取指 ================= */
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
        /* 立即数 / 偏移 / 跳转目标永远是指令的最后 8 字节 */
        cpu->valC = read_mem(cpu, pc + size - 8, 8);
        if (cpu->stat != AOK)
            return;
    }

    cpu->valP = pc + size;

    if (cpu->icode == I_HALT)
        cpu->stat = HLT;
}

/* ================= 译码 ================= */
void decode(CPU *cpu)
{
    cpu->valA = 0;
    cpu->valB = 0;

    switch (cpu->icode) {
    case I_RRMOVQ:                      
        cpu->valA = cpu->reg[cpu->rA];
        break;

    case I_ALU:                        
    case I_RMMOVQ:                      
        cpu->valA = cpu->reg[cpu->rA];
        cpu->valB = cpu->reg[cpu->rB];
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

/* ================= 执行 ================= */
void execute(CPU *cpu)
{
    cpu->valE = 0;
    cpu->cnd = 0;

    switch (cpu->icode) {
    case I_RRMOVQ:
        cpu->valE = cpu->valA;
        cpu->cnd = check_cond(cpu, cpu->ifun);
        break;

    case I_IRMOVQ:
        cpu->valE = cpu->valC;
        break;

    case I_RMMOVQ:
    case I_MRMOVQ:
        cpu->valE = cpu->valB + cpu->valC;
        break;

    case I_ALU: {
        uint64_t a = cpu->valA;
        uint64_t b = cpu->valB;
        uint64_t r = 0;

        switch (cpu->ifun) {
        case 0: r = b + a; break;       /* addq */
        case 1: r = b - a; break;       /* subq */
        case 2: r = b & a; break;       /* andq */
        case 3: r = b ^ a; break;       /* xorq */
        default: cpu->stat = INS; return;
        }

        cpu->valE = r;
        cpu->zf = (r == 0);
        cpu->sf = (int)(r >> 63);

        if (cpu->ifun == 0)
            cpu->of = (int)((~(b ^ a) & (b ^ r)) >> 63);
        else if (cpu->ifun == 1)
            cpu->of = (int)(((b ^ a) & (b ^ r)) >> 63);
        else
            cpu->of = 0;
        break;
    }

    case I_JMP:
        cpu->cnd = check_cond(cpu, cpu->ifun);
        break;

    case I_PUSHQ:
    case I_CALL:
        cpu->valE = cpu->valB - 8;
        break;

    case I_POPQ:
    case I_RET:
        cpu->valE = cpu->valB + 8;
        break;

    default:
        break;
    }
}

/* ================= 访存 ================= */
void memory_stage(CPU *cpu)
{
    cpu->valM = 0;

    switch (cpu->icode) {
    case I_RMMOVQ:
        write_mem(cpu, cpu->valE, 8, cpu->valA);
        break;

    case I_PUSHQ:
        write_mem(cpu, cpu->valE, 8, cpu->valA);
        break;

    case I_CALL:
        write_mem(cpu, cpu->valE, 8, cpu->valP);
        break;

    case I_MRMOVQ:
        cpu->valM = read_mem(cpu, cpu->valE, 8);
        break;

    case I_POPQ:
    case I_RET:
        cpu->valM = read_mem(cpu, cpu->valA, 8); 
        break;

    default:
        break;
    }
}

/* ================= 写回 ================= */
void write_back(CPU *cpu)
{
    int dstE = RNONE;
    int dstM = RNONE;

    switch (cpu->icode) {
    case I_RRMOVQ:
        if (cpu->cnd)
            dstE = cpu->rB;
        break;

    case I_IRMOVQ:
    case I_ALU:
        dstE = cpu->rB;
        break;

    case I_MRMOVQ:
        dstM = cpu->rA;
        break;

    case I_PUSHQ:
    case I_CALL:
    case I_RET:
        dstE = RSP;
        break;

    case I_POPQ:
        dstE = RSP;
        dstM = cpu->rA;
        break;

    default:
        break;
    }

    if (dstE != RNONE)
        cpu->reg[dstE] = cpu->valE;
    if (dstM != RNONE)
        cpu->reg[dstM] = cpu->valM;
}

/* ================= 更新 PC ================= */
void update_pc(CPU *cpu)
{
    switch (cpu->icode) {
    case I_JMP:
        cpu->pc = cpu->cnd ? cpu->valC : cpu->valP;
        break;

    case I_CALL:
        cpu->pc = cpu->valC;
        break;

    case I_RET:
        cpu->pc = cpu->valM;
        break;

    default:
        cpu->pc = cpu->valP;
        break;
    }
}
