//
//  main.c
//  BlueVM
//
//  Created by Christopher Langfield on 4/14/21.
//

// test gitignore
//https://blog.subnetzero.io/post/building-language-vm-part-01/
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <signal.h>
#include<readline/readline.h>
#include<readline/history.h>


// colors :)
#define RESET  "\x1B[0m"
#define KRED  "\x1B[31m"
#define KGRN  "\x1B[32m"
#define KYEL  "\x1B[33m"
#define KBLU  "\x1B[34m"
#define KMAG  "\x1B[35m"
#define KCYN  "\x1B[36m"
#define KWHT  "\x1B[37m"

// types
typedef struct {
  char* buffer;
  size_t buffer_length;
  ssize_t input_length;
} REPLBuffer;

// functions
void repl(void);
void print_repl_prompt(void);
uint32_t read_repl_input(REPLBuffer*);
void close_repl_input(REPLBuffer*);
uint32_t repl_parse(const char*);
void execute(uint32_t);


// opcodes
// using 8 bits to represent opcodes = 256 possible
enum {
    OP_NTHNG = 0,
    OP_CEASE = 1, // 0
    OP_ILEGL, // 2
    OP_RLOAD, // 3 load 16bit to a register
    OP_RRADD, // 4 register-register add, store in 3rd register
    OP_RRSUB, // 5 register-register subtract, store in 3rd register
    OP_RRMUX, // 6 register-register multiply, stored in 3rd register
    OP_RRDIV, // 7 register-register divide, store quotient in 3rd register, store remainder in "remainder"
    OP_JUMPA, // 8 absolute jump to a specific address (direct input)
    OP_JUMPF, // 9 relative jump forward from PC, by 16bit offset (direct input)
    OP_JUMPB, // 10 A relative jump backward from PC, by 16bit offset (direct input)
    OP_RJMPA, // 11 B absolute jump to an address specified in a register
    OP_RJMPF, // 12 C relative jump forward frp, PC, by 32bit offset in register (register input)
    OP_RJMPB, // 13 D relative jump backward from PC, by 32bit offset in register (register inpt)
    OP_RREQL, // 14 E are two registers equal --> R_COND
    OP_RRNQL, // 15 F are two register unequal --> R_COND
    OP_RRGTE, // 16 10 is reg 1 gte reg 2 --> R_COND
    OP_RRLTE, // 17 11 is reg 1 lte reg 2 --> R_COND
    OP_RRCMP, // 18 12 compare registers --> R_COMP
    OP_RJMPQ, // 19 13 jump to address specified by register if conditional register has a 1
    OP_RJMPN, // 20 14 " if cond reg has a 0
    OP_JUMPQ, // 21 15 jump by 16bit offset if cond reg true
    OP_JUMPN, // 22 16 jump by 16bit offset if cond reg not true
    OP_RRGTX, // 23 17 is reg 1 gt reg 2 --> R_COND
    OP_RRLTX, // 24 18 is reg 1 lt reg 2 --> R_COND
    OP_COUNT
};

// registers
enum {
    R_00=0,
    R_01,
    R_02,
    R_03,
    R_04,
    R_05,
    R_06,
    R_07,
    R_08,
    R_09,
    R_10,
    R_11,
    R_12,
    R_13,
    R_14,
    R_15,
    R_PC, // program counter
    R_RMDR, // remainder of division
    R_COND, // conditional flag -- holds 1 or 0
    R_COMP, // comparison flag -- holds POS, NEG, ZRO
    R_COUNT // # of registers
};



uint32_t registers[R_COUNT] = {0};

uint32_t ZEXT16(uint16_t x) {
    return (uint32_t)x;
}

uint32_t ZEXT8(uint8_t x) {
    return (uint32_t)x;
}

void DO_RLOAD(int reg, uint16_t value) {
    // | op-code | reg (8) | value (16)
    registers[reg] = ZEXT16(value);
}

void DO_RRADD(int reg1, int reg2, int DR) {
    registers[DR] = registers[reg1] + registers[reg2];
}

void DO_RRSUB(int reg1, int reg2, int DR) {
    registers[DR] = registers[reg1] - registers[reg2];
}

void DO_RRMUX(int reg1, int reg2, int DR) {
    registers[DR] = registers[reg1] * registers[reg2];
}

void DO_RRDIV(int reg1, int reg2, int DRQ) {
    // algebraic quotient is captured by int division in C
    registers[DRQ] = registers[reg1] / registers[reg2];
    // store remainder in R_RMDR register
    registers[R_RMDR] = registers[reg1] % registers[reg2];
}

void DO_JUMPA(uint32_t address) {
    registers[R_PC] = address;
}

void DO_JUMPF(uint16_t offset) {
    registers[R_PC] += ZEXT16(offset);
}

void DO_JUMPB(uint16_t offset) {
    registers[R_PC] -= ZEXT16(offset);
}

void DO_RJMPA(int reg) {
    registers[R_PC] = registers[reg];
}

void DO_RJMPF(int reg) {
    registers[R_PC] += registers[reg];
}

void DO_RJMPB(int reg) {
    registers[R_PC] -= registers[reg];
}

void DO_RREQL(int reg1, int reg2) {
    registers[R_COND] = (uint32_t)(registers[reg1] == registers[reg2]);
}

void DO_RRNQL(int reg1, int reg2) {
    registers[R_COND] = (uint32_t)(registers[reg1] != registers[reg2]);
}

void DO_RRGTE(int reg1, int reg2) {
    registers[R_COND] = (uint32_t)(registers[reg1] >= registers[reg2]);
}

void DO_RRLTE(int reg1, int reg2) {
    registers[R_COND] = (uint32_t)(registers[reg1] <= registers[reg2]);
}

void DO_RRGTX(int reg1, int reg2) {
    registers[R_COND] = (uint32_t)(registers[reg1] > registers[reg2]);
}

void DO_RRLTX(int reg1, int reg2) {
    registers[R_COND] = (uint32_t)(registers[reg1] < registers[reg2]);
}

void DO_RRCMP(int reg1, int reg2) {
    if (registers[reg1] == registers[reg2]) {registers[R_COMP] = (uint32_t)2;} // 010
    else if (registers[reg1] > registers[reg2]) {registers[R_COMP] = (uint32_t)1;} // 001
    else if (registers[reg1] < registers[reg2]) {registers[R_COMP] = (uint32_t)4;} // 100
}

void DO_RJMPQ(int reg) {
    if (registers[R_COND]) {
        registers[R_PC] = registers[reg];
    }
}

void DO_RJMPN(int reg) {
    if (!registers[R_COND]) {
        registers[R_PC] = registers[reg];
    }
}

void DO_JUMPQ(uint16_t offset) {
    if (registers[R_COND]) {
        registers[R_PC] += ZEXT16(offset);
    }
}

void DO_JUMPN(uint16_t offset) {
    if (!registers[R_COND]) {
        registers[R_PC] += ZEXT16(offset);
    }
}



int main(int argc, const char * argv[]) {
    printf(KCYN "Welcome to Blue :)\n" RESET);
    if (argc < 2) {
        printf(KCYN "I expect at least one argument...\n" RESET);
        return 0;
    }
    if (strcmp(argv[1],"repl\n")) {
        printf(KCYN "Running Blue REPL\n" RESET);
        repl();
    }
    
    return 0;
}

// REPL

REPLBuffer* new_repl_buffer() {
    REPLBuffer* repl_buffer = malloc(sizeof(REPLBuffer));
    repl_buffer->buffer = NULL;
    repl_buffer->buffer_length = 0;
    repl_buffer->input_length = 0;

  return repl_buffer;
}

void repl() {
    
    REPLBuffer* repl_buffer = new_repl_buffer();
    uint32_t instr;
    while(1) {
        print_repl_prompt();
        instr = read_repl_input(repl_buffer);

        if (repl_buffer->buffer[0] == '.') {
            
            // special commands
            
            if (strcmp(repl_buffer->buffer, ".exit")==0) {
                close_repl_input(repl_buffer);
                exit(EXIT_SUCCESS);
            }
            if (strcmp(repl_buffer->buffer, ".registers")==0) {
                printf(KCYN "Registers\n" RESET);
                for (int i = 0; i < R_COUNT; i++)
                {
                    if (i == R_COUNT-1) {
                        printf(KCYN "COMP: " RESET);
                    }
                    else if (i == R_COUNT-2) {
                        printf(KCYN "COND:" RESET);
                    }
                    else if (i == R_COUNT-3) {
                        printf(KCYN "RMDR: " RESET);
                    }
                    else if (i == R_COUNT-4) {
                        printf(KCYN "PC: " RESET);
                    }
                    else {
                        printf(KCYN "R_%02d: " RESET, i);
                    }
                    printf("%d\n", registers[i]);
                }
            }
            
            // assembly
        }
        // carriage return just leads to another prompt
        else if (strcmp(repl_buffer->buffer, "") == 0) {continue;}
        // hex instruction code
        else {
            printf("%X\n", instr);
            execute(instr);
        }
    }
}

void print_repl_prompt() {
    printf(KCYN "bl$ " RESET);
}

uint32_t read_repl_input(REPLBuffer* repl_buffer) {
    ssize_t bytes_read = getline(&(repl_buffer->buffer), &(repl_buffer->buffer_length), stdin);
    if (bytes_read <= 0) {
        printf(KRED "Error reading input\n" RESET);
        exit(EXIT_FAILURE);
      }
    repl_buffer->input_length = bytes_read - 1;
    repl_buffer->buffer[bytes_read - 1] = 0;
    
    return (uint32_t)strtoul(repl_buffer->buffer, NULL, 16);
}

void close_repl_input(REPLBuffer* repl_buffer) {
    free(repl_buffer->buffer);
    free(repl_buffer);
}
uint32_t repl_parse(const char *input_buffer) {
    
    return 0;
}

void execute(uint32_t instr) {
    uint32_t op_code;
    op_code = instr >> 24;
    printf("%X\n", op_code);
    switch (op_code) {
        case OP_RLOAD:
        {
            // | 0x03 | reg (8) | value (16 |
            uint16_t value = (uint16_t)(instr & 0xFFFF);
            int DR = (instr >> 16) & 0xFF;
            printf(KCYN "RLOAD 0x%X (%d) into R_%d\n" RESET, value, value, DR);
            DO_RLOAD(DR, value);
            break;
        }
        case OP_RRADD: {
            // | 0x04 | reg1 | reg2 | DR |
            int reg1 = (instr >> 16) & 0xFF;
            int reg2 = (instr >> 8) & 0xFF;
            int DR = instr % 0xFF;
            printf(KCYN "RRADD reg%d reg%d into R_%d\n" RESET, reg1, reg2, DR);
            DO_RRADD(reg1, reg2,  DR);
            break;
        }
        case OP_RRSUB: {
            // | 0x05 | reg1 | reg2 | DR |
            int reg1 = (instr >> 16) & 0xFF;
            int reg2 = (instr >> 8) & 0xFF;
            int DR = instr % 0xFF;
            printf(KCYN "RRSUB reg%d reg%d into R_%d\n" RESET, reg1, reg2, DR);
            DO_RRSUB(reg1, reg2,  DR);
            break;
        }
        case OP_RRMUX: {
            // | 0x06 | reg1 | reg2 | DR |
            int reg1 = (instr >> 16) & 0xFF;
            int reg2 = (instr >> 8) & 0xFF;
            int DR = instr % 0xFF;
            printf(KCYN "RRMUX reg%d reg%d into R_%d\n" RESET, reg1, reg2, DR);
            DO_RRMUX(reg1, reg2,  DR);
            break;
        }
        case OP_RRDIV: {
            // | 0x07 | reg1 | reg2 | DR |
            int reg1 = (instr >> 16) & 0xFF;
            int reg2 = (instr >> 8) & 0xFF;
            int DR = instr % 0xFF;
            printf(KCYN "RRDIV reg%d reg%d into R_%d\n" RESET, reg1, reg2, DR);
            DO_RRDIV(reg1, reg2,  DR);
            break;
        }
        case OP_JUMPA: {
            // | 0x08 | 0x00 | address (16) |
            uint16_t address = (uint16_t)(instr & 0xFFFF);
            printf(KCYN "JUMPA %X\n" RESET,address);
            DO_JUMPA(address);
            break;
        }
        case OP_JUMPF: {
            // | 0x09 | 0x00 | offset (16) |
            uint16_t offset = (uint16_t)(instr & 0xFFFF);
            printf(KCYN "JUMPA %X\n" RESET,offset);
            DO_JUMPF(offset);
            break;
        }
        case OP_JUMPB: {
            // | 0x0a | 0x00 | offset (16) |
            uint16_t offset = (uint16_t)(instr & 0xFFFF);
            printf(KCYN "JUMPA %X\n" RESET,offset);
            DO_JUMPB(offset);
            break;
        }
        case OP_RREQL: {
            // | 0x0e | reg1 | reg2 | 0x00 |
            int reg1 = (instr >> 16) & 0xFF;
            int reg2 = (instr >> 8) & 0xFF;
            printf(KCYN "RREQL reg%d reg%d" RESET, reg1, reg2);
            DO_RREQL(reg1, reg2);
            break;
        }
        case OP_RRNQL: {
            // | 0x0f | reg1 | reg2 | 0x00 |
            int reg1 = (instr >> 16) & 0xFF;
            int reg2 = (instr >> 8) & 0xFF;
            printf(KCYN "RRNQL reg%d reg%d" RESET, reg1, reg2);
            DO_RRNQL(reg1, reg2);
            break;
        }
        case OP_RRGTE: {
            // | 0x10 | reg1 | reg2 | 0x00 |
            int reg1 = (instr >> 16) & 0xFF;
            int reg2 = (instr >> 8) & 0xFF;
            printf(KCYN "RRGTE reg%d reg%d" RESET, reg1, reg2);
            DO_RRGTE(reg1, reg2);
            break;
        }
        case OP_RRLTE: {
            // | 0x11 | reg1 | reg2 | 0x00 |
            int reg1 = (instr >> 16) & 0xFF;
            int reg2 = (instr >> 8) & 0xFF;
            printf(KCYN "RRLTE reg%d reg%d" RESET, reg1, reg2);
            DO_RRLTE(reg1, reg2);
            break;
        }
        case OP_RRGTX: {
            // | 0x17  | reg1 | reg2 | 0x00 |
            int reg1 = (instr >> 16) & 0xFF;
            int reg2 = (instr >> 8) & 0xFF;
            printf(KCYN "RRGTX reg%d reg%d" RESET, reg1, reg2);
            DO_RRGTE(reg1, reg2);
            break;
        }
        case OP_RRLTX: {
            // | 0x18  | reg1 | reg2 | 0x00 |
            int reg1 = (instr >> 16) & 0xFF;
            int reg2 = (instr >> 8) & 0xFF;
            printf(KCYN "RRLTX reg%d reg%d" RESET, reg1, reg2);
            DO_RRLTX(reg1, reg2);
            break;
        }
        case OP_RRCMP: {
            // | 0x12 | reg1 | reg2 | 0x00 |
            int reg1 = (instr >> 16) & 0xFF;
            int reg2 = (instr >> 8) & 0xFF;
            printf(KCYN "RRCMP reg%d reg%d" RESET, reg1, reg2);
            DO_RRCMP(reg1, reg2);
            break;
        }
        case OP_RJMPA: {
            // | 0x0b | reg | 0x00 | 0x00 |
            int reg = (instr >> 16) & 0xFF;
            DO_RJMPA(reg);
        }
        case OP_RJMPF: {
            // | 0x0c | reg | 0x00 | 0x00 |
            int reg = (instr >> 16) & 0xFF;
            DO_RJMPF(reg);
        }
        case OP_RJMPB: {
            // | 0x0d | reg | 0x00 | 0x00 |
            int reg = (instr >> 16) & 0xFF;
            DO_RJMPF(reg);
        }
        case OP_RJMPQ: {
            // | 0x0c | reg | 0x00 | 0x00 |
            int reg = (instr >> 16) & 0xFF;
            DO_RJMPQ(reg);
        }
        case OP_RJMPN: {
            // | 0x0d | reg | 0x00 | 0x00 |
            int reg = (instr >> 16) & 0xFF;
            DO_RJMPN(reg);
        }
        default:
            printf(KRED "UNKNOWN OP_CODE %X [REPL-- did you forget the .?]\n" RESET, op_code);
    }
}
