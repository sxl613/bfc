#include <cstdint>
#include <fstream>
#include <functional>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

#define CELL_SIZE uint8_t // could do 16 bit?

struct RunLengthResult {
    int count;
    int new_index;
};

struct vm;

using Op = std::function<void(vm *)>;

struct vm {
    unsigned int ip = 0;           // Instruction pointer
    unsigned int ptr = 0;          // Data pointer
    std::vector<CELL_SIZE> memory; // VM memory, using a vector of bytes
    std::vector<Op> ops;           // Compiled operations
    bool halted = false;

    vm() : memory(30000, 0) {} // Initialize memory to 30,000 zeroed bytes
};

Op vm_exit() {
    return [](vm *v) { v->halted = true; };
}

Op inc(int n) {
    return [n](vm *v) {
        v->memory[v->ptr] += n;
        ++v->ip;
    };
}

Op dec(int n) {
    return [n](vm *v) {
        v->memory[v->ptr] -= n;
        ++v->ip;
    };
}

Op ptr_inc(int n) {
    return [n](vm *v) {
        v->ptr += n;
        // TODO: Add bounds checking for the pointer ???
        if (v->ptr >= v->memory.size()) {
            // For this simple interpreter, let it wrap or expand memory.
            // Could also throw an error for robustness.
        }
        ++v->ip;
    };
}

Op ptr_dec(int n) {
    return [n](vm *v) {
        v->ptr -= n;
        ++v->ip;
    };
}

Op vm_read() {
    return [](vm *v) {
        char input;
        if (std::cin.get(input)) {
            v->memory[v->ptr] = static_cast<CELL_SIZE>(input);
        } else {
            v->memory[v->ptr] = 0; // Default on EOF
        }
        ++v->ip;
    };
}

Op vm_write() {
    return [](vm *v) {
        std::cout << static_cast<char>(v->memory[v->ptr]);
        ++v->ip;
    };
}

Op vm_loop_start(int jump_target) {
    return [jump_target](vm *v) {
        if (v->memory[v->ptr] == 0) {
            v->ip = jump_target;
        } else {
            ++v->ip;
        }
    };
}

Op vm_loop_end(int jump_target) {
    return [jump_target](vm *v) {
        if (v->memory[v->ptr] != 0) {
            v->ip = jump_target;
        } else {
            ++v->ip;
        }
    };
}

// A placeholder for '[' until we know the matching ']' location
Op noop() {
    return [](vm *v) {
        // This should ideally never be called.
        throw std::logic_error("Unmatched '[' was executed.");
    };
}

// make this more like C++
std::string read_file_content(const char *filename) {
    std::ifstream file(filename);
    if (!file) {
        throw std::runtime_error("Failed to open file");
    }
    std::stringstream buffer;
    buffer << file.rdbuf();
    return buffer.str();
}

RunLengthResult find_multiple(const std::string &content, int start, char c) {
    int k = start;
    while (k < content.length() && content[k] == c) {
        k++;
    }
    return {k - start, k};
}

int main(int argc, char **argv) {
    if (argc < 2) {
        std::cerr << "Error: Expected argument: file" << std::endl;
        return 1;
    }

    try {
        const std::string content = read_file_content(argv[1]);
        if (content.empty()) {
            std::cerr
                << "Error: BrainFuck program is empty or could not be read."
                << std::endl;
            return 1;
        }

        vm prog;
        std::vector<int> loop_stack;

        // Compile
        int i = 0;
        while (i < content.length()) {
            RunLengthResult r;
            switch (content[i]) {
            case '+':
                r = find_multiple(content, i, '+');
                prog.ops.push_back(inc(r.count));
                i = r.new_index;
                break;
            case '-':
                r = find_multiple(content, i, '-');
                prog.ops.push_back(dec(r.count));
                i = r.new_index;
                break;
            case '>':
                r = find_multiple(content, i, '>');
                prog.ops.push_back(ptr_inc(r.count));
                i = r.new_index;
                break;
            case '<':
                r = find_multiple(content, i, '<');
                prog.ops.push_back(ptr_dec(r.count));
                i = r.new_index;
                break;
            case ',':
                prog.ops.push_back(vm_read());
                ++i;
                break;
            case '.':
                prog.ops.push_back(vm_write());
                ++i;
                break;
            case '[':
                loop_stack.push_back(prog.ops.size());
                prog.ops.push_back(noop()); // Placeholder
                i++;
                break;
            case ']': {
                if (loop_stack.empty()) {
                    throw std::runtime_error("Mismatched ']'");
                }
                int loop_start_ip = loop_stack.back();
                loop_stack.pop_back();
                int loop_end_ip = prog.ops.size();
                prog.ops[loop_start_ip] = vm_loop_start(loop_end_ip + 1);
                prog.ops.push_back(vm_loop_end(loop_start_ip + 1));
                i++;
                break;
            }
            default:
                i++; // Ignore other characters
                break;
            }
        }

        if (!loop_stack.empty()) {
            throw std::runtime_error("Mismatched '['");
        }

        prog.ops.push_back(vm_exit());

        // Execute
        while (!prog.halted) {
            prog.ops[prog.ip](&prog);
        }

    } catch (const std::exception &e) {
        std::cerr << "Runtime Error: " << e.what() << std::endl;
        return 1;
    }

    return 0;
}
