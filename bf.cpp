#include <functional>
#include <iostream>
#include <stdexcept>
#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
/*
** A closure-based BrainFuck intepreter.
** Inspired by
*https://planetscale.com/blog/faster-interpreters-in-go-catching-up-with-cpp
**
** and https://github.com/skx/closure-based-brainfuck-vm.
**
** Compiles the input program into a series of closures.
*/
#define ERROR(msg)                                                             \
    do {                                                                       \
        fprintf(stderr, "Error: %s\n", msg);                                   \
    } while (0)

long get_file_size_cross_platform(const char *filename) {
    struct stat st;

#ifdef _WIN32
    if (_stat(filename, &st) == 0) {
#else
    if (stat(filename, &st) == 0) {
#endif
        return st.st_size;
    }
    return -1;
}

char *read_entire_file(const char *filename) {
    FILE *fp = fopen(filename, "rb"); // Binary mode for exact byte count
    if (fp == NULL) {
        perror("Failed to open file");
        return NULL;
    }

    // Get file size
    fseek(fp, 0, SEEK_END);
    long size = ftell(fp);
    fseek(fp, 0, SEEK_SET);

    if (size == -1) {
        perror("Failed to get file size");
        fclose(fp);
        return NULL;
    }

    // Allocate buffer (+1 for null terminator)
    char *buffer = (char *)malloc(size + 1);
    if (buffer == NULL) {
        perror("Failed to allocate memory");
        fclose(fp);
        return NULL;
    }

    // Read entire file
    size_t bytes_read = fread(buffer, 1, size, fp);
    fclose(fp);

    if (bytes_read != (size_t)size) {
        fprintf(stderr, "Failed to read entire file\n");
        free(buffer);
        return NULL;
    }

    // Null-terminate the string
    buffer[size] = '\0';

    return buffer;
}

struct vm;

typedef std::function<void(struct vm *)> func;
typedef struct vm {
    int ip;            // instruction pointer
    int ptr;           // data pointer
    int memory[30000]; // only use 8 bits
    std::vector<func> fns;
    vm() : ip(0), ptr(0), memory{}, fns() {}
} vm;

struct rpt {
    int count;
    int i;
};

func vm_exit() {
    return [](vm *VM) { throw std::runtime_error("DONE"); };
}

func inc(int n) {
    return [n](vm *VM) {
        VM->memory[VM->ptr] += n;
        VM->memory[VM->ptr] &= 0xFF;
        ++VM->ip;
    };
}

func dec(int n) {
    return [n](vm *VM) {
        VM->memory[VM->ptr] -= n;
        VM->memory[VM->ptr] &= 0xFF;
        ++VM->ip;
    };
}

func ptr_inc(int n) {
    return [n](vm *VM) {
        VM->ptr += n;
        ++VM->ip;
    };
}

func ptr_dec(int n) {
    return [n](vm *VM) {
        VM->ptr -= n;
        ++VM->ip;
    };
}

func vm_read() {
    return [](vm *VM) {
        int res;
        if ((res = scanf("%d", &(VM->memory[VM->ptr]))) < 0)
            exit(1);
        VM->memory[VM->ptr] &= 0xFF;
        ++VM->ip;
    };
}

func vm_write() {
    return [](vm *VM) {
        printf("%c", VM->memory[VM->ptr]);
        ++VM->ip;
    };
}

func vm_loop_start(int offset) {
    return [offset](vm *VM) {
        if (VM->memory[VM->ptr] != 0x00) {
            ++VM->ip;
            return;
        }
        VM->ip = offset;
    };
}

func noop() {
    return [](vm *VM) {};
}

func vm_loop_end(int offset) {
    return [offset](vm *VM) {
        if (VM->memory[VM->ptr] == 0x00) {
            ++VM->ip;
            return;
        }
        VM->ip = offset;
    };
}

struct rpt findMultiple(char *content, int sz, int start, char c) {
    int k = start;
    while (k < sz) {
        if (content[k] != c) {
            break;
        }
        k++;
    }
    return {.count = (k - start), .i = k};
};

int main(const int argc, const char **argv) {

    if (argc < 2) {
        ERROR("Expected argument: file");
        return 1;
    }

    FILE *fp = fopen(argv[1], "r");
    if (fp == NULL) {
        ERROR("Could not find file");
        exit(1);
    }

    int i = 0;
    long sz = get_file_size_cross_platform(argv[1]);
    if (sz < 1) {

        ERROR("Invalid BrainFuck program.");
        exit(1);
    }

    char *content = read_entire_file(argv[1]);
    if (content == NULL) {
        ERROR("Failed to read file");
        exit(1);
    }
    vm prog;
    char c;

    // Compile
    std::vector<int> loop_stack;
    struct rpt r;
    while (i < sz) {
        c = content[i];
        if (c == '\0')
            break;
        switch (c) {
        case '+':
            r = findMultiple(content, sz, i, '+');
            prog.fns.push_back(inc(r.count));
            i = r.i;
            break;
        case '-':
            r = findMultiple(content, sz, i, '-');
            prog.fns.push_back(dec(r.count));
            i = r.i;
            break;
        case '>':
            r = findMultiple(content, sz, i, '>');
            prog.fns.push_back(ptr_inc(r.count));
            i = r.i;
            break;
        case '<':
            r = findMultiple(content, sz, i, '<');
            prog.fns.push_back(ptr_dec(r.count));
            i = r.i;
            break;
        case ',':
            prog.fns.push_back(vm_read());
            ++i;
            break;
        case '.':
            prog.fns.push_back(vm_write());
            ++i;
            break;
        case '[':
            loop_stack.push_back(prog.fns.size());
            prog.fns.push_back(noop());
            i++;
            break;
        case ']': {
            int loop = loop_stack.back();
            loop_stack.pop_back();
            prog.fns[loop] = vm_loop_start(prog.fns.size());
            prog.fns.push_back(vm_loop_end(loop));
            i++;
            break;
        }
        default:
            i++;
            break;
        }
    }
    prog.fns.push_back(vm_exit());
    fclose(fp);
    free(content);

    // Execute
    while (1) {
        try {
            prog.fns[prog.ip](&prog);
        } catch (const std::exception &e) {
            return 0;
        }
    }
}
