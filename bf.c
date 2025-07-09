#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define ERROR(msg) fprintf(stderr, "Error: %s\n", msg)
#define MEMORY_SIZE 30000

// Since writing a closure in C is a pain let's instead
// store a sequence of ( opcode; data ) pairs.
// Apart from this the approach is pretty much the same.

int get_count(const char *content, char c, int i) {
    while (content[++i] == c)
        ;
    return i;
}

typedef enum {
    OP_INC,        // +
    OP_DEC,        // -
    OP_PTR_INC,    // >
    OP_PTR_DEC,    // <
    OP_WRITE,      // .
    OP_READ,       // ,
    OP_LOOP_START, // [
    OP_LOOP_END,   // ]
    OP_EXIT
} OpCode;

typedef struct {
    OpCode opcode;
    int argument;
} Instruction;

typedef struct {
    int ip;
    int ptr;
    uint8_t memory[MEMORY_SIZE]; // 8-bit by design

    Instruction *instructions;
    int instruction_count;
    int instruction_capacity;

    int *loop_stack;
    int loop_stack_top;
    int loop_stack_capacity;
} vm;

char *read_entire_file(const char *filename, long *out_size) {
    FILE *fp = fopen(filename, "rb");
    if (!fp) {
        perror("Failed to open file");
        return NULL;
    }
    fseek(fp, 0, SEEK_END);
    long size = ftell(fp);
    fseek(fp, 0, SEEK_SET);
    if (size < 0) {
        fclose(fp);
        return NULL;
    }
    char *buffer = malloc(size + 1);
    if (!buffer) {
        fclose(fp);
        return NULL;
    }
    if (fread(buffer, 1, size, fp) != size) {
        free(buffer);
        fclose(fp);
        return NULL;
    }
    fclose(fp);
    buffer[size] = '\0';
    *out_size = size;
    return buffer;
}

void vm_init(vm *v) {
    memset(v, 0, sizeof(vm));
    v->instruction_capacity = 8;
    v->instructions = malloc(v->instruction_capacity * sizeof(Instruction));
    v->loop_stack_capacity = 8;
    v->loop_stack = malloc(v->loop_stack_capacity * sizeof(int));
    if (!v->instructions || !v->loop_stack) {
        ERROR("Failed to allocate VM");
        exit(1);
    }
}

void vm_free(vm *v) {
    free(v->instructions);
    free(v->loop_stack);
}

void add_instruction(vm *v, OpCode op, int arg) {
    if (v->instruction_count >= v->instruction_capacity) {
        v->instruction_capacity *= 2;
        v->instructions = realloc(v->instructions, v->instruction_capacity *
                                                       sizeof(Instruction));
        if (!v->instructions) {
            ERROR("realloc failed");
            exit(1);
        }
    }
    v->instructions[v->instruction_count++] = (Instruction){op, arg};
}

void push_loop(vm *v, int ip) {
    if (v->loop_stack_top >= v->loop_stack_capacity) {
        v->loop_stack_capacity *= 2;
        v->loop_stack =
            realloc(v->loop_stack, v->loop_stack_capacity * sizeof(int));
        if (!v->loop_stack) {
            ERROR("realloc failed");
            exit(1);
        }
    }
    v->loop_stack[v->loop_stack_top++] = ip;
}

int pop_loop(vm *v) {
    if (v->loop_stack_top == 0) {
        ERROR("Mismatched ']'");
        exit(1);
    }
    return v->loop_stack[--v->loop_stack_top];
}

int main(int argc, char **argv) {
    if (argc < 2) {
        ERROR("Expected argument: file");
        return 1;
    }

    long content_size;
    char *content = read_entire_file(argv[1], &content_size);
    if (!content) {
        return 1;
    }

    vm prog;
    vm_init(&prog);

    // Compile
    for (int i = 0; i < content_size;) {
        char c = content[i];
        int start = i;
        switch (c) {
        case '+':
            while (content[++i] == '+')
                ;
            add_instruction(&prog, OP_INC, i - start);
            break;
        case '-':
            while (content[++i] == '-')
                ;
            add_instruction(&prog, OP_DEC, i - start);
            break;
        case '>':
            while (content[++i] == '>')
                ;
            add_instruction(&prog, OP_PTR_INC, i - start);
            break;
        case '<':
            while (content[++i] == '<')
                ;
            add_instruction(&prog, OP_PTR_DEC, i - start);
            break;
        case '.':
            add_instruction(&prog, OP_WRITE, 0);
            i++;
            break;
        case ',':
            add_instruction(&prog, OP_READ, 0);
            i++;
            break;
        case '[':
            push_loop(&prog, prog.instruction_count);
            add_instruction(&prog, OP_LOOP_START, 0); // Placeholder argument
            i++;
            break;
        case ']': {
            int loop_start_ip = pop_loop(&prog);
            int loop_end_ip = prog.instruction_count;
            // Set the jump target for the opening '['
            prog.instructions[loop_start_ip].argument = loop_end_ip + 1;
            // Add the closing ']' instruction, jumping back to the start
            add_instruction(&prog, OP_LOOP_END, loop_start_ip + 1);
            i++;
            break;
        }
        default:
            i++; // Ignore other characters
            break;
        }
    }
    add_instruction(&prog, OP_EXIT, 0);
    free(content);

    if (prog.loop_stack_top != 0) {
        ERROR("Mismatched '['");
        vm_free(&prog);
        return 1;
    }

    // Execute
    while (prog.instructions[prog.ip].opcode != OP_EXIT) {
        Instruction inst = prog.instructions[prog.ip];
        switch (inst.opcode) {
        case OP_INC:
            prog.memory[prog.ptr] += inst.argument;
            prog.ip++;
            break;
        case OP_DEC:
            prog.memory[prog.ptr] -= inst.argument;
            prog.ip++;
            break;
        case OP_PTR_INC:
            prog.ptr += inst.argument;
            prog.ip++;
            break;
        case OP_PTR_DEC:
            prog.ptr -= inst.argument;
            prog.ip++;
            break;
        case OP_WRITE:
            putchar(prog.memory[prog.ptr]);
            prog.ip++;
            break;
        case OP_READ:
            prog.memory[prog.ptr] = getchar();
            prog.ip++;
            break;
        case OP_LOOP_START:
            prog.ip =
                (prog.memory[prog.ptr] == 0) ? inst.argument : prog.ip + 1;
            break;
        case OP_LOOP_END:
            prog.ip =
                (prog.memory[prog.ptr] != 0) ? inst.argument : prog.ip + 1;
            break;
        case OP_EXIT:
            break; // Should be caught by the while loop condition
        }
    }

    vm_free(&prog);
    return 0;
}
