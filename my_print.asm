; ------------
; void _nout(COLOR c, const char* s);
; 输出一个指定颜色的字符串
; 参数通过esp传递
global _nout

SECTION .data:
    COLOR_RED   db  0x1B, '[31m', 0H ; 红色，\[31m，0x1B代表转义字符
    COLOR_NULL  db  0x1B, '[0m', 0H  ; 无色，\[0m

SECTION .text:

_nout:
    mov     eax, [esp + 4]           ; get color
    cmp     eax, 0                   ; if(c==COLOR_NULL)
    je      .print_string
.change_to_red:
    mov     eax, COLOR_RED
    call    sprint                   ; print(c)
.print_string:
    mov     eax, [esp + 8]           ; get string
    call    sprint                   ; print(s)
.reset_color:
    mov     eax, COLOR_NULL          ; print(COLOR_NULL)
    call    sprint
    ret

    ; ------------
    ; int slen(const char* eax)
    ; get string length
slen:
    push    ebx
    mov     ebx, eax                 ; use ebx as *begin

.nextchar:
    cmp     byte[eax], 0
    jz      .finished
    inc     eax
    jmp     .nextchar

.finished:
    sub     eax, ebx                 ; eax -= ebx
    pop     ebx
    ret

    ; ------------
    ; void sprint(const char* eax)
    ; print string
sprint:
    push    edx
    push    ecx
    push    ebx
    push    eax
    call    slen

    mov     edx, eax
    pop     eax

    mov     ecx, eax
    mov     ebx, 1
    mov     eax, 4
    int     0x80

    pop     ebx
    pop     ecx
    pop     edx
    ret