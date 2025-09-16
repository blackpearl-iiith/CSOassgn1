.global fillB

fillB:
    movq $1, %r8       # left = 1
    movq $0, %r9            # i = 0

L1:
    cmpq %rsi, %r9  # Checking i and size
    jge L2                      # If i >= size, jump to L2

    movq %r8, (%rdx, %r9, 8)    # B[i] = left
    movq (%rdi, %r9, 8), %r10   # Moving A[i] into %r10
    imulq %r10, %r8      # left = left * A[i]

    incq %r9                    # i++
    jmp L1                      # looping

L2:
    movq $1, %r8      # right = 1
    movq %rsi, %r9         # i = size
    decq %r9            # i = size - 1

L3:
    cmpq $0, %r9                # Checking i and 0
    jl L5        # If i < 0, jump to L5

    movq (%rdx, %r9, 8), %r10   #Moving B[i] into %r10
    imulq %r8, %r10           # B[i] = B[i] * right
    movq %r10, (%rdx, %r9, 8)   # Storing result back to B[i]

    movq (%rdi, %r9, 8), %r11   # Load A[i] into %r11
    imulq %r11, %r8             # right = right * A[i]

    decq %r9      # i--
    jmp L3     # looping

L5:
    ret
