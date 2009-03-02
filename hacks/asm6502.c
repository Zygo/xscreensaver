/*-*- indent-tabs-mode:nil -*- */
/* Copyright (C) 2007 Jeremy English <jhe@jeremyenglish.org>
 * 
 * Permission to use, copy, modify, distribute, and sell this software and its
 * documentation for any purpose is hereby granted without fee, provided that
 * the above copyright notice appear in all copies and that both that
 * copyright notice and this permission notice appear in supporting
 * documentation.  No representations are made about the suitability of this
 * software for any purpose.  It is provided "as is" without express or 
 * implied warranty.
 * 
 * Created: 12-April-2007 
 */ 

/*
      This is a port of the javascript 6502 assembler, compiler and
      debugger. The orignal code was copyright 2006 by Stian Soreng -
      www.6502asm.com

      I changed the structure of the assembler in this version.
*/

#define NDEBUG  /* Uncomment when done with debugging */

#include <stdlib.h>
#include <stdio.h>
/*#include <malloc.h>*/
#include <string.h>
#include <errno.h>
#include <stdarg.h>
#include <assert.h>
#include <ctype.h>
#include <math.h>
#include <stdint.h>
#include <unistd.h>

#include "asm6502.h"

#ifdef DEBUGGER
#  define random rand
#endif

typedef enum{
  LEFT, RIGHT
    } Side;

/* 

Bit Flags
   _  _  _  _  _  _  _  _ 
  |N||V||F||B||D||I||Z||C|
   -  -  -  -  -  -  -  - 
   7  6  5  4  3  2  1  0

*/

typedef enum{
      CARRY_FL = 0, ZERO_FL = 1, INTERRUPT_FL = 2, 
	DECIMAL_FL = 3, BREAK_FL = 4, FUTURE_FL = 5,
	OVERFLOW_FL = 6, NEGATIVE_FL = 7
	} Flags;
	

typedef BOOL (*CharTest) (char);

/* A jump function takes a pointer to the current machine and a
   opcode. The opcode is needed to figure out the memory mode. */
/*typedef void (*JumpFunc) (machine_6502* AddrMode);*/

typedef struct {
  AddrMode type;
  Bit32 value[MAX_PARAM_VALUE];
  unsigned int vp; /*value pointer, index into the value table.*/
  char *label;
  Bit32 lbladdr;
} Param;

typedef struct {
  Bit32 addr; /* Address of the label */  
  char *label; 
} Label;  

typedef struct AsmLine AsmLine;
struct AsmLine {
  BOOL labelDecl; /* Does the line have a label declaration? */
  Label *label;
  char *command;
  Param *param;
  AsmLine *next; /* in list */
};

typedef struct {
  Bit16 addr;
  Bit16 value;
} Pointer;

/* eprintf - Taken from "Practice of Programming" by Kernighan and Pike */
static void eprintf(char *fmt, ...){
  va_list args;
  
  char *progname = "Assembler";

  fflush(stdout);
  if (progname != NULL)
    fprintf(stderr, "%s: ", progname);

  va_start(args, fmt);
  vfprintf(stderr, fmt, args);
  va_end(args);
  
  if (fmt[0] != '\0' && fmt[strlen(fmt) -1] == ':')
    fprintf(stderr, " %s", strerror(errno));
  fprintf(stderr, "\n");
  exit(2); /* conventional value for failed execution */
}

/* emalloc - Taken from "Practice of Programming" by Kernighan and
   Pike.  If memory allocatiion fails the program will print a message
   an exit. */
static void *emalloc(size_t n) {
  void *p;
  
  p = malloc(n);
  if (p == NULL)
    eprintf("malloc of %u bytes failed:", n);
  return p;
}

/* ecalloc - Dose the same thing as emalloc just calls calloc instead. */
static void *ecalloc(uint32_t nelm, size_t nsize){
  void *p;
 
  p = calloc(nelm, nsize);
  if (p == NULL)
    eprintf("calloc of %u bytes failed:", nelm * nsize);
  return p;
}

/* estrdup() - Allocates memory for a new string a returns a copy of the source sting in it. */
static char *estrdup(const char *source){
  int ln = strlen(source) + 1;
  char *s = ecalloc(ln, sizeof(char));
  strncpy(s,source,ln);
  return s;
}

static void checkAddress(Bit32 address){
  /* XXX: Do we want to kill the program here? */
  if (address >= MEM_64K)
    fprintf(stderr, "Address %d is beyond 64k", address);
}

/*
 *  stackPush() - Push byte to stack
 *
 */

static void stackPush(machine_6502 *machine, Bit8 value ) {
  if(machine->regSP >= STACK_BOTTOM){
    machine->memory[machine->regSP--] = value;
  }
  else{
    fprintf(stderr, "The stack is full: %.4x\n", machine->regSP);
    machine->codeRunning = FALSE;
  }
}


/*
 *  stackPop() - Pop byte from stack
 *
 */

static Bit8 stackPop(machine_6502 *machine) {
  if (machine->regSP < STACK_TOP){
    Bit8 value =machine->memory[++machine->regSP];
    return value;
  }
  else {
    /*    fprintf(stderr, "The stack is empty.\n"); xxx */
    machine->codeRunning = FALSE;
    return 0;
  }
}

static void pushByte(machine_6502 *machine, Bit32 value ) {
  Bit32 address = machine->defaultCodePC;
  checkAddress(address);
  machine->memory[address] = value & 0xff;
  machine->codeLen++;
  machine->defaultCodePC++;
}

/*
 * pushWord() - Push a word using pushByte twice
 *
 */

static void pushWord(machine_6502 *machine, Bit16 value ) {
  pushByte(machine, value & 0xff );
  pushByte(machine, (value>>8) & 0xff );
}

/*
 * popByte( machine_6502 *machine,) - Pops a byte
 *
 */

static Bit8 popByte( machine_6502 *machine) {
  Bit8 value = machine->memory[machine->regPC];
  machine->regPC++;
  return value;
}

/*
 * popWord() - Pops a word using popByte() twice
 *
 */

static int popWord(machine_6502 *machine) {
  return popByte(machine) + (popByte(machine) << 8);
}


/*
 * memReadByte() - Peek a byte, don't touch any registers
 *
 */

static int memReadByte( machine_6502 *machine, int addr ) {
  if( addr == 0xfe ) return floor( random()%255 );
  return machine->memory[addr];
}

static void updateDisplayPixel(machine_6502 *machine, Bit16 addr){
  Bit8 idx = memReadByte(machine,addr) & 0x0f;
  Bit8 x,y;
  addr -= 0x200;
  x = addr & 0x1f;
  y = (addr >> 5);
  if (machine->plot) {
    machine->plot(x,y,idx,machine->plotterState);
  }
}

/*
 * memStoreByte() - Poke a byte, don't touch any registers
 *
 */

static void memStoreByte( machine_6502 *machine, int addr, int value ) {
  machine->memory[ addr ] = (value & 0xff);
  if( (addr >= 0x200) && (addr<=0x5ff) )
    updateDisplayPixel(machine, addr );
}



/* EMULATION CODE */

static Bit8 bitOn(Bit8 value,Flags bit){
  Bit8 mask = 1;
  mask = mask << bit;
  return ((value & mask) > 0);
}

static Bit8 bitOff(Bit8 value, Flags bit){
  return (! bitOn(value,bit));
}

static Bit8 setBit(Bit8 value, Flags bit, int on){
  Bit8 onMask  = 1;
  Bit8 offMask = 0xff;
  onMask = onMask << bit;
  offMask = offMask ^ onMask;
  return ((on) ? value | onMask : value & offMask);
}

static Bit8 nibble(Bit8 value, Side side){
  switch(side){
  case LEFT:  return value & 0xf0;
  case RIGHT: return value & 0xf;
  default:
    fprintf(stderr,"nibble unknown side\n");
    return 0;
  }
}


/* used for tracing. XXX: combined with function getvalue */
static BOOL peekValue(machine_6502 *machine, AddrMode adm, Pointer *pointer, Bit16 PC){
  Bit8 zp;
  pointer->value = 0;
  pointer->addr = 0;
  switch(adm){
  case SINGLE:
    return FALSE;
  case IMMEDIATE_LESS:
  case IMMEDIATE_GREAT:
  case IMMEDIATE_VALUE:
    pointer->value = memReadByte(machine, PC);
    return TRUE;
  case INDIRECT_X:
    zp = memReadByte(machine, PC) + machine->regX;
    pointer->addr = memReadByte(machine,zp) + 
      (memReadByte(machine,zp+1)<<8);
    pointer->value = memReadByte(machine, pointer->addr);
    return TRUE;
  case INDIRECT_Y:
    zp = memReadByte(machine, PC);
    pointer->addr = memReadByte(machine,zp) + 
      (memReadByte(machine,zp+1)<<8) + machine->regY;
    pointer->value = memReadByte(machine, pointer->addr);
    return TRUE;
  case ZERO:
    pointer->addr = memReadByte(machine, PC);
    pointer->value = memReadByte(machine, pointer->addr);
    return TRUE;
  case ZERO_X:
    pointer->addr = memReadByte(machine, PC) + machine->regX;
    pointer->value = memReadByte(machine, pointer->addr);
    return TRUE;
  case ZERO_Y:
    pointer->addr = memReadByte(machine, PC) + machine->regY;
    pointer->value = memReadByte(machine, pointer->addr);
    return TRUE;
  case ABS_OR_BRANCH:
    pointer->addr = memReadByte(machine, PC);
    return TRUE;
  case ABS_VALUE:
    pointer->addr = memReadByte(machine, PC) + (memReadByte(machine, PC+1) << 8);
    pointer->value = memReadByte(machine, pointer->addr);
    return TRUE;
  case ABS_LABEL_X:
  case ABS_X:
    pointer->addr = (memReadByte(machine, PC) + 
		     (memReadByte(machine, PC+1) << 8)) + machine->regX;
    pointer->value = memReadByte(machine, pointer->addr);
    return TRUE;
  case ABS_LABEL_Y:
  case ABS_Y:
    pointer->addr = (memReadByte(machine, PC) + 
		     (memReadByte(machine, PC+1) << 8)) + machine->regY;
    pointer->value = memReadByte(machine, pointer->addr);
    return TRUE;
  case DCB_PARAM:
    /* Handled elsewhere */
    break;
  }
  return FALSE;

}


/* Figure out how to get the value from the addrmode and get it.*/
static BOOL getValue(machine_6502 *machine, AddrMode adm, Pointer *pointer){
  Bit8 zp;
  pointer->value = 0;
  pointer->addr = 0;
  switch(adm){
  case SINGLE:
    return FALSE;
  case IMMEDIATE_LESS:
  case IMMEDIATE_GREAT:
  case IMMEDIATE_VALUE:
    pointer->value = popByte(machine);
    return TRUE;
  case INDIRECT_X:
    zp = popByte(machine) + machine->regX;
    pointer->addr = memReadByte(machine,zp) + 
      (memReadByte(machine,zp+1)<<8);
    pointer->value = memReadByte(machine, pointer->addr);
    return TRUE;
  case INDIRECT_Y:
    zp = popByte(machine);
    pointer->addr = memReadByte(machine,zp) + 
      (memReadByte(machine,zp+1)<<8) + machine->regY;
    pointer->value = memReadByte(machine, pointer->addr);
    return TRUE;
  case ZERO:
    pointer->addr = popByte(machine);
    pointer->value = memReadByte(machine, pointer->addr);
    return TRUE;
  case ZERO_X:
    pointer->addr = popByte(machine) + machine->regX;
    pointer->value = memReadByte(machine, pointer->addr);
    return TRUE;
  case ZERO_Y:
    pointer->addr = popByte(machine) + machine->regY;
    pointer->value = memReadByte(machine, pointer->addr);
    return TRUE;
  case ABS_OR_BRANCH:
    pointer->addr = popByte(machine);
    return TRUE;
  case ABS_VALUE:
    pointer->addr = popWord(machine);
    pointer->value = memReadByte(machine, pointer->addr);
    return TRUE;
  case ABS_LABEL_X:
  case ABS_X:
    pointer->addr = popWord(machine) + machine->regX;
    pointer->value = memReadByte(machine, pointer->addr);
    return TRUE;
  case ABS_LABEL_Y:
  case ABS_Y:
    pointer->addr = popWord(machine) + machine->regY;
    pointer->value = memReadByte(machine, pointer->addr);
    return TRUE;
  case DCB_PARAM:
    /* Handled elsewhere */
    break;
  }
  return FALSE;

}

static void dismem(machine_6502 *machine, AddrMode adm, char *output){
  Bit8 zp;
  Bit16 n;
  switch(adm){
  case SINGLE:
    output = "";
    break;
  case IMMEDIATE_LESS:
  case IMMEDIATE_GREAT:
  case IMMEDIATE_VALUE:
    n = popByte(machine);
    sprintf(output,"#$%x",n);
    break;
  case INDIRECT_X:
    zp = popByte(machine);
    n = memReadByte(machine,zp) + 
      (memReadByte(machine,zp+1)<<8);
    sprintf(output,"($%x,x)",n);
    break;
  case INDIRECT_Y:
    zp = popByte(machine);
    n = memReadByte(machine,zp) + 
      (memReadByte(machine,zp+1)<<8);
    sprintf(output,"($%x),y",n);
    break;
  case ABS_OR_BRANCH:
  case ZERO:    
    n = popByte(machine);
    sprintf(output,"$%x",n);
    break;
  case ZERO_X:
    n = popByte(machine);
    sprintf(output,"$%x,x",n);
    break;
  case ZERO_Y:
    n = popByte(machine);
    sprintf(output,"$%x,y",n);
    break;
  case ABS_VALUE:
    n = popWord(machine);
    sprintf(output,"$%x",n);
    break;
  case ABS_LABEL_X:
  case ABS_X:
    n = popWord(machine);
    sprintf(output,"$%x,x",n);
    break;
  case ABS_LABEL_Y:
  case ABS_Y:
    n = popWord(machine);
    sprintf(output,"$%x,x",n);
    break;
  case DCB_PARAM:
    output = "";
    break;
  }
}

/* manZeroNeg - Manage the negative and zero flags */
static void manZeroNeg(machine_6502 *machine, Bit8 value){
  machine->regP = setBit(machine->regP, ZERO_FL, (value == 0));
  machine->regP = setBit(machine->regP, NEGATIVE_FL, bitOn(value,NEGATIVE_FL));
}

static void warnValue(BOOL isValue){
  if (! isValue){
    fprintf(stderr,"Invalid Value from getValue.\n");
  }
}

static void jmpADC(machine_6502 *machine, AddrMode adm){
  Pointer ptr;
  Bit16 tmp;
  Bit8 c = bitOn(machine->regP, CARRY_FL);
  BOOL isValue = getValue(machine, adm, &ptr);

  warnValue(isValue);
  
  if (bitOn(machine->regA, NEGATIVE_FL) &&
      bitOn(ptr.value, NEGATIVE_FL))
    machine->regP = setBit(machine->regP, OVERFLOW_FL, 0);
  else
    machine->regP = setBit(machine->regP, OVERFLOW_FL, 1);

  if (bitOn(machine->regP, DECIMAL_FL)) {    
    tmp = nibble(machine->regA,RIGHT) + nibble(ptr.value,RIGHT ) + c;
    /* The decimal part is limited to 0 through 9 */
    if (tmp >= 10){
      tmp = 0x10 | ((tmp + 6) & 0xf);
    }
    tmp += nibble(machine->regA,LEFT) + nibble(ptr.value,LEFT);
    if (tmp >= 160){
      machine->regP = setBit(machine->regP,CARRY_FL,1);
      if (bitOn(machine->regP, OVERFLOW_FL) && tmp >= 0x180)
	machine->regP = setBit(machine->regP, OVERFLOW_FL, 0);
      tmp += 0x60;
    }
    else {
      machine->regP = setBit(machine->regP,CARRY_FL,0);
      if (bitOn(machine->regP, OVERFLOW_FL) && tmp < 0x80)
	machine->regP = setBit(machine->regP, OVERFLOW_FL, 0);
    }
  } /* end decimal */      
  else {
    tmp = machine->regA + ptr.value + c;
    if ( tmp >= 0x100 ){
      machine->regP = setBit(machine->regP,CARRY_FL,1);
      if (bitOn(machine->regP, OVERFLOW_FL) && tmp >= 0x180)
	machine->regP =setBit(machine->regP, OVERFLOW_FL, 0);
    }
    else {
      machine->regP = setBit(machine->regP,CARRY_FL,0);
      if (bitOn(machine->regP, OVERFLOW_FL) && tmp < 0x80)
	machine->regP =setBit(machine->regP, OVERFLOW_FL, 0);
    }
  }

  machine->regA = tmp;
  manZeroNeg(machine,machine->regA);
}

static void jmpAND(machine_6502 *machine, AddrMode adm){
  Pointer ptr;
  BOOL isValue = getValue(machine, adm, &ptr);
  warnValue(isValue);
  machine->regA &= ptr.value;
  manZeroNeg(machine,machine->regA);
}

static void jmpASL(machine_6502 *machine, AddrMode adm){
  Pointer ptr;
  BOOL isValue = getValue(machine, adm, &ptr);
  if (isValue){
      machine->regP = setBit(machine->regP, CARRY_FL, bitOn(ptr.value, NEGATIVE_FL));
      ptr.value = ptr.value << 1;
      ptr.value = setBit(ptr.value, CARRY_FL, 0);
      memStoreByte(machine, ptr.addr, ptr.value);
      manZeroNeg(machine,ptr.value);
  }
  else { /* Accumulator */
      machine->regP = setBit(machine->regP, CARRY_FL, bitOn(machine->regA, NEGATIVE_FL));
      machine->regA = machine->regA << 1;
      machine->regA = setBit(machine->regA, CARRY_FL, 0);
      manZeroNeg(machine,machine->regA);
  }  
  
}

static void jmpBIT(machine_6502 *machine, AddrMode adm){
  Pointer ptr;
  BOOL isValue = getValue(machine, adm, &ptr);
  warnValue(isValue);
  machine->regP = setBit(machine->regP, ZERO_FL, (ptr.value & machine->regA));
  machine->regP = setBit(machine->regP, OVERFLOW_FL, bitOn(ptr.value, OVERFLOW_FL));
  machine->regP = setBit(machine->regP, NEGATIVE_FL, bitOn(ptr.value, NEGATIVE_FL));
  
}

static void jumpBranch(machine_6502 *machine, Bit16 offset){
  if ( offset > 0x7f )
    machine->regPC = machine->regPC - (0x100 - offset);
  else
    machine->regPC = machine->regPC + offset;
}

static void jmpBPL(machine_6502 *machine, AddrMode adm){
  Pointer ptr;
  BOOL isValue = getValue(machine, adm, &ptr);
  warnValue(isValue);
  if (bitOff(machine->regP,NEGATIVE_FL))
    jumpBranch(machine, ptr.addr);
    
}

static void jmpBMI(machine_6502 *machine, AddrMode adm){
  Pointer ptr;
  BOOL isValue = getValue(machine, adm, &ptr);
  warnValue(isValue);
  if (bitOn(machine->regP,NEGATIVE_FL))
    jumpBranch(machine, ptr.addr);

}

static void jmpBVC(machine_6502 *machine, AddrMode adm){
  Pointer ptr;
  BOOL isValue = getValue(machine, adm, &ptr);
  warnValue(isValue);
  if (bitOff(machine->regP,OVERFLOW_FL))
    jumpBranch(machine, ptr.addr);
}

static void jmpBVS(machine_6502 *machine, AddrMode adm){
  Pointer ptr;
  BOOL isValue = getValue(machine, adm, &ptr);
  warnValue(isValue);
  if (bitOn(machine->regP,OVERFLOW_FL))
    jumpBranch(machine, ptr.addr);
}

static void jmpBCC(machine_6502 *machine, AddrMode adm){
  Pointer ptr;
  BOOL isValue = getValue(machine, adm, &ptr);
  warnValue(isValue);
  if (bitOff(machine->regP,CARRY_FL))
    jumpBranch(machine, ptr.addr);
}

static void jmpBCS(machine_6502 *machine, AddrMode adm){
  Pointer ptr;
  BOOL isValue = getValue(machine, adm, &ptr);
  warnValue(isValue);
  if (bitOn(machine->regP,CARRY_FL))
    jumpBranch(machine, ptr.addr);
}

static void jmpBNE(machine_6502 *machine, AddrMode adm){
  Pointer ptr;
  BOOL isValue = getValue(machine, adm, &ptr);
  warnValue(isValue);
  if (bitOff(machine->regP, ZERO_FL))
    jumpBranch(machine, ptr.addr);
}

static void jmpBEQ(machine_6502 *machine, AddrMode adm){
  Pointer ptr;
  BOOL isValue = getValue(machine, adm, &ptr);
  warnValue(isValue);
  if (bitOn(machine->regP, ZERO_FL))
    jumpBranch(machine, ptr.addr);
}

static void doCompare(machine_6502 *machine, Bit16 reg, Pointer *ptr){
  machine->regP = setBit(machine->regP,CARRY_FL, ((reg + ptr->value) > 0xff));
  manZeroNeg(machine,(reg - ptr->value));
}

static void jmpCMP(machine_6502 *machine, AddrMode adm){
  Pointer ptr;
  BOOL isValue = getValue(machine, adm, &ptr);
  warnValue(isValue);
  doCompare(machine,machine->regA,&ptr);
}

static void jmpCPX(machine_6502 *machine, AddrMode adm){
  Pointer ptr;
  BOOL isValue = getValue(machine, adm, &ptr);
  warnValue(isValue);
  doCompare(machine,machine->regX,&ptr);
}

static void jmpCPY(machine_6502 *machine, AddrMode adm){
  Pointer ptr;
  BOOL isValue = getValue(machine, adm, &ptr);
  warnValue(isValue);
  doCompare(machine,machine->regY,&ptr);
}

static void jmpDEC(machine_6502 *machine, AddrMode adm){
  Pointer ptr;
  BOOL isValue = getValue(machine, adm, &ptr);
  warnValue(isValue);
  if (ptr.value > 0)
    ptr.value--;
  else
    ptr.value = 0xFF;
  memStoreByte(machine, ptr.addr, ptr.value);
  manZeroNeg(machine,ptr.value);
}

static void jmpEOR(machine_6502 *machine, AddrMode adm){
  Pointer ptr;
  BOOL isValue = getValue(machine, adm, &ptr);
  warnValue(isValue);
  machine->regA ^= ptr.value;
  manZeroNeg(machine, machine->regA);
}

static void jmpCLC(machine_6502 *machine, AddrMode adm){
  machine->regP = setBit(machine->regP, CARRY_FL, 0);
}

static void jmpSEC(machine_6502 *machine, AddrMode adm){
  machine->regP = setBit(machine->regP, CARRY_FL, 1);
}

static void jmpCLI(machine_6502 *machine, AddrMode adm){
  machine->regP = setBit(machine->regP, INTERRUPT_FL, 0);
}

static void jmpSEI(machine_6502 *machine, AddrMode adm){
  machine->regP = setBit(machine->regP, INTERRUPT_FL, 1);
}

static void jmpCLV(machine_6502 *machine, AddrMode adm){
  machine->regP = setBit(machine->regP, OVERFLOW_FL, 0);
}

static void jmpCLD(machine_6502 *machine, AddrMode adm){
  machine->regP = setBit(machine->regP, DECIMAL_FL, 0);
}

static void jmpSED(machine_6502 *machine, AddrMode adm){
  machine->regP = setBit(machine->regP, DECIMAL_FL, 1);
}

static void jmpINC(machine_6502 *machine, AddrMode adm){
  Pointer ptr;
  BOOL isValue = getValue(machine, adm, &ptr);
  warnValue(isValue);
  ptr.value = (ptr.value + 1) & 0xFF;
  memStoreByte(machine, ptr.addr, ptr.value);
  manZeroNeg(machine,ptr.value);
}

static void jmpJMP(machine_6502 *machine, AddrMode adm){
  Pointer ptr;
  BOOL isValue = getValue(machine, adm, &ptr);
  warnValue(isValue);
  machine->regPC = ptr.addr;
}

static void jmpJSR(machine_6502 *machine, AddrMode adm){
  Pointer ptr;
  /* Move past the 2 byte parameter. JSR is always followed by
     absolute address. */
  Bit16 currAddr = machine->regPC + 2;
  BOOL isValue = getValue(machine, adm, &ptr);
  warnValue(isValue);
  stackPush(machine, (currAddr >> 8) & 0xff);
  stackPush(machine, currAddr & 0xff);
  machine->regPC = ptr.addr;  
}

static void jmpLDA(machine_6502 *machine, AddrMode adm){
  Pointer ptr;
  BOOL isValue = getValue(machine, adm, &ptr);
  warnValue(isValue);
  machine->regA = ptr.value;
  manZeroNeg(machine, machine->regA);
}

static void jmpLDX(machine_6502 *machine, AddrMode adm){
  Pointer ptr;
  BOOL isValue = getValue(machine, adm, &ptr);
  warnValue(isValue);
  machine->regX = ptr.value;
  manZeroNeg(machine, machine->regX);
}

static void jmpLDY(machine_6502 *machine, AddrMode adm){
  Pointer ptr;
  BOOL isValue = getValue(machine, adm, &ptr);
  warnValue(isValue);
  machine->regY = ptr.value;
  manZeroNeg(machine, machine->regY);
}

static void jmpLSR(machine_6502 *machine, AddrMode adm){
  Pointer ptr;
  BOOL isValue = getValue(machine, adm, &ptr);
  if (isValue){
    machine->regP = 
      setBit(machine->regP, CARRY_FL, 
	     bitOn(ptr.value, CARRY_FL));
    ptr.value = ptr.value >> 1;
    ptr.value = setBit(ptr.value,NEGATIVE_FL,0);
    memStoreByte(machine,ptr.addr,ptr.value);
    manZeroNeg(machine,ptr.value);
  }
  else { /* Accumulator */
    machine->regP = 
      setBit(machine->regP, CARRY_FL, 
	     bitOn(machine->regA, CARRY_FL));
    machine->regA = machine->regA >> 1;
    machine->regA = setBit(machine->regA,NEGATIVE_FL,0);
    manZeroNeg(machine,ptr.value);
  }
}

static void jmpNOP(machine_6502 *machine, AddrMode adm){
  /* no operation */
}

static void jmpORA(machine_6502 *machine, AddrMode adm){
  Pointer ptr;
  BOOL isValue = getValue(machine, adm, &ptr);
  warnValue(isValue);
  machine->regA |= ptr.value;
  manZeroNeg(machine,machine->regA);
}

static void jmpTAX(machine_6502 *machine, AddrMode adm){
  machine->regX = machine->regA;
  manZeroNeg(machine,machine->regX);
}

static void jmpTXA(machine_6502 *machine, AddrMode adm){
  machine->regA = machine->regX;
  manZeroNeg(machine,machine->regA);
}

static void jmpDEX(machine_6502 *machine, AddrMode adm){
  if (machine->regX > 0)
    machine->regX--;
  else
    machine->regX = 0xFF;
  manZeroNeg(machine, machine->regX);
}

static void jmpINX(machine_6502 *machine, AddrMode adm){
  Bit16 value = machine->regX + 1;
  machine->regX = value & 0xFF;
  manZeroNeg(machine, machine->regX);
}

static void jmpTAY(machine_6502 *machine, AddrMode adm){
  machine->regY = machine->regA;
  manZeroNeg(machine, machine->regY);
}

static void jmpTYA(machine_6502 *machine, AddrMode adm){
  machine->regA = machine->regY;
  manZeroNeg(machine, machine->regA);
}

static void jmpDEY(machine_6502 *machine, AddrMode adm){
  if (machine->regY > 0)
    machine->regY--;
  else
    machine->regY = 0xFF;
  manZeroNeg(machine, machine->regY);
}

static void jmpINY(machine_6502 *machine, AddrMode adm){
  Bit16 value = machine->regY + 1;
  machine->regY = value & 0xff;
  manZeroNeg(machine, machine->regY);
}

static void jmpROR(machine_6502 *machine, AddrMode adm){
  Pointer ptr;
  Bit8 cf;
  BOOL isValue = getValue(machine, adm, &ptr);
  if (isValue) { 
    cf = bitOn(machine->regP, CARRY_FL);
    machine->regP = 
      setBit(machine->regP, CARRY_FL,
	     bitOn(ptr.value, CARRY_FL));
    ptr.value = ptr.value >> 1;
    ptr.value = setBit(ptr.value, NEGATIVE_FL, cf);
    memStoreByte(machine, ptr.addr, ptr.value);
    manZeroNeg(machine, ptr.value);
  }
  else { /* Implied */
    cf = bitOn(machine->regP, CARRY_FL);
    machine->regP = 
      setBit(machine->regP, CARRY_FL,
	     bitOn(machine->regA, CARRY_FL));
    machine->regA = machine->regA >> 1;
    machine->regA = setBit(machine->regA, NEGATIVE_FL, cf);
    manZeroNeg(machine, machine->regA);
  }
}

static void jmpROL(machine_6502 *machine, AddrMode adm){
  Pointer ptr;
  Bit8 cf;
  BOOL isValue = getValue(machine, adm, &ptr);
  if (isValue) { 
    cf = bitOn(machine->regP, CARRY_FL);
    machine->regP = 
      setBit(machine->regP, CARRY_FL,
	     bitOn(ptr.value, NEGATIVE_FL));
    ptr.value = ptr.value << 1;
    ptr.value = setBit(ptr.value, CARRY_FL, cf);
    memStoreByte(machine, ptr.addr, ptr.value);
    manZeroNeg(machine, ptr.value);
  }
  else { /* Implied */
    cf = bitOn(machine->regP, CARRY_FL);
    machine->regP = 
      setBit(machine->regP, CARRY_FL,
	     bitOn(machine->regA,NEGATIVE_FL));
    machine->regA = machine->regA << 1;
    machine->regA = setBit(machine->regA, CARRY_FL, cf);
    manZeroNeg(machine, machine->regA);
  }
}

static void jmpRTI(machine_6502 *machine, AddrMode adm){
  machine->regP = stackPop(machine);
  machine->regPC = stackPop(machine);
}

static void jmpRTS(machine_6502 *machine, AddrMode adm){
  Pointer ptr;
  BOOL isValue = getValue(machine, adm, &ptr);
  Bit16 nr = stackPop(machine);
  Bit16 nl = stackPop(machine);
  warnValue(! isValue);
  machine->regPC = (nl << 8) | nr;
}

static void jmpSBC(machine_6502 *machine, AddrMode adm){
  Pointer ptr;
  Bit8 vflag;
  Bit8 c = bitOn(machine->regP, CARRY_FL);
  Bit16 tmp, w;
  BOOL isValue = getValue(machine, adm, &ptr);
  warnValue(isValue);
  vflag = (bitOn(machine->regA,NEGATIVE_FL) &&
	   bitOn(ptr.value, NEGATIVE_FL));

  if (bitOn(machine->regP, DECIMAL_FL)) {
    Bit8 ar = nibble(machine->regA, RIGHT);
    Bit8 br = nibble(ptr.value, RIGHT);
    Bit8 al = nibble(machine->regA, LEFT);
    Bit8 bl = nibble(ptr.value, LEFT);

    tmp = 0xf + ar - br + c;
    if ( tmp < 0x10){
      w = 0;
      tmp -= 6;
    }
    else {
      w = 0x10;
      tmp -= 0x10;
    }
    w += 0xf0 + al - bl;
    if ( w < 0x100) {
      machine->regP = setBit(machine->regP, CARRY_FL, 0);
      if (bitOn(machine->regP, OVERFLOW_FL) && w < 0x80)
	machine->regP = setBit(machine->regP, OVERFLOW_FL, 0);
      w -= 0x60;
    }
    else {
      machine->regP = setBit(machine->regP, CARRY_FL, 1);
      if (bitOn(machine->regP, OVERFLOW_FL) && w >= 0x180)
	machine->regP = setBit(machine->regP, OVERFLOW_FL, 0);
    }
    w += tmp;
  } /* end decimal mode */
  else {
    w = 0xff + machine->regA - ptr.value + c;
    if ( w < 0x100 ){
      machine->regP = setBit(machine->regP, CARRY_FL, 0);
      if (bitOn(machine->regP, OVERFLOW_FL) && w < 0x80)
	machine->regP = setBit(machine->regP, OVERFLOW_FL, 0);
    }
    else {
      machine->regP = setBit(machine->regP, CARRY_FL, 1);
      if (bitOn(machine->regP, OVERFLOW_FL) && w >= 0x180)
	machine->regP = setBit(machine->regP, OVERFLOW_FL, 0);
    }
  }
  machine->regA = w;
  manZeroNeg(machine,machine->regA);
}

static void jmpSTA(machine_6502 *machine, AddrMode adm){
  Pointer ptr;
  BOOL isValue = getValue(machine, adm, &ptr);
  warnValue(isValue);
  memStoreByte(machine,ptr.addr,machine->regA);
}

static void jmpTXS(machine_6502 *machine, AddrMode adm){
  stackPush(machine,machine->regX);
}

static void jmpTSX(machine_6502 *machine, AddrMode adm){
  machine->regX = stackPop(machine);
  manZeroNeg(machine, machine->regX);
}

static void jmpPHA(machine_6502 *machine, AddrMode adm){
  stackPush(machine, machine->regA);
}

static void jmpPLA(machine_6502 *machine, AddrMode adm){
  machine->regA = stackPop(machine);
  manZeroNeg(machine, machine->regA);
}

static void jmpPHP(machine_6502 *machine, AddrMode adm){
  stackPush(machine,machine->regP);
}

static void jmpPLP(machine_6502 *machine, AddrMode adm){
  machine->regP = stackPop(machine);
  machine->regP = setBit(machine->regP, FUTURE_FL, 1);
}

static void jmpSTX(machine_6502 *machine, AddrMode adm){
  Pointer ptr;
  BOOL isValue = getValue(machine, adm, &ptr);
  warnValue(isValue);
  memStoreByte(machine,ptr.addr,machine->regX);
}

static void jmpSTY(machine_6502 *machine, AddrMode adm){
  Pointer ptr;
  BOOL isValue = getValue(machine, adm, &ptr);
  warnValue(isValue);
  memStoreByte(machine,ptr.addr,machine->regY);
}



/* OPCODES */
static void assignOpCodes(Opcodes *opcodes){

 #define SETOP(num, _name, _Imm, _ZP, _ZPX, _ZPY, _ABS, _ABSX, _ABSY, _INDX, _INDY, _SNGL, _BRA, _func) \
{opcodes[num].name[3] = '\0'; \
 strncpy(opcodes[num].name, _name, 3); opcodes[num].Imm = _Imm; opcodes[num].ZP = _ZP; \
 opcodes[num].ZPX = _ZPX; opcodes[num].ZPY = _ZPY; opcodes[num].ABS = _ABS; \
 opcodes[num].ABSX = _ABSX; opcodes[num].ABSY = _ABSY; opcodes[num].INDX = _INDX; \
 opcodes[num].INDY = _INDY; opcodes[num].SNGL = _SNGL; opcodes[num].BRA = _BRA; \
 opcodes[num].func = _func;}

  /*        OPCODE Imm   ZP    ZPX   ZPY   ABS   ABSX  ABSY  INDX  INDY  SGNL  BRA   Jump Function*/ 
  SETOP( 0, "ADC", 0x69, 0x65, 0x75, 0x00, 0x6d, 0x7d, 0x79, 0x61, 0x71, 0x00, 0x00, jmpADC);
  SETOP( 1, "AND", 0x29, 0x25, 0x35, 0x31, 0x2d, 0x3d, 0x39, 0x00, 0x00, 0x00, 0x00, jmpAND);
  SETOP( 2, "ASL", 0x00, 0x06, 0x16, 0x00, 0x0e, 0x1e, 0x00, 0x00, 0x00, 0x0a, 0x00, jmpASL);
  SETOP( 3, "BIT", 0x00, 0x24, 0x00, 0x00, 0x2c, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, jmpBIT);
  SETOP( 4, "BPL", 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x10, jmpBPL);
  SETOP( 5, "BMI", 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x30, jmpBMI);
  SETOP( 6, "BVC", 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x50, jmpBVC);
  SETOP( 7, "BVS", 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x70, jmpBVS);
  SETOP( 8, "BCC", 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x90, jmpBCC);
  SETOP( 9, "BCS", 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xb0, jmpBCS);
  SETOP(10, "BNE", 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xd0, jmpBNE);
  SETOP(11, "BEQ", 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xf0, jmpBEQ);
  SETOP(12, "CMP", 0xc9, 0xc5, 0xd5, 0x00, 0xcd, 0xdd, 0xd9, 0xc1, 0xd1, 0x00, 0x00, jmpCMP);
  SETOP(13, "CPX", 0xe0, 0xe4, 0x00, 0x00, 0xec, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, jmpCPX);
  SETOP(14, "CPY", 0xc0, 0xc4, 0x00, 0x00, 0xcc, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, jmpCPY);
  SETOP(15, "DEC", 0x00, 0xc6, 0xd6, 0x00, 0xce, 0xde, 0x00, 0x00, 0x00, 0x00, 0x00, jmpDEC);
  SETOP(16, "EOR", 0x49, 0x45, 0x55, 0x00, 0x4d, 0x5d, 0x59, 0x41, 0x51, 0x00, 0x00, jmpEOR);
  SETOP(17, "CLC", 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x18, 0x00, jmpCLC);
  SETOP(18, "SEC", 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x38, 0x00, jmpSEC);
  SETOP(19, "CLI", 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x58, 0x00, jmpCLI);
  SETOP(20, "SEI", 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x78, 0x00, jmpSEI);
  SETOP(21, "CLV", 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xb8, 0x00, jmpCLV);
  SETOP(22, "CLD", 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xd8, 0x00, jmpCLD);
  SETOP(23, "SED", 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xf8, 0x00, jmpSED);
  SETOP(24, "INC", 0x00, 0xe6, 0xf6, 0x00, 0xee, 0xfe, 0x00, 0x00, 0x00, 0x00, 0x00, jmpINC);
  SETOP(25, "JMP", 0x00, 0x00, 0x00, 0x00, 0x4c, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, jmpJMP);
  SETOP(26, "JSR", 0x00, 0x00, 0x00, 0x00, 0x20, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, jmpJSR);
  SETOP(27, "LDA", 0xa9, 0xa5, 0xb5, 0x00, 0xad, 0xbd, 0xb9, 0xa1, 0xb1, 0x00, 0x00, jmpLDA);
  SETOP(28, "LDX", 0xa2, 0xa6, 0x00, 0xb6, 0xae, 0x00, 0xbe, 0x00, 0x00, 0x00, 0x00, jmpLDX);
  SETOP(29, "LDY", 0xa0, 0xa4, 0xb4, 0x00, 0xac, 0xbc, 0x00, 0x00, 0x00, 0x00, 0x00, jmpLDY);
  SETOP(30, "LSR", 0x00, 0x46, 0x56, 0x00, 0x4e, 0x5e, 0x00, 0x00, 0x00, 0x4a, 0x00, jmpLSR);
  SETOP(31, "NOP", 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xea, 0x00, jmpNOP);
  SETOP(32, "ORA", 0x09, 0x05, 0x15, 0x00, 0x0d, 0x1d, 0x19, 0x01, 0x11, 0x00, 0x00, jmpORA);
  SETOP(33, "TAX", 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xaa, 0x00, jmpTAX);
  SETOP(34, "TXA", 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x8a, 0x00, jmpTXA);
  SETOP(35, "DEX", 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xca, 0x00, jmpDEX);
  SETOP(36, "INX", 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xe8, 0x00, jmpINX);
  SETOP(37, "TAY", 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xa8, 0x00, jmpTAY);
  SETOP(38, "TYA", 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x98, 0x00, jmpTYA);
  SETOP(39, "DEY", 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x88, 0x00, jmpDEY);
  SETOP(40, "INY", 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xc8, 0x00, jmpINY);
  SETOP(41, "ROR", 0x00, 0x66, 0x76, 0x00, 0x6e, 0x7e, 0x00, 0x00, 0x00, 0x6a, 0x00, jmpROR);
  SETOP(42, "ROL", 0x00, 0x26, 0x36, 0x00, 0x2e, 0x3e, 0x00, 0x00, 0x00, 0x2a, 0x00, jmpROL);
  SETOP(43, "RTI", 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x40, 0x00, jmpRTI);
  SETOP(44, "RTS", 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x60, 0x00, jmpRTS);
  SETOP(45, "SBC", 0xe9, 0xe5, 0xf5, 0x00, 0xed, 0xfd, 0xf9, 0xe1, 0xf1, 0x00, 0x00, jmpSBC);
  SETOP(46, "STA", 0x00, 0x85, 0x95, 0x00, 0x8d, 0x9d, 0x99, 0x81, 0x91, 0x00, 0x00, jmpSTA);
  SETOP(47, "TXS", 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x9a, 0x00, jmpTXS);
  SETOP(48, "TSX", 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xba, 0x00, jmpTSX);
  SETOP(49, "PHA", 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x48, 0x00, jmpPHA);
  SETOP(50, "PLA", 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x68, 0x00, jmpPLA);
  SETOP(51, "PHP", 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x08, 0x00, jmpPHP);
  SETOP(52, "PLP", 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x28, 0x00, jmpPLP);
  SETOP(53, "STX", 0x00, 0x86, 0x00, 0x96, 0x8e, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, jmpSTX);
  SETOP(54, "STY", 0x00, 0x84, 0x94, 0x00, 0x8c, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, jmpSTY);
  SETOP(55, "---", 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, NULL);
}

static void buildIndexCache(machine_6502 *machine){
  unsigned int i;
  for (i = 0; i < NUM_OPCODES; i++) {
    if (machine->opcodes[i].Imm != 0x00){
      machine->opcache[machine->opcodes[i].Imm].adm = IMMEDIATE_VALUE;
      machine->opcache[machine->opcodes[i].Imm].index = i;
    }
     if (machine->opcodes[i].ZP != 0x00){
      machine->opcache[machine->opcodes[i].ZP].adm = ZERO;
      machine->opcache[machine->opcodes[i].ZP].index = i;
    }
     if (machine->opcodes[i].ZPX != 0x00){
      machine->opcache[machine->opcodes[i].ZPX].adm = ZERO_X;
      machine->opcache[machine->opcodes[i].ZPX].index = i;;
    }
     if (machine->opcodes[i].ZPY != 0x00){
      machine->opcache[machine->opcodes[i].ZPY].adm = ZERO_Y;
      machine->opcache[machine->opcodes[i].ZPY].index = i;;
    }
     if (machine->opcodes[i].ABS != 0x00){
      machine->opcache[machine->opcodes[i].ABS].adm = ABS_VALUE;
      machine->opcache[machine->opcodes[i].ABS].index = i;;
    }
     if (machine->opcodes[i].ABSX != 0x00){
      machine->opcache[machine->opcodes[i].ABSX].adm = ABS_X;
      machine->opcache[machine->opcodes[i].ABSX].index = i;;
    }
     if (machine->opcodes[i].ABSY != 0x00){
      machine->opcache[machine->opcodes[i].ABSY].adm = ABS_Y;
      machine->opcache[machine->opcodes[i].ABSY].index = i;;
    }
     if (machine->opcodes[i].INDX != 0x00){
      machine->opcache[machine->opcodes[i].INDX].adm = INDIRECT_X;
      machine->opcache[machine->opcodes[i].INDX].index = i;;
    }
     if (machine->opcodes[i].INDY != 0x00){
      machine->opcache[machine->opcodes[i].INDY].adm = INDIRECT_Y;
      machine->opcache[machine->opcodes[i].INDY].index = i;;
    }
     if (machine->opcodes[i].SNGL != 0x00){
      machine->opcache[machine->opcodes[i].SNGL].adm = SINGLE;
      machine->opcache[machine->opcodes[i].SNGL].index = i;
    }
     if (machine->opcodes[i].BRA != 0x00){
      machine->opcache[machine->opcodes[i].BRA].adm = ABS_OR_BRANCH;
      machine->opcache[machine->opcodes[i].BRA].index = i;
    }
  }   
}

/* opIndex() - Search the opcode table for a match. If found return
   the index into the optable and the address mode of the opcode. If
   the opcode is not found then return -1. */
static int opIndex(machine_6502 *machine, Bit8 opcode, AddrMode *adm){ 
  /* XXX could catch errors by setting a addressmode of error or something */
  *adm = machine->opcache[opcode].adm;
  return machine->opcache[opcode].index;
}


/* Assembly parser */

static Param *newParam(void){
  Param *newp;
  int i = 0;

  newp = (Param *) emalloc(sizeof(Param));
  newp->type = SINGLE;
  for (i = 0; i < MAX_PARAM_VALUE; i++)
    newp->value[i] = 0;
  newp->vp = 0;
  newp->label = ecalloc(MAX_LABEL_LEN,sizeof(char));
  newp->lbladdr = 0;
  return newp;
}

/* Copy the fields from p2 to p1 */
static void copyParam(Param *p1, Param *p2){
  int i = 0;
  strncpy(p1->label,p2->label,MAX_LABEL_LEN);
  for(i = 0; i < MAX_PARAM_VALUE; i++)
    p1->value[i] = p2->value[i];
  p1->vp = p2->vp;
  p1->type = p2->type;
}

static Label *newLabel(void){
  Label *newp; 

  newp = (Label *) emalloc(sizeof(Label));
  newp->addr = 0;
  newp->label = ecalloc(MAX_LABEL_LEN,sizeof(char));
  
  return newp;
}

static AsmLine *newAsmLine(char *cmd, char *label, BOOL decl, Param *param, int lc)
{
    AsmLine *newp;

    newp =  (AsmLine *) emalloc(sizeof(AsmLine));
    newp->labelDecl = decl;
    newp->label = newLabel();
    strncpy(newp->label->label,label,MAX_LABEL_LEN);
    newp->command = estrdup(cmd);
    newp->param = newParam();
    copyParam(newp->param, param);
    newp->next = NULL;
    return newp;
}

static AsmLine *addend(AsmLine *listp, AsmLine *newp)
{
    AsmLine *p;
    if(listp == NULL)
      return newp;
    for (p =listp; p->next != NULL; p = p->next)
      ;
    p->next = newp;
    return listp;
}

static BOOL apply(AsmLine *listp, BOOL(*fn)(AsmLine*, void*), void *arg)
{
  AsmLine *p;
  if(listp == NULL)
    return FALSE;
  for (p = listp; p != NULL; p = p->next)
    if (! fn(p,arg) )
      return FALSE;
  return TRUE;
}

static void freeParam(Param *param){
  free(param->label);
  free(param);
}

static void freeLabel(Label *label){
  free(label->label);
  free(label);
}

static void freeallAsmLine(AsmLine *listp)
{
    AsmLine *next;
    for(; listp != NULL; listp = next){
       next = listp->next;
       freeParam(listp->param);
       freeLabel(listp->label);
       free(listp->command);
       free(listp);
    }
}

static BOOL addvalue(Param *param,Bit32 value){
  if (0 <= param->vp && param->vp < MAX_PARAM_VALUE) {
    param->value[param->vp++] = value;
    return TRUE;
  }
  else {
    fprintf(stderr,"Wrong number of parameters: %d. The limit is %d\n",param->vp+1, MAX_PARAM_VALUE);
    return FALSE;
  }
}

static void parseError(char *s){
  fprintf(stderr,"6502 Syntax Error: %s\n", s);
}

/* stoupper() - Destructivley modifies the string making all letters upper case*/
static void stoupper(char **s){
  int i = 0;
  while((*s)[i] != '\0'){
    (*s)[i] = toupper((*s)[i]);
    i++;
  }
}
 
static BOOL isWhite(char c){
  return (c == '\r' || c == '\t' || c == ' ');
}

static void skipSpace(char **s){
  for(; isWhite(**s); (*s)++)
    ;
}
  
/* nullify() - fills a string with upto sourceLength null characters. */
static void nullify(char *token, unsigned int sourceLength){
  unsigned int i = 0;
  while (i < sourceLength)
    token[i++] = '\0';
}

static BOOL isBlank(const char *token){
  return (token[0] == '\0');
}

static BOOL isCommand(machine_6502 *machine, const char *token){
  int i = 0;

  while (i < NUM_OPCODES) {
    if (strcmp(machine->opcodes[i].name,token) == 0) 
      return TRUE;
    i++;
  }
  
  if (strcmp(token, "DCB") == 0) return TRUE;
  return FALSE;
}

/* hasChar() - Check to see if the current line has a certain
   charater */
static BOOL hasChar(char *s, char c){
  for(; *s != '\0' && *s != '\n'; s++) {
    if (*s  == c)
      return TRUE;
  }
  return FALSE;
}

static BOOL ishexdigit(char c){
  if (isdigit(c))
    return TRUE;
  else {
    char c1 = toupper(c);
    return ('A' <= c1 && c1 <= 'F');
  }
}

/* isCmdChar() - Is this a valid character for a command. All of the
   command are alpha except for the entry point code that is "*=" */
static BOOL isCmdChar(char c){
  return (isalpha(c) || c == '*' || c == '=');
}
  

/* command() - parse a command from the source code. We pass along a
   machine so the opcode can be validated. */
static BOOL command(machine_6502 *machine, char **s, char **cmd){
  int i = 0;
  skipSpace(s);
  for(;isCmdChar(**s) && i < MAX_CMD_LEN; (*s)++)
    (*cmd)[i++] = **s;
  if (i == 0)
    return TRUE; /* Could be a blank line. */
  else if (strcmp(*cmd,"*=") == 0)
    return TRUE; /* This is an entry point. */
  else
    return isCommand(machine,*cmd);
}

static BOOL declareLabel(char **s, char **label){
  int i = 0;
  skipSpace(s);
  for(;**s != ':' && **s != '\n' && **s != '\0'; (*s)++){
    if (isWhite(**s)) 
      continue;
    (*label)[i++] = **s;
  }
  if (i == 0)
    return FALSE; /* Current line has to have a label */
  else if (**s == ':'){
    (*s)++; /* Skip colon */
    return TRUE;
  }
  else
    return FALSE;
}

static BOOL parseHex(char **s, Bit32 *value){
  enum { MAX_HEX_LEN = 5 };
  if (**s == '$') {    
    char *hex = ecalloc(MAX_HEX_LEN, sizeof(char));
    int i = 0;

    (*s)++; /* move pass $ */
    for(; ishexdigit(**s) && i < MAX_HEX_LEN; (*s)++)
      hex[i++] = **s;
    
    *value = strtol(hex,NULL,16);
    free(hex);  
    return TRUE;
  }
  else
    return FALSE;
}
  
static BOOL parseDec(char **s, Bit32 *value){
  enum { MAX_DEC_LEN = 4 };
  char *dec = ecalloc(MAX_DEC_LEN, sizeof(char));
  int i;
  for(i = 0; isdigit(**s) && i < MAX_DEC_LEN; (*s)++)
    dec[i++] = **s;
  
  if (i > 0){
    *value = atoi(dec);
    free(dec);  
    return TRUE;
  }
  else
    return FALSE;
}

static BOOL parseValue(char **s, Bit32 *value){
  skipSpace(s);
  if (**s == '$')
    return parseHex(s, value);
  else
    return parseDec(s, value);
}

static BOOL paramLabel(char **s, char **label){
  int i;
  for(i = 0; (isalnum(**s) || **s == '_') && i < MAX_LABEL_LEN; (*s)++)
    (*label)[i++] = **s;

  if (i > 0)
    return TRUE;
  else
    return FALSE;
}

static BOOL immediate(char **s, Param *param){
  if (**s != '#') 
    return FALSE;

  (*s)++; /*Move past hash */
  if (**s == '<' || **s == '>'){    
    char *label = ecalloc(MAX_LABEL_LEN, sizeof(char));
    param->type = (**s == '<') ? IMMEDIATE_LESS : IMMEDIATE_GREAT;
    (*s)++; /* move past < or > */
    if (paramLabel(s, &label)){
      int ln = strlen(label) + 1;
      strncpy(param->label, label, ln);
      free(label);
      return TRUE;
    }    
    free(label);
  }
  else {
    Bit32 value;
    if (parseValue(s, &value)){
      if (value > 0xFF){
	parseError("Immediate value is too large.");
	return FALSE;
      }
      param->type = IMMEDIATE_VALUE;
      return addvalue(param, value);
    }
  }
  return FALSE;
}

static BOOL isDirection(char c){
  return (c == 'X' || c == 'Y');
}

static BOOL getDirection(char **s, char *direction){
  skipSpace(s);
  if (**s == ','){
    (*s)++;
    skipSpace(s);
    if (isDirection(**s)){
      *direction = **s;
      (*s)++;
      return TRUE;
    }
  }
  return FALSE;
}
  
static BOOL indirect(char **s, Param *param){
  Bit32 value;
  char c;
  if (**s == '(') 
    (*s)++;
  else
    return FALSE;
  
  if (! parseHex(s,&value)) 
    return FALSE;
  if (value > 0xFF) {
    parseError("Indirect value is too large.");
    return FALSE;
  }
  if (!addvalue(param, value))
    return FALSE;
  skipSpace(s);
  if (**s == ')'){
    (*s)++;
    if (getDirection(s,&c)) {
      if (c == 'Y'){
	param->type = INDIRECT_Y;
	return TRUE;
      }
    }
  }
  else if (getDirection(s, &c)){
    if (c == 'X'){
      skipSpace(s);
      if (**s == ')'){
	(*s)++;
	param->type = INDIRECT_X;
	return TRUE;
      }
    }
  }
  return FALSE;
}

static BOOL dcbValue(char **s, Param *param){
  Bit32 val;
  if (! parseValue(s,&val))
    return FALSE;

  if (val > 0xFF) 
    return FALSE;
		    
  if (!addvalue(param,val))
    return FALSE;

  param->type = DCB_PARAM;

  skipSpace(s);
  if(**s == ','){
    (*s)++;
    return dcbValue(s, param);
  }
  else
    return TRUE;
} 

static BOOL value(char **s, Param *param){
  Bit32 val;
  BOOL abs;
  BOOL dir;
  char c = '\0';
  if (! parseValue(s,&val))
    return FALSE;

  abs = (val > 0xFF);
  dir = getDirection(s,&c);
  if (!addvalue(param,val))
    return FALSE;

  if(abs && dir){
    if (c == 'X')
      param->type = ABS_X;
    else if (c == 'Y')
      param->type = ABS_Y;
    else
      return FALSE;
  }
  else if (abs)
    param->type = ABS_VALUE;
  else if (dir){
    if (c == 'X')
      param->type = ZERO_X;
    else if (c == 'Y')
      param->type = ZERO_Y;
    else
      return FALSE;
  }
  else
    param->type = ZERO;

  return TRUE;
}

static BOOL label(char **s, Param *param){
  char *label = ecalloc(MAX_LABEL_LEN, sizeof(char));
  char c;
  BOOL labelOk = FALSE;
  if (paramLabel(s, &label)){
    labelOk = TRUE;
    param->type = ABS_OR_BRANCH;
    if (getDirection(s, &c)){
      if (c == 'X')
	param->type = ABS_LABEL_X;
      else if (c == 'Y')
	param->type = ABS_LABEL_Y;
      else
	labelOk = FALSE;
    }
    strncpy(param->label,label,MAX_LABEL_LEN);
  }
  free(label);
  return labelOk;
}

static BOOL parameter(const char *cmd, char **s, Param *param){
  skipSpace(s);
  if (**s == '\0' || **s == '\n')
    return TRUE;
  else if (**s == '#')
    return immediate(s,param);
  else if (**s == '(')
    return indirect(s,param);
  else if (**s == '$' || isdigit(**s)){
    if (strcmp(cmd, "DCB") == 0)
      return dcbValue(s,param);
    else
      return value(s,param);
  }
  else if (isalpha(**s))
    return label(s ,param);
  else
    return FALSE; /* Invalid Parameter */
}

static void comment(char **s){
  skipSpace(s);
  if (**s == ';')
    for(;**s != '\n' && **s != '\0'; (*s)++)
      ;
}

static void initParam(Param *param){
  int i;
  param->type = SINGLE;
  for(i = 0; i < MAX_PARAM_VALUE; i++)
    param->value[i] = 0;
  param->vp = 0;
  nullify(param->label,MAX_LABEL_LEN);
}
  

static AsmLine *parseAssembly(machine_6502 *machine, BOOL *codeOk, const char *code){
  char *s;
  char *cmd = ecalloc(MAX_CMD_LEN, sizeof(char));
  char *label = ecalloc(MAX_LABEL_LEN, sizeof(char));
  char *start; /*pointer to the start of the code.*/
  unsigned int lc = 1;
  Param *param;
  BOOL decl;
  AsmLine *listp = NULL;

  *codeOk = TRUE;
  param = newParam();
  s = estrdup(code);
  start = s;
  stoupper(&s);

  while(*s != '\0' && *codeOk){
    initParam(param);
    nullify(cmd, MAX_CMD_LEN);
    nullify(label, MAX_LABEL_LEN);
    decl = FALSE;
    skipSpace(&s);
    comment(&s);
    if (*s == '\n'){
      lc++;
      s++;
      continue; /* blank line */
    }
    else if (*s == '\0')
      continue; /* no newline at the end of the code */
    else if (hasChar(s,':')){
      decl = TRUE;
      if(! declareLabel(&s,&label)){
	*codeOk = FALSE;
	break;
      }
      skipSpace(&s);
    }
    if(!command(machine, &s, &cmd)){
      *codeOk = FALSE;
      break;
    }
    skipSpace(&s);
    comment(&s);
    if(!parameter(cmd, &s, param)){
      *codeOk = FALSE;
      break;
    }
    skipSpace(&s);
    comment(&s);
    if (*s == '\n' || *s == '\0'){
      AsmLine *asmm;
      asmm = newAsmLine(cmd,label,decl,param,lc);
      listp = addend(listp,asmm);
    }
    else {
      *codeOk = FALSE;
      break;
    }
  }
  if (! *codeOk)
    fprintf(stderr,"Syntax error at line %u\n", lc);
  free(start);
  free(cmd);
  free(label);
  freeParam(param);
  return listp;
}
    
/* fileToBuffer() - Allocates a buffer and loads all of the file into memory. */
static char *fileToBuffer(const char *filename){
  const int defaultSize = 1024;
  FILE *ifp;
  int c;
  int size = defaultSize;
  int i = 0;
  char *buffer = ecalloc(defaultSize,sizeof(char));

  if (buffer == NULL) 
    eprintf("Could not allocate memory for buffer.");

  ifp = fopen(filename, "rb");
  if (ifp == NULL)
    eprintf("Could not open file.");

  while((c = getc(ifp)) != EOF){
    buffer[i++] = c;
    if (i == size){
      size += defaultSize;
      buffer = realloc(buffer, size);
      if (buffer == NULL) {
	fclose(ifp);
	eprintf("Could not resize buffer.");
      }
    }
  }
  fclose(ifp);
  buffer = realloc(buffer, i+2);
  if (buffer == NULL) 
    eprintf("Could not resize buffer.");
  /* Make sure we have a line feed at the end */
  buffer[i] = '\n';
  buffer[i+1] = '\0';
  return buffer;
}


/* Routines */

/* reset() - Reset CPU and memory. */
static void reset(machine_6502 *machine){
  int x, y;
  for ( y = 0; y < 32; y++ ){
    for (x = 0; x < 32; x++){
      machine->screen[x][y] = 0;
    }
  }

  for(x=0; x < MEM_64K; x++)
    machine->memory[x] = 0;

  machine->codeCompiledOK = FALSE;
  machine->regA = 0;
  machine->regX = 0;
  machine->regY = 0;
  machine->regP = setBit(machine->regP, FUTURE_FL, 1);
  machine->defaultCodePC = machine->regPC = PROG_START; 
  machine->regSP = STACK_TOP;
  machine->runForever = FALSE;
  machine->labelPtr = 0;
  machine->codeRunning = FALSE;
}

/* hexDump() - Dump the memory to output */
void hexDump(machine_6502 *machine, Bit16 start, Bit16 numbytes, FILE *output){
  Bit32 address;
  Bit32 i;
  for( i = 0; i < numbytes; i++){
    address = start + i;
    if ( (i&15) == 0 ) {
      fprintf(output,"\n%.4x: ", address);
    }
    fprintf(output,"%.2x%s",machine->memory[address], (i & 1) ? " ":"");
  }
  fprintf(output,"%s\n",(i&1)?"--":"");
}

/* XXX */
/* void save_program(machine_6502 *machine, char *filename){ */
/*   FILE *ofp; */
/*   Bit16 pc = PROG_START; */
/*   Bit16 end = pc + machine->codeLen; */
/*   Bit16 n; */
/*   ofp = fopen(filename, "w"); */
/*   if (ofp == NULL) */
/*     eprintf("Could not open file."); */
  
/*   fprintf(ofp,"Bit8 prog[%d] =\n{",machine->codeLen); */
/*   n = 1; */
/*   while(pc < end) */
/*     fprintf(ofp,"0x%.2x,%s",machine->memory[pc++],n++%10?" ":"\n"); */
/*   fseek(ofp,-2,SEEK_CUR); */
/*   fprintf(ofp,"};\n"); */
  
/*   fclose(ofp); */
/* } */

static BOOL translate(Opcodes *op,Param *param, machine_6502 *machine){
   switch(param->type){
    case SINGLE:
      if (op->SNGL)
	pushByte(machine, op->SNGL);
      else {
	fprintf(stderr,"%s needs a parameter.\n",op->name);
	return FALSE;
      }
      break;
    case IMMEDIATE_VALUE:
      if (op->Imm) {
	pushByte(machine, op->Imm);
	pushByte(machine, param->value[0]);
	break;
      }
      else {
	fprintf(stderr,"%s does not take IMMEDIATE_VALUE parameters.\n",op->name);
	return FALSE;
      }
    case IMMEDIATE_GREAT:
      if (op->Imm) {
	pushByte(machine, op->Imm);
	pushByte(machine, param->lbladdr >> 8);
	break;
      }
      else {
	fprintf(stderr,"%s does not take IMMEDIATE_GREAT parameters.\n",op->name);
	return FALSE;
      }
    case IMMEDIATE_LESS:
      if (op->Imm) {
	pushByte(machine, op->Imm);
	pushByte(machine, param->lbladdr & 0xFF);
	break;
      }
      else {
	fprintf(stderr,"%s does not take IMMEDIATE_LESS parameters.\n",op->name);
	return FALSE;
      }
    case INDIRECT_X:
      if (op->INDX) {
	pushByte(machine, op->INDX);
	pushByte(machine, param->value[0]);
	break;
      }
      else {
	fprintf(stderr,"%s does not take INDIRECT_X parameters.\n",op->name);
	return FALSE;
      }
    case INDIRECT_Y:
      if (op->INDY) {
	pushByte(machine, op->INDY);
	pushByte(machine, param->value[0]);
	break;
      }
      else {
	fprintf(stderr,"%s does not take INDIRECT_Y parameters.\n",op->name);
	return FALSE;
      }
    case ZERO:
      if (op->ZP) {
	pushByte(machine, op->ZP);
	pushByte(machine, param->value[0]);
	break;
      }
      else {
	fprintf(stderr,"%s does not take ZERO parameters.\n",op->name);
	return FALSE;
      }
    case ZERO_X:
      if (op->ZPX) {
	pushByte(machine, op->ZPX);
	pushByte(machine, param->value[0]);
	break;
      }
      else {
	fprintf(stderr,"%s does not take ZERO_X parameters.\n",op->name);
	return FALSE;
      }
    case ZERO_Y:
      if (op->ZPY) {
	pushByte(machine, op->ZPY);
	pushByte(machine, param->value[0]);
	break;
      }
      else {
	fprintf(stderr,"%s does not take ZERO_Y parameters.\n",op->name);
	return FALSE;
      }
    case ABS_VALUE:
      if (op->ABS) {
	pushByte(machine, op->ABS);
	pushWord(machine, param->value[0]);
	break;
      }
      else {
	fprintf(stderr,"%s does not take ABS_VALUE parameters.\n",op->name);
	return FALSE;
      }
    case ABS_OR_BRANCH:
      if (op->ABS > 0){
	pushByte(machine, op->ABS);
	pushWord(machine, param->lbladdr);
      }
      else {
	if (op->BRA) {
	  pushByte(machine, op->BRA);
          {
            int diff = abs(param->lbladdr - machine->defaultCodePC);
            int backward = (param->lbladdr < machine->defaultCodePC);
            pushByte(machine, (backward) ? 0xff - diff : diff - 1);
          }
	}
	else {
	  fprintf(stderr,"%s does not take BRANCH parameters.\n",op->name);
	  return FALSE;
	}
      }
      break;
    case ABS_X:
      if (op->ABSX) {
	pushByte(machine, op->ABSX);
	pushWord(machine, param->value[0]);
	break;
      }
      else {
	fprintf(stderr,"%s does not take ABS_X parameters.\n",op->name);
	return FALSE;
      }
    case ABS_Y:
      if (op->ABSY) {
	pushByte(machine, op->ABSY);
	pushWord(machine, param->value[0]);
	break;
      }
      else {
	fprintf(stderr,"%s does not take ABS_Y parameters.\n",op->name);
	return FALSE;
      }
    case ABS_LABEL_X:
      if (op->ABSX) {
	pushByte(machine, op->ABSX);
	pushWord(machine, param->lbladdr);
	break;
      }
      else {
	fprintf(stderr,"%s does not take ABS_LABEL_X parameters.\n",op->name);
	return FALSE;
      }
    case ABS_LABEL_Y:
      if (op->ABSY) {
	pushByte(machine, op->ABSY);
	pushWord(machine, param->lbladdr);
	break;
      }
      else {
	fprintf(stderr,"%s does not take ABS_LABEL_Y parameters.\n",op->name);
	return FALSE;
      }
   case DCB_PARAM:
     /* Handled elsewhere */
     break;
   }
   return TRUE;
}

/* compileLine() - Compile one line of code. Returns
   TRUE if it compile successfully. */
static BOOL compileLine(AsmLine *asmline, void *args){
  machine_6502 *machine;
  machine = args;
  if (isBlank(asmline->command)) return TRUE;
  if (strcmp("*=",asmline->command) == 0){
    machine->defaultCodePC = asmline->param->value[0];
  }
  else if (strcmp("DCB",asmline->command) == 0){
    int i;
    for(i = 0; i < asmline->param->vp; i++)
      pushByte(machine, asmline->param->value[i]);
  }    
  else{
    int i;
    char *command = asmline->command;
    Opcodes op;
    for(i = 0; i < NUM_OPCODES; i++){
      if (strcmp(machine->opcodes[i].name, command) == 0){
	op = machine->opcodes[i];
	break;      
      }
    }
    if (i == NUM_OPCODES)
      return FALSE; /* unknow upcode */
    else
      return translate(&op,asmline->param,machine);
  }
  return TRUE;
}

/* indexLabels() - Get the address for each label */
static BOOL indexLabels(AsmLine *asmline, void *arg){
  machine_6502 *machine; 
  int thisPC;
  Bit16 oldDefault;
  machine = arg;
  oldDefault = machine->defaultCodePC;
  thisPC = machine->regPC;
  /* Figure out how many bytes this instruction takes */
  machine->codeLen = 0;

  if ( ! compileLine(asmline, machine) ){
    return FALSE;
  }

  /* If the machine's defaultCodePC has changed then we encountered a
     *= which changes the load address. We need to initials our code
     *counter with the current default. */
  if (oldDefault == machine->defaultCodePC){    
    machine->regPC += machine->codeLen;
  }
  else {
    machine->regPC = machine->defaultCodePC;
    oldDefault = machine->defaultCodePC;
  }

  if (asmline->labelDecl) {
    asmline->label->addr = thisPC;
  }
   return TRUE; 
}

static BOOL changeParamLabelAddr(AsmLine *asmline, void *label){
  Label *la = label;
  if (strcmp(asmline->param->label, la->label) == 0)
    asmline->param->lbladdr = la->addr;
  return TRUE;
}

static BOOL linkit(AsmLine *asmline, void *asmlist){
  apply(asmlist,changeParamLabelAddr,asmline->label);
  return TRUE;
}

/* linkLabels - Make sure all of the references to the labels contain
   the right address*/
static void linkLabels(AsmLine *asmlist){
  apply(asmlist,linkit,asmlist);
}

/* compileCode() - Compile the current assembly code for the machine */
static BOOL compileCode(machine_6502 *machine, const char *code){
  BOOL codeOk;
  AsmLine *asmlist;

  reset(machine);
  machine->defaultCodePC = machine->regPC = PROG_START;
  asmlist = parseAssembly(machine, &codeOk, code);

  if(codeOk){
    /* First pass: Find the addresses for the labels */
    if (!apply(asmlist, indexLabels, machine))
      return FALSE;
    /* update label references */
    linkLabels(asmlist);

#if 0 /* prints out some debugging information */
    {
      AsmLine *p;
      if(asmlist != NULL){
        for (p = asmlist; p != NULL; p = p->next)
          fprintf(stderr,"%s lbl: %s addr: %d ParamLbl: %s ParamAddr: %d\n",
                  p->command, p->label->label, p->label->addr,
                  p->param->label, p->param->lbladdr);
            }
    }

#endif    

    /* Second pass: translate the instructions */
    machine->codeLen = 0;
    /* Link label call push_byte which increments defaultCodePC.
       We need to reset it so the compiled code goes in the 
       correct spot. */
    machine->defaultCodePC = PROG_START;
    if (!apply(asmlist, compileLine, machine))
      return FALSE;

    if (machine->defaultCodePC > PROG_START ){
      machine->memory[machine->defaultCodePC] = 0x00;
      codeOk = TRUE;
    }
    else{
      fprintf(stderr,"No Code to run.\n");
      codeOk = FALSE;
    }
  }
  else{
    fprintf(stderr,"An error occured while parsing the file.\n");  
    codeOk = FALSE;
  }
  freeallAsmLine(asmlist);
  return codeOk;
}


/*
 *  execute() - Executes one instruction.
 *              This is the main part of the CPU emulator.
 *
 */

static void execute(machine_6502 *machine){
  Bit8 opcode;
  AddrMode adm;
  int opidx;

  if(!machine->codeRunning) return;

  opcode = popByte(machine);
  if (opcode == 0x00)
    machine->codeRunning = FALSE;
  else {
    opidx = opIndex(machine,opcode,&adm);
    if(opidx > -1)
      machine->opcodes[opidx].func(machine, adm);
    else
      fprintf(stderr,"Invalid opcode!\n");
  }
  if( (machine->regPC == 0) || 
      (!machine->codeRunning) ) {
    machine->codeRunning = FALSE;
  }
}

machine_6502 *build6502(){
  machine_6502 *machine;
  machine = emalloc(sizeof(machine_6502));
  assignOpCodes(machine->opcodes);
  buildIndexCache(machine);
  reset(machine);
  return machine;
}

void destroy6502(machine_6502 *machine){
  free(machine);
  machine = NULL;
}

void trace(machine_6502 *machine, FILE *output){
  Bit8 opcode = memReadByte(machine,machine->regPC);
  AddrMode adm;
  Pointer ptr;
  int opidx = opIndex(machine,opcode,&adm);
  int stacksz = STACK_TOP - machine->regSP;

  fprintf(output,"\n   NVFBDIZC\nP: %d%d%d%d%d%d%d%d ",
	 bitOn(machine->regP,NEGATIVE_FL),
	 bitOn(machine->regP,OVERFLOW_FL),
	 bitOn(machine->regP,FUTURE_FL),
	 bitOn(machine->regP,BREAK_FL),
	 bitOn(machine->regP,DECIMAL_FL),
	 bitOn(machine->regP,INTERRUPT_FL),
	 bitOn(machine->regP,ZERO_FL),
	 bitOn(machine->regP,CARRY_FL));
  fprintf(output,"A: %.2x X: %.2x Y: %.2x SP: %.4x PC: %.4x\n",
	 machine->regA, machine->regX, machine->regY, machine->regSP, machine->regPC);
  if (opidx > -1){
    Bit16 pc = machine->regPC;
    fprintf(output,"\n%.4x:\t%s",machine->regPC, machine->opcodes[opidx].name);
    if (peekValue(machine, adm, &ptr, pc+1))
      fprintf(output,"\tAddress:%.4x\tValue:%.4x\n",
	     ptr.addr,ptr.value);
    else
      fprintf(output,"\n");
  }
  fprintf(output,"STACK:");
  hexDump(machine,(STACK_TOP - stacksz) + 1, stacksz, output);
}

void disassemble(machine_6502 *machine, FILE *output){
  /* Read the opcode
     increment the program counter
     print the opcode
     loop until end of program. */
  AddrMode adm;
  Bit16 addr;
  Bit8 opcode;
  int opidx;
  char *mem;
  int i;
  Bit16 opc = machine->regPC;
  mem = calloc(20,sizeof(char));
  machine->regPC = PROG_START;
  do{
    addr = machine->regPC;
    opcode = popByte(machine);
    opidx = opIndex(machine,opcode,&adm);
    for (i = 0; i < 20; i++) mem[i] = '\0';
    dismem(machine, adm, mem);
    fprintf(output,"%x\t%s\t%s\n",
	    addr,machine->opcodes[opidx].name,mem); 
  }while((machine->regPC - PROG_START) < machine->codeLen); /*XXX - may need to change since defaultCodePC */
  free(mem);
  machine->regPC = opc;
}


void eval_file(machine_6502 *machine, const char *filename, Plotter plot, void *plotterState){
  char *code = NULL;

  machine->plot = plot;
  machine->plotterState = plotterState;

  code = fileToBuffer(filename);
  
  if (! compileCode(machine, code) ){
    eprintf("Could not compile code.\n");
  }

  free(code);

  machine->defaultCodePC = machine->regPC = PROG_START;
  machine->codeRunning = TRUE;
  do{
    sleep(0); /* XXX */
#if 0
    trace(machine);
#endif
    execute(machine);
  }while(machine->codeRunning);
}

void start_eval_file(machine_6502 *machine, const char *filename, Plotter plot, void *plotterState){
  char *code = NULL;
  reset(machine);

  machine->plot = plot;
  machine->plotterState = plotterState;

  code = fileToBuffer(filename);
  
  if (! compileCode(machine, code) ){
    eprintf("Could not compile code.\n");
  }

  free(code);

  machine->defaultCodePC = machine->regPC = PROG_START;
  machine->codeRunning = TRUE;
  execute(machine);
}

void start_eval_string(machine_6502 *machine, const char *code,
		       Plotter plot, void *plotterState){
  reset(machine);

  machine->plot = plot;
  machine->plotterState = plotterState;

  if (! compileCode(machine, code) ){
    fprintf(stderr,"Could not compile code.\n");
  }

  machine->defaultCodePC = machine->regPC = PROG_START;
  machine->codeRunning = TRUE;
  execute(machine);
}

/* void start_eval_binary(machine_6502 *machine, Bit8 *program, */
/* 		       unsigned int proglen, */
/* 		       Plotter plot, void *plotterState){ */
/*   unsigned int pc, n; */
/*   reset(machine); */
/*   machine->plot = plot; */
/*   machine->plotterState = plotterState; */

/*   machine->regPC = PROG_START; */
/*   pc = machine->regPC; */
/*   machine->codeLen = proglen; */
/*   n = 0; */
/*   while (n < proglen){ */
/*     machine->memory[pc++] = program[n++]; */
/*   } */
/*   machine->codeRunning = TRUE; */
/*   execute(machine); */
/* } */

void next_eval(machine_6502 *machine, int insno){
  int i = 0;
  for (i = 1; i < insno; i++){
    if (machine->codeRunning){
#if 0
      trace(machine, stdout);
#endif
      execute(machine);
    }
    else
      break;
  }
}
  
