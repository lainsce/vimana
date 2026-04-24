/* vinrom.h — Vimana ROM format: shared between compiler and VM
 *
 * Binary layout (little-endian):
 *
 *  Header (128 bytes):
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
 *      WORDS: u32 count, then count little-endian u16 words
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
#define YCON_INT   0  /* 8-byte signed integer */
#define YCON_FLT   1  /* 8-byte IEEE 754 float */
#define YCON_STR   2  /* String: u32 length + bytes (no NUL) */
#define YCON_BOOL  3  /* Boolean: u8 (0 or 1) */
#define YCON_NULL  4  /* Null: no data */
#define YCON_WORDS 5  /* Packed u16 array: u32 count + u16 words */

/* Opcodes — 1 byte; operands follow inline */
#define OP_BRK     0x00  /* ( -- )                          */
#define OP_LIT     0x01  /* u16 const_idx                   */
#define OP_LDA     0x02  /* u16 slot; local variable        */
#define OP_STA     0x03  /* u16 slot; local variable        */
#define OP_LDZ     0x04  /* u16 global_idx; global variable */
#define OP_STZ     0x05  /* u16 global_idx; global variable */
#define OP_POP     0x06  /* ( val -- )                      */
#define OP_DUP     0x07  /* ( val -- val val )              */
#define OP_NIP     0x08  /* ( val1 val2 -- val2 )           */
#define OP_ADD     0x09  /* ( val1 val2 -- val1 + val2 )    */
#define OP_SUB     0x0A  /* ( val1 val2 -- val1 - val2 )    */
#define OP_MUL     0x0B  /* ( val1 val2 -- val1 * val2 )    */
#define OP_DIV     0x0C  /* ( val1 val2 -- val1 / val2 )    */
#define OP_EQU     0x0D  /* ( val1 val2 -- val1 == val2 )   */
#define OP_NEQ     0x0E  /* ( val1 val2 -- val1 != val2 )   */
#define OP_LTH     0x0F  /* ( val1 val2 -- val1 < val2 )    */
#define OP_GTH     0x10  /* ( val1 val2 -- val1 > val2 )    */
#define OP_NOT     0x11  /* ( val -- !val )                 */
#define OP_AND     0x12  /* ( val1 val2 -- val1 && val2 )   */
#define OP_ORA     0x13  /* ( val1 val2 -- val1 || val2 )   */
#define OP_EOR     0x14  /* ( val1 val2 -- val1 ^ val2 )    */
#define OP_JMP     0x15  /* i32 offset; jump                 */
#define OP_FUN     0x16  /* u16 func_idx, u8 argc           */
#define OP_DEI     0x17  /* u16 ext_idx,  u8 argc           */
#define OP_RET     0x18  /* ( val -- )                      */
#define OP_SWP     0x19  /* ( val1 val2 -- val2 val1 )      */
#define OP_OVR     0x1A  /* ( val1 val2 -- val1 val2 val1 ) */
#define OP_ROT     0x1B  /* ( val1 val2 val3 -- val2 val3 val1 ) */
#define OP_SFT     0x1C  /* ( val shift -- shifted )        */
#define OP_JCN     0x1D  /* ( bool -- ) i32 offset; jump if true */
#define OP_CLO     0x1E  /* u16 func_idx, u8 n_captures     */
#define OP_DEO     0x1F  /* u16 ext_idx,  u8 argc; discard result */
