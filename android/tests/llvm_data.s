.section .data
.p2align 4
.globl bt4_test_data_table
bt4_test_data_table:
    .long guest_value
    .long bt4_test_data_table + 4
    .long 0x12345678
