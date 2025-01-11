# CCDIS - A Tracing Disassembler for 6502 Assembler and BCPL CINTCODE
This disassembler is able to disassemble both 6502 machine code into assembler
nmemonics, and also disassemble the code called CINTCODE used by some RCP
implementations of BCPL.

There are two types of input file.  One is a BCPL CINTCODE hunk file.  This is
a file format documented in the manual for "BCPL on the BBC Microcomputer"
and is the normal format used for executable (compiled) BCPL programs that
will be loaded into a running BCPL system.  The file contains a series of
hunks each of which may be either CINTCODE, 6502 machine code, or a
relocation hunk that is paired with a 6502 machine code hunk so as to make
the 6502 code relocatable.  To disassemble files of this type specify the
-h or --hunk option to ccdis and ccdis will use the hunk types from the
file to guide the disassembly.

The other type of file is a raw file that is loaded into memory, for example
with *LOAD or *RUN.  The may contain only 6502 machine code or a mix of
6502 machine code and assembler but there is no structure to the file that
would help ccdis work which parts are which.  Instead a labels file is needed.
The labels file enables name to be assigned to addresses and to declare whether
the address concerned is the start of 6502 machine code, CINTCODE or some form
of data, for example words, bytes, or strings.  To use ccdis to disassemble a
raw file like this, use the -s or --simple option along with the -l or --label
option, the latter specifying the name of the file containing the labels.

An example labels file (for the BCPL ROM) is:
```
# Zero page workspace.

0x0000  word    cc_mcst
0x0001  data    cc_mcst_msb
0x0002  data    cc_currco_lsb
0x0003  data    cc_currco_msb
0x0004  data    cc_sp_lsb
0x0005  data    cc_sp_msb
0x0006  data    cc_pc_lsb
0x0007  data    cc_pc_msb
0x0008  data    cc_a_lsb
0x0009  data    cc_a_msb
0x000a  data    cc_b_lsb
0x000b  data    cc_b_msb
0x000c  data    cc_c_lsb
0x000d  data    cc_c_msb
0x000e  data    wa_gv
0x0010  data    ba_result2
0x0012  data    ba_mcresult
0x0014  data    ba_faultroutine
0x0016  data    flags
0x0022  data    limits
0x0026  data    trapstate
0x0034  data    trap_count
0x003e  data    debug_flag
0x0040  data    runstate
0x004e  data    lastjmp
0x0052  data    workarea

# Operating System Vectors

0x0202  word    BRKV

# Language RAM

0x416   mcode   dispatch

# Operating System Entry Points.

0xfff7  mcode   OSCLI
0xfff4  mcode   OSBYTE
0xfff1  mcode   OSWORD
0xffee  mcode   OSWRCH
0xffe7  mcode   OSNEWL
0xffe3  mcode   OSASCI
0xffdd  mcode   OSFILE
0xffda  mcode   OSARGS
0xffd7  mcode   OSBGET
0xffd4  mcode   OSBPUT
0xffd1  mcode   OSGBPB
0xffce  mcode   OSFIND

# ROM Header.

0x8000  mcode   langent
0x8003  mcode   servent
0x8006  data    romtype
0x8007  data    copyoff
0x8008  data    binver
0x8009  string  title
0x800e  string  ascver
0x8011  string  copyright
0x803b  string  cmduc
0x803f  string  cmdlc

# General Labels.

0x8043  mcode   service
0x804e  mcode   cmdloop
0x805a  mcode   cmdmat
0x8068  mcode   notbcpl
0x806e  mcode   brk_handler
0x8083  mcode   init
0x8086  mcode   init_lp1
0x8096  mcode   init_lp2
0x80a5  mcode   init_lp3
0x80ab  mcode   init_zplp
0x893d  string  error
0x8949  data    lram_move_spec
0x894f  data    lram_clear_spec
0x897a  data    lram_init_data
0x8955  data    zp_dest
0x8962  data    zp_lowbyte
0x896e  data    zp_hibyte
0x8990  mcode   dispatch_rom
0x89aa  data
0x8ae3  mcode   movebyte_mc
0x8ae7  mcode   movebyte_oloop
0x8aeb  mcode   movebyte_iloop
0x8af9  mcode   movebyte_tail
0x8b04  mcode   movebyte_done
0x8c23  data
0xbe00  data    cc_table_lo
0xbf00  data    cc_table_hi

0x8234  mcode   opcode_jeq
0x8238  mcode   opcode_jne
0x823c  mcode   opcode_jls
0x8240  mcode   opcode_jgr
0x8244  mcode   opcode_jle
0x8248  mcode   opcode_jge
0x824d  mcode   opcode_jeq0
0x8251  mcode   opcode_jne0
0x8255  mcode   opcode_jls0
0x8259  mcode   opcode_jgr0
0x825d  mcode   opcode_jle0
0x8261  mcode   opcode_jge0
0x8266  mcode   opcode_j
0x826a  mcode   opcode_brk
0x8277  mcode   opcode_mul
0x82a0  mcode   opcode_div
0x82a6  mcode   opcode_rem
0x82db  mcode   opcode_sla
0x82e6  mcode   opcode_ll
0x82f2  mcode   opcode_kn
0x82fa  mcode   opcode_k
0x8303  mcode   opcode_kw
0x830e  mcode   opcode_kng
0x8317  mcode   opcode_fhop
0x831e  mcode   opcode_ln
0x8331  mcode   opcode_xor
0x833e  mcode   opcode_lng
0x8352  mcode   opcode_lg
0x8358  mcode   opcode_sg
0x8363  mcode   opcode_llg
0x836c  mcode   opcode_ag
0x8375  mcode   opcode_kng1
0x837e  mcode   opcode_kng2
0x8387  mcode   opcode_rtn
0x83b3  mcode   opcode_llp
0x83bf  mcode   opcode_llpw
0x83d3  mcode   opcode_lm1
0x83dc  mcode   opcode_add
0x83ea  mcode   opcode_sub
0x83f8  mcode   opcode_and
0x8405  mcode   opcode_or
0x8412  mcode   opcode_lsh
0x841e  mcode   opcode_rsh
0x8448  mcode   opcode_lll
0x8458  mcode   opcode_rv
0x8461  mcode   opcode_l
0x846a  mcode   opcode_lw
0x8473  mcode   opcode_s0g1
0x8479  mcode   opcode_s0g2
0x847f  mcode   opcode_lng1
0x8489  mcode   opcode_lng2
0x8493  mcode   opcode_lg1
0x8499  mcode   opcode_lg2
0x849f  mcode   opcode_sg1
0x84a5  mcode   opcode_sg2
0x84ab  mcode   opcode_llg1
0x84b1  mcode   opcode_llg2
0x84b7  mcode   opcode_ag1
0x84bd  mcode   opcode_ag2
0x84c3  mcode   opcode_swb
0x84f5  mcode   opcode_swl
0x8559  mcode   opcode_goto
0x856d  mcode   opcode_gbyt
0x8577  mcode   opcode_pbyt
0x8597  mcode   opcode_s0g
0x85a1  mcode   opcode_st
0x85ae  mcode   opcode_st0pn
0x85b4  mcode   opcode_st1pn
0x85c9  mcode   opcode_xch
0x85d8  mcode   opcode_atb
0x85de  mcode   opcode_atc
0x85e9  mcode   opcode_sn
0x85fa  mcode   opcode_lp
0x8600  mcode   opcode_lpn
0x861f  mcode   opcode_lpw
0x862b  mcode   opcode_spn
0x8638  mcode   opcode_sp
0x8646  mcode   opcode_spw
0x864f  mcode   opcode_stpn
0x865e  mcode   opcode_undef
0x8661  mcode   opcode_a
0x8667  mcode   opcode_aw
0x8678  mcode   opcode_lm
0x8684  mcode   opcode_neg
0x868c  mcode   opcode_an
0x8697  mcode   opcode_nop
0x869a  mcode   opcode_not
0x86a7  mcode   opcode_apw
0x86b0  mcode   opcode_ap
0x86b6  mcode   opcode_apn
0x86c1  mcode   opcode_rvpn
0x86d0  mcode   opcode_l0pn
0x86d6  mcode   opcode_l1pn
0x86dc  mcode   opcode_l2pn
0x86e3  mcode   opcode_l3pn
0x86ea  mcode   opcode_l4pn
0x8701  mcode   opcode_code2
0x871a  mcode   opcode_code1
0x8776  mcode   opcode_trap

0x886a  mcode   get_imm16
0x8875  mcode   get_imm8

# CINTCODE

0x8c56  cintcode    BEGIN
0x9850  cintcode    DOGBPB
0x98c4  cintcode    RBIN
0x9ece  cintcode
0x9ef6  cintcode
0x9f20  cintcode
0x9f30  cintcode
0x9f56  cintcode
0x9f58  cintcode
0x9f8e  cintcode
0x9faa  cintcode
0x9fac  cintcode
0x9fb0  cintcode
0x9ff0  data
0xa02a  cintcode
0xa048  cintcode
0xa058  cintcode
0xa082  cintcode
0xa0a0  cintcode
0xa0b6  cintcode
0xa1f6  cintcode
0xa1f8  cintcode
0xa21a  cintcode

```
