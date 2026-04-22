/* vinrom.h — Vimana ROM format: shared between compiler and VM
 *
 * Binary layout (little-endian):
 *
 *  Header (28 bytes):
 *    u32 magic         = 0x564D4941 ('VIMANA')
 *    u16 version       = 1
 *    u16 flags         = 0
 *    u32 nconsts
 *    u32 nfuncs
 *    u32 nglobals
 *    u32 nextfuncs     (external function name table size)
 *    u32 entry_func    (0xFFFFFFFF = none)
 *
 *  Constant pool  (variable length):
 *    for each const: u8 type + data
 *      INT:  i64 (8 bytes)
 *      FLT:  f64 (8 bytes)
 *      STR:  u32 len, then len bytes (no NUL)
 *      BOOL: u8
 *      NULL: (no data)
 *
 *  Global table  (nglobals × 8 bytes):
 *    u32 name_idx      (const pool index)
 *    u32 init_idx      (const pool index for literal init, 0xFFFFFFFF = null/complex)
 *
 *  External function table  (nextfuncs × 4 bytes):
 *    u32 name_idx      (const pool index of "module.func" string)
 *
 *  Function table  (variable):
 *    for each func:
 *      u32 name_idx    (0xFFFFFFFF = anonymous)
 *      u16 arity
 *      u16 nlocals
 *      u32 code_size
 *      u8  code[code_size]
 */
#pragma once
#include <stdint.h>

#define YROM_MAGIC    0x564D4941
#define YROM_VERSION  1

/* Constant pool types */
#define YCON_INT   0
#define YCON_FLT   1
#define YCON_STR   2
#define YCON_BOOL  3
#define YCON_NULL  4

/* Opcodes — 1 byte; operands follow inline */
#define OP_HALT    0x00
#define OP_CONST   0x01  /* u16 const_idx                  */
#define OP_NULL    0x02
#define OP_TRUE    0x03
#define OP_FALSE   0x04
#define OP_LDLOC   0x05  /* u16 slot                       */
#define OP_STLOC   0x06  /* u16 slot                       */
#define OP_LDGLOB  0x07  /* u16 global_idx                 */
#define OP_STGLOB  0x08  /* u16 global_idx                 */
#define OP_POP     0x09
#define OP_DUP     0x0A
#define OP_ADD     0x10
#define OP_SUB     0x11
#define OP_MUL     0x12
#define OP_DIV     0x13
#define OP_MOD     0x14
#define OP_NEG     0x15
#define OP_EQ      0x20
#define OP_NEQ     0x21
#define OP_LT      0x22
#define OP_LE      0x23
#define OP_GT      0x24
#define OP_GE      0x25
#define OP_AND     0x30
#define OP_OR      0x31
#define OP_NOT     0x32
#define OP_BAND    0x33
#define OP_BOR     0x34
#define OP_SHL     0x35
#define OP_SHR     0x36
#define OP_ISNULL  0x37  /* push bool: TOS == null          */
#define OP_JMP     0x40  /* i32 offset from end of instr   */
#define OP_JF      0x41  /* i32 offset; jump if TOS falsy  */
#define OP_CALL    0x50  /* u16 func_idx, u8 argc          */
#define OP_CEXT    0x51  /* u16 ext_idx,  u8 argc          */
#define OP_RET     0x52
#define OP_RETNULL 0x53
#define OP_CLOSURE 0x54  /* u16 func_idx, u8 capture_count */
#define OP_NEWARR  0x60
#define OP_APUSH   0x61  /* ( arr val -- arr )             */
#define OP_AGET    0x62  /* ( arr idx -- val )             */
#define OP_ASET    0x63  /* ( arr idx val -- arr )         */
#define OP_ALEN    0x64  /* ( arr -- len )                 */
#define OP_STRCAT  0x70
#define OP_FLOOR   0x80
#define OP_TOSTR   0x81
#define OP_TOINT   0x82
#define OP_TOFLT   0x83
