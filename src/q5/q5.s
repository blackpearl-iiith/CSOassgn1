.global fillB

fillB:
    movq $0, %r9               # i = 0 for making all element of B to 1
L1:
    cmpq %rsi, %r9             # Compare i with size
    jge L2                     # If i >= size, jump to L2 (end of loop)

    movq $1, (%rdx, %r9, 8)    # B[i] = 1
    incq %r9                   # i++
    jmp L1                     # Jump to the start of the loop

L2:
    # Initialize left = 1
    movq $1, %r8               # left = 1
    movq $0, %r9               # Reset i = 0 for the second loop

L3:
    cmpq %rsi, %r9             # Compare i with size
    jge L4                     # If i >= size, jump to L4 (end of loop)

    movq (%rdx, %r9, 8), %r10   # Load B[i] into %r10
    imulq %r8, %r10            # Multiply B[i] by left (B[i] = B[i] * left)
    movq %r10, (%rdx, %r9, 8)   # Store the result back to B[i]

    movq (%rdi, %r9, 8), %r11   # Load A[i] into %r11
    imulq %r11, %r8            # left = left * A[i]

    incq %r9                   # i++
    jmp L3                     # Jump to the next iteration

L4:
    # Initialize right = 1 for the third loop
    movq $1, %r9               # right = 1
    movq %rsi, %r10            # Set i = size - 1
    subq $1, %r10              # i = size - 1

L5:
    cmpq $0, %r10              # Compare i with 0
    jl L6                      # If i < 0, jump to L6 (end of loop)

    movq (%rdx, %r10, 8), %r11   # Load B[i] into %r11
    imulq %r9, %r11            # Multiply B[i] by right (B[i] = B[i] * right)
    movq %r11, (%rdx, %r10, 8)  # Store the result back to B[i]

    movq (%rdi, %r10, 8), %r12   # Load A[i] into %r12
    imulq %r12, %r9            # right = right * A[i]

    decq %r10                   # i--
    jmp L5                      # Jump to the next iteration

L6:
    ret
