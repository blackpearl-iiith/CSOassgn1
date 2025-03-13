.global palindrome

palindrome:
    movq %rsi, %r8       # %r8 = size
    decq %r8            # %r8 = size - 1
    movq $0, %r9       # left = 0
    movq %r8, %r10         # right = size - 1

L1:
    cmpq %r10, %r9              # comparing left and right
    jge L5                      # if left >= right, jump to L5

    movzbq (%rdi, %r9), %r11   # Putting s[left] into %r11
    movzbq (%rdi, %r10), %r12  # Putting s[right] into %r12

    cmpb %r11b, %r12b             # comparing s[left] and s[right]
    jne L4                      # if not equal, jump to L4

    incq %r9         # left++
    decq %r10     # right--
    jmp L1       # repeat the loop

L4:
    movq $0, %rax      # return 0
    ret

L5:
    movq $1, %rax               # return 1
    ret
