"""Correct a verified scalar which the original data splitter mistook for code.

D_002EB190 is consumed in 16-byte TLB records by 0x2BC968. Its second
record's EntryHi is 0x00100000; it is not a pointer to the program entry.
Keep the correction scoped to this table, never to the function name globally.
"""

def literal_for(table, expression):
    if table == 'D_002EB190' and expression == 'func_00100000':
        return '0x00100000'
    return expression


def literal_at(table, offset):
    return 0x00100000 if table == 'D_002EB190' and offset == 0x14 else None
