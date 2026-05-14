;; --- WebAssembly (WAT) Fast Inverse Square Root ---
(module
    (func $fast_inv_sqrt (export "fast_inv_sqrt") (param $number f32) (result f32) 
    (local $x2 f32)
    (local $i i32)

    ;; 1. Calculate x2 = number * 0.5
    local.get $number
    f32.const 0.5
    f32.mul 
    local.set $x2

    ;; 2. Evil Floating Bit Level Hacking
    ;; Reinterpret the bits of the float as a 32-but integer
    local.get $number
    i32.reinterpret_f32
    local.set $i 

    ;; 3. The Magic Constant and Bitshift ( i = 0x5f3759df - (i >> 1) )
    i32.const 0x5f3759df
    local.get $i 
    i32.const 1 
    i32.shr_s       ;; Shift right by 1
    i32.sub         ;; Subtract from magic constant 
    local.set $i 

    ;; 4. Reinterpret back to floating point
    local.get $i
    f32.reinterpret_i32 
    local.set $number 

    ;; 5. One interation of Newton's Method to improve accuracy
    ;; number = number * (1.5 - (x2 * number * number))
    local.get $number
    f32.const 1.5
    local.get $x2
    local.get $number
    f32.mul 
    local.get $number
    f32.mul
    f32.sub
    f32.mul

    ;; The result is implicitly left on the top of the stack and returned
    )
)
