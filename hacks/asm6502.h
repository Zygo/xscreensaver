/*
 * Copyright (c) 2007 Jeremy English <jhe@jeremyenglish.org>
 * 
 * Permission to use, copy, modify, distribute, and sell this software and its
 * documentation for any purpose is hereby granted without fee, provided that
 * the above copyright notice appear in all copies and that both that
 * copyright notice and this permission notice appear in supporting
 * documentation.  No representations are made about the suitability of this
 * software for any purpose.  It is provided "as is" without express or 
 * implied warranty.
 * 
 * Created: 07-May-2007 
 */

/* 
      This is a port of the javascript 6502 assembler, compiler and
      debugger. The orignal code was copyright 2006 by Stian Soreng -
      www.6502asm.com

   The stack space is in page $100 to $1ff. The video buffer starts at
   $200 and is 1024 bytes. Programs get loaded at address
   $600. Address $fe is random and byte $ff is used for user input.

   User input is disabled.
*/

#ifndef __ASM6502_H__
#define __ASM6502_H__

typedef uint8_t  Bit8;
typedef uint16_t Bit16;
typedef uint32_t Bit32;

#undef  BOOL
#undef  TRUE
#undef  FALSE
#define BOOL  Bit8
#define TRUE  1
#define FALSE 0

enum {
  MAX_LABEL_LEN = 80, 
  NUM_OPCODES = 56, /* Number of unique instructions not counting DCB */
  MEM_64K = 65536, /* We have 64k of memory to work with. */
  MAX_PARAM_VALUE = 25, /* The number of values allowed behind dcb */
  MAX_CMD_LEN = 4, /* Each assembly command is 3 characeters long */
/* The stack works from the top down in page $100 to $1ff */
  STACK_TOP = 0x1ff,
  STACK_BOTTOM = 0x100, 
  PROG_START = 0x600 /* The default entry point for the program */
};

typedef enum{
  SINGLE, IMMEDIATE_VALUE, IMMEDIATE_GREAT, 
    IMMEDIATE_LESS, INDIRECT_X, INDIRECT_Y,
    ZERO, ZERO_X, ZERO_Y,
    ABS_VALUE, ABS_OR_BRANCH, ABS_X, ABS_Y,
    ABS_LABEL_X, ABS_LABEL_Y, DCB_PARAM
} m6502_AddrMode;

typedef struct machine_6502 machine_6502;

typedef struct {
  char name[MAX_CMD_LEN];
  Bit8 Imm;
  Bit8 ZP;
  Bit8 ZPX;
  Bit8 ZPY;
  Bit8 ABS;
  Bit8 ABSX;
  Bit8 ABSY;
  Bit8 INDX;
  Bit8 INDY;
  Bit8 SNGL;
  Bit8 BRA;
  void (*func) (machine_6502*, m6502_AddrMode);
} m6502_Opcodes;

/* Used to cache the index of each opcode */
typedef struct {
  Bit8 index;
  m6502_AddrMode adm;
} m6502_OpcodeIndex;

/* Plotter is a function that will be called everytime a pixel
   needs to be updated. The first two parameter are the x and y
   values. The third parameter is the color index:

   Color Index Table
   00 black      #000000
   01 white      #ffffff
   02 Red        #880000
   03 seafoam    #aaffee
   04 fuscia     #cc44cc
   05 green      #00cc55
   06 blue       #0000aa
   07 Yellow     #eeee77
   08 tangerine  #dd8855
   09 brown      #664400
   10 salmon     #ff7777
   11 charcoal   #333333
   12 smoke      #777777
   13 lime       #aaff66
   14 light blue #0088ff
   15 gray       #bbbbbb

   The plotter state variable of the machine gets passed as the forth
   parameter. You can use this parameter to store state information.

*/
typedef void (*m6502_Plotter) (Bit8, Bit8, Bit8, void*);

struct machine_6502 {
  BOOL codeCompiledOK;
  Bit8 regA;
  Bit8 regX;
  Bit8 regY;
  Bit8 regP;
  Bit16 regPC; /* A pair of 8 bit registers */
  Bit16 regSP;
  Bit16 defaultCodePC;
  Bit8 memory[MEM_64K];
  BOOL runForever;
  int labelPtr;
  BOOL codeRunning;
  int myInterval;
  m6502_Opcodes opcodes[NUM_OPCODES];
  int screen[32][32];
  int codeLen;
  m6502_OpcodeIndex opcache[0xff];
  m6502_Plotter plot;
  void *plotterState;
};

/* build6502() - Creates an instance of the 6502 machine */
machine_6502 *m6502_build(void);

/* destroy6502() - compile the file and exectue it until the program
   is finished */
void m6502_destroy6502(machine_6502 *machine);

/* eval_file() - Compiles and runs a file until the program is
   finished */
void m6502_eval_file(machine_6502 *machine, const char *filename, 
	       m6502_Plotter plot, void *plotterState);

/* start_eval_file() - Compile the file and execute the first
   instruction */
void m6502_start_eval_file(machine_6502 *machine, const char *filename, 
		     m6502_Plotter plot, void *plotterState);

/* XXX
void m6502_start_eval_binary(machine_6502 *machine, Bit8 *program,
		       unsigned int proglen,
		       Plotter plot, void *plotterState);
*/

void m6502_start_eval_string(machine_6502 *machine, const char *code,
		       m6502_Plotter plot, void *plotterState);

/* next_eval() - Execute the next insno of machine instructions */
void m6502_next_eval(machine_6502 *machine, int insno);

/* hexDump() - Dumps memory to output */
void m6502_hexDump(machine_6502 *machine, Bit16 start, 
	     Bit16 numbytes, FILE *output);

/* Disassemble() - Prints the assembly code for the program currently
   loaded in memory.*/
void m6502_disassemble(machine_6502 *machine, FILE *output);

/* trace() - Prints to output the current value of registers, the
   current nmemonic, memory address and value. */
void m6502_trace(machine_6502 *machine, FILE *output);

/* save_program() - Writes a binary file of the program loaded in
   memory. */
/* XXX
void save_program(machine_6502 *machine, const char *filename);
*/
#endif /* __ASM6502_H__ */
