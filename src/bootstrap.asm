global _start

section .data
    filename db "tests/app.jl", 0
    
    ; We are going to print it like this: "-> FOUND TAG: [ main ]"
    msg_prefix db "-> FOUND TAG: [ ", 0
    len_prefix equ $ - msg_prefix
    
    msg_suffix db " ]", 10, 0    ; The '10' adds a clean New Line at the end
    len_suffix equ $ - msg_suffix

section .bss
    buffer resb 1024
    tag_buffer resb 64       ; NEW: A 64-byte empty box to hold the tag's name!

section .text
_start:
    ; 1. OPEN THE FILE
    mov rax, 2
    mov rdi, filename
    mov rsi, 0
    syscall
    mov r8, rax

    ; 2. READ THE FILE INTO RAM
    mov rax, 0
    mov rdi, r8
    mov rsi, buffer
    mov rdx, 1024
    syscall
    mov r9, rax

    ; 3. THE MAIN SCANNER
    mov rcx, r9         
    mov rbx, buffer     

scan_loop:
    cmp rcx, 0
    je exit_program

    mov al, byte [rbx]
    cmp al, 123             ; Did we find '{' ?
    je start_extract        ; If YES, jump to our new extraction loop!

continue_scan:
    inc rbx
    dec rcx
    jmp scan_loop

    ; -----------------------------------------
    ; 4. THE EXTRACTION LOOP
    ; -----------------------------------------
start_extract:
    mov r10, tag_buffer     ; R10 is our pointer for writing into the empty box
    inc rbx                 ; Move past the '{' character so we don't save it
    dec rcx                 

extract_loop:
    cmp rcx, 0              ; Did we run out of file?
    je exit_program

    mov al, byte [rbx]      ; Grab the next letter
    cmp al, 125             ; Did we find '}' ?
    je print_tag            ; If YES, we finished grabbing the word! Print it!

    ; If it's a normal letter, copy it into our tag_buffer box
    mov byte [r10], al      
    inc r10                 ; Move our box pointer forward 1 space
    inc rbx                 ; Move our file pointer forward 1 space
    dec rcx
    jmp extract_loop        ; Loop again!

    ; -----------------------------------------
    ; 5. PRINT THE EXACT WORD WE FOUND
    ; -----------------------------------------
print_tag:
    push rcx                ; Save our timers
    push rbx

    ; A. Print the prefix: "-> FOUND TAG: [ "
    mov rax, 1
    mov rdi, 1
    mov rsi, msg_prefix
    mov rdx, len_prefix
    syscall

    ; B. Print the extracted word (e.g., "main")
    ; To tell the kernel how many letters to print, we subtract the start 
    ; of the box from our current position in the box. 
    mov rdx, r10            
    sub rdx, tag_buffer     ; RDX now holds the exact length of the word!
    
    mov rax, 1
    mov rdi, 1
    mov rsi, tag_buffer
    syscall

    ; C. Print the suffix: " ]" and a New Line
    mov rax, 1
    mov rdi, 1
    mov rsi, msg_suffix
    mov rdx, len_suffix
    syscall

    pop rbx                 ; Restore our timers
    pop rcx
    jmp continue_scan       ; Go back to scanning the rest of the file

    ; -----------------------------------------
    ; 6. EXIT CLEANLY
    ; -----------------------------------------
exit_program:
    mov rax, 60
    mov rdi, 0
    syscall