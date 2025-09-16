.global get

get:
    movq $0,%r8  #once=0
    movq $0,%r9  #twice=0

    movq $0, %r10     # i = 0

L1:
    cmpq %rsi, %r10   #comparing i with the size of array
    jge L3           #if i>=size, jump to L10

    movq (%rdi,%r10,8), %rcx  #iterating array

    xorq %rcx, %r8   # once ^= arr[i]
    notq %r9   # ~twice
    andq %r9, %r8   # (once ^ arr[i]) & ~twice
    notq %r9   # ~twice->twice
    
    xorq %rcx, %r9   # twice ^= arr[i]
    notq %r8   # ~once
    andq %r8, %r9   # (twice ^ arr[i]) & ~once;
    notq %r8   # ~once->once

L2:
    incq %r10         # i++
    jmp L1










L3:
    movq %r8,%rax    # return result into %rax 
    ret
