global maxofthree

SECTION .text

maxofthree: 
    mov     eax, [esp+4]
    mov     ecx, [esp+8]
    mov     edx, [esp+12]
label_2:
    cmp     eax, ecx
    cmovl   eax, ecx
    cmp     eax, edx
    cmovl   eax, edx
    ret
