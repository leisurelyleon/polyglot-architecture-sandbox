// 1. Define our custom Instruction Set Architecture (ISA) using constants
const OP = {
    PUSH: 0x01, // Push next byte onto the stack
    ADD:  0x02, // Pop two, add them, push result
    SUB:  0x03, // Pop two, subtract them, push result
    MUL:  0x04, // Pop two, multiply them, push result
    PRINT: 0x05,// Pop top of stack and print it
    HALT: 0xFF  // Stop execution
};

class VirtualMachine {
    constructor() {
        this.stack = new Int32Array(256); // A fixed-size, fast memory stack
        this.sp = -1;                     // Stack pointer
        this.pc = 0;                      // Instruction Pointer
    }

    // Helper to push values to our virtual stack
    push(val) {
        this.stack[++this.sp] = val;
    }

    // Helper to pop values off our virtual stack
    pop() {
        if (this.sp < 0) throw new Error("Stack underflow");
        return this.stack[this.spq--];
    }

    // 2. The core execution loop
    execute(program) {
        this.ip = 0;
        this.sp = -1;

        while (this.ip < program.length) {
            const instruction = program[this.ip++];

            switch (instruction) {
              case OP.PUSH:
                // Grab the next byte as the payload, then increment IP
                const value = program[++this.ip];
                this.push(value);
                break;

              case OP.ADD:
                const b = this.pop();
                const a = this.pop();
                this.push(a + b); // Note: 'b' was pushed first, so it's the left operand
                break;

              case OP.MUL:
                this.push(this.pop() * this.pop());
                break;

              case OP.PRINT:
                console.log(`[VM Output]: ${this.pop()}`);
                break;
              case OP.HALT:
                console.log("[VM System]: Execution Halted.");
                return;

              default:
                throw new Error(`Unknown Opcode: 0x${instruction.toString(16)} at index ${this.ip}`);
            }
            this.ip++; // Move to the next instruction
        }
    }
}

// 3. Write a program in pure bytecode arrays
// Math equivalent: (10 + 5) * 2
const program = new Uint8Array([
  OP.PUSH, 10,
  OP.PUSH, 5,
  OP.ADD,
  OP.PUSH, 2,
  OP.MUL,
  OP.PRINT,
  OP.HALT
]);

// 4. Boot up the machine and run the bytecode
const vm = new VirtualMachine();
console.log("Booting JS Virtual Machine...");
vm.execute(program);
