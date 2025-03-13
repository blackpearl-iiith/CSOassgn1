.global difference

difference:
    movq (%rdi),%r8  # min = array[0]
    movq (%rdi),%r9  # max = array[0]
    movq $1, %r10     # i = 1 

L1:
    cmpq %rsi, %r10   #comparing i with the size of array
    jge L6             #if i>=size, jump to L6


    movq (%rdi,%r10,8), %rcx  #iterating the array

    cmpq %r8, %rcx     # compare arr[i] and min
    jl L5             # check for next if condition


L8:
    cmpq %r9, %rcx   # compare arr[i] and max
    jg L9               # go to update max


L10:
    incq %r10         # i++
    jmp L1

L5:
    movq %rcx, %r8   # updating min
    jmp L8

L9:
    movq %rcx, %r9     #updatinf max
    jmp L10

L6:
    subq %r8, %r9   # subtract min from max, rsult stored in %rdx
    movq %r9, %rax   # return result into %rax 
    ret
