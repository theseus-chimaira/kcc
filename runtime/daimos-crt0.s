; KCC native phase startup adapter.
;
; DAIMOS supplies AC1=argc, AC2=counted-SIXBIT argv, AC3=envp.  The bootstrap
; C adapter converts argv to ordinary C strings and calls the phase main().
        .text
        .globl _start
        .globl kcc_native_start
        .globl dsys_exit
_start:
        pushj 17,kcc_native_start
        pushj 17,dsys_exit
        halt .
