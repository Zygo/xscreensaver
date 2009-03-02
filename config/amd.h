/*-
 * Sound file for vms
 * Jouk Jansen <joukj@alpha.chem.uva.nl> contributed this
 * which he found at http://axp616.gsi.de:8080/www/vms/mzsw.html
 */

/** amd.h **/

/*-
 *    AMD access for the SODRIVER
 */

#ifndef AMD_H
#define AMD_H 1

/*-
 *    Define the SO Indirect Registers
 */
#define MAP_X		0x01
#define MAP_R		0x02
#define MAP_GX		0x03
#define MAP_GR		0x04
#define MAP_GER		0x05
#define MAP_SGCR	0x06
#define MAP_FTGR	0x07
#define MAP_ATGR	0x08
#define MAP_MMR1	0x09
#define MAP_MMR2	0x0A
#define MAP_PERFORM	0x0B
#define MUX_MCR1	0x0C
#define MUX_MCR2	0x0D
#define MUX_MCR3	0x0E
#define MUX_MCR4	0x0F
#define MUX_PERFORM	0x10
#define INIT_INIT	0x11

/*-
 *    Define SO ACPCONTROL (IOCTL) Subfunctions
 */
#define AMD_SETQSIZE	1
#define AMD_GETQSIZE	2
#define AMD_GETREG	3
#define AMD_SETREG	4
#define AMD_READSTART	5
#define AMD_STOP	6
#define AMD_PAUSE	7
#define AMD_RESUME	8
#define AMD_READQ	9
#define AMD_WRITEQ	10
#define AMD_DEBUG	11

/*-
 *	*** HACK ***
 *
 *	GER and GR values for Volume Control for the SODRIVER.  These values
 *	where determine via FTS (available from PUBLIC.TGV.COM) which allowed
 *	me to see the $QIO[W] calls made to the SODRIVER.  How DECsound
 *	actually calculates them is beyond me.  This table is index by percentage.
 *	For Example, if you want the volume at 45% then you use the following
 *	values:	VolumeTable[44].ger_value and VolumeTable[44].gr_value.
 *
 *	*** HACK ***
 */

static struct vtable {
	unsigned short ger_value;
	unsigned short gr_value;
} VolumeTable[] = {

/* GER            GR    */
	{
		0XAAAA, 0X91F9
	},			/* 001 % */
	{
		0XAAAA, 0X91F9
	},			/* 002 % */
	{
		0X9BBB, 0X91F9
	},			/* 003 % */
	{
		0X9BBB, 0X91F9
	},			/* 004 % */
	{
		0X79AC, 0X91F9
	},			/* 005 % */
	{
		0X79AC, 0X91C5
	},			/* 006 % */
	{
		0X099A, 0X91C5
	},			/* 007 % */
	{
		0X099A, 0X91C5
	},			/* 008 % */
	{
		0X4199, 0X91C5
	},			/* 009 % */
	{
		0X3199, 0X91B6
	},			/* 010 % */
	{
		0X3199, 0X91B6
	},			/* 011 % */
	{
		0X9CDE, 0X91B6
	},			/* 012 % */
	{
		0X9CDE, 0X91B6
	},			/* 013 % */
	{
		0X9DEF, 0X9212
	},			/* 014 % */
	{
		0X9DEF, 0X9212
	},			/* 015 % */
	{
		0X749C, 0X9212
	},			/* 016 % */
	{
		0X749C, 0X9212
	},			/* 017 % */
	{
		0X549D, 0X91A4
	},			/* 018 % */
	{
		0X6AAE, 0X91A4
	},			/* 019 % */
	{
		0X6AAE, 0X91A4
	},			/* 020 % */
	{
		0XABCD, 0X91A4
	},			/* 021 % */
	{
		0XABCD, 0X9222
	},			/* 022 % */
	{
		0XABDF, 0X9222
	},			/* 023 % */
	{
		0XABDF, 0X9222
	},			/* 024 % */
	{
		0X7429, 0X9222
	},			/* 025 % */
	{
		0X64AB, 0X9232
	},			/* 026 % */
	{
		0X64AB, 0X9232
	},			/* 027 % */
	{
		0X6AFF, 0X9232
	},			/* 028 % */
	{
		0X6AFF, 0X9232
	},			/* 029 % */
	{
		0X2ABD, 0X9232
	},			/* 030 % */
	{
		0X2ABD, 0X92FB
	},			/* 031 % */
	{
		0XBEEF, 0X92FB
	},			/* 032 % */
	{
		0XBEEF, 0X92FB
	},			/* 033 % */
	{
		0X5CCE, 0X92FB
	},			/* 034 % */
	{
		0X75CD, 0X92AA
	},			/* 035 % */
	{
		0X75CD, 0X92AA
	},			/* 036 % */
	{
		0X0099, 0X92AA
	},			/* 037 % */
	{
		0X0099, 0X92AA
	},			/* 038 % */
	{
		0X554C, 0X9327
	},			/* 039 % */
	{
		0X554C, 0X9327
	},			/* 040 % */
	{
		0X43DD, 0X9327
	},			/* 041 % */
	{
		0X43DD, 0X9327
	},			/* 042 % */
	{
		0X33DD, 0X93B3
	},			/* 043 % */
	{
		0X52EF, 0X93B3
	},			/* 044 % */
	{
		0X52EF, 0X93B3
	},			/* 045 % */
	{
		0X771B, 0X93B3
	},			/* 046 % */
	{
		0X771B, 0X94B3
	},			/* 047 % */
	{
		0X5542, 0X94B3
	},			/* 048 % */
	{
		0X5542, 0X94B3
	},			/* 049 % */
	{
		0X41DD, 0X94B3
	},			/* 050 % */
	{
		0X31DD, 0X9F91
	},			/* 051 % */
	{
		0X31DD, 0X9F91
	},			/* 052 % */
	{
		0X441F, 0X9F91
	},			/* 053 % */
	{
		0X431F, 0X9F91
	},			/* 054 % */
	{
		0X431F, 0X9CEA
	},			/* 055 % */
	{
		0X331F, 0X9CEA
	},			/* 056 % */
	{
		0X331F, 0X9CEA
	},			/* 057 % */
	{
		0X40DD, 0X9CEA
	},			/* 058 % */
	{
		0X1100, 0X9BF9
	},			/* 059 % */
	{
		0X1100, 0X9BF9
	},			/* 060 % */
	{
		0X440F, 0X9BF9
	},			/* 061 % */
	{
		0X440F, 0X9BF9
	},			/* 062 % */
	{
		0X411F, 0X9AAC
	},			/* 063 % */
	{
		0X411F, 0X9AAC
	},			/* 064 % */
	{
		0X311F, 0X9AAC
	},			/* 065 % */
	{
		0X311F, 0X9AAC
	},			/* 066 % */
	{
		0X5520, 0X9A4A
	},			/* 067 % */
	{
		0X10DD, 0X9A4A
	},			/* 068 % */
	{
		0X10DD, 0X9A4A
	},			/* 069 % */
	{
		0X4211, 0X9A4A
	},			/* 070 % */
	{
		0X4211, 0XA222
	},			/* 071 % */
	{
		0X410F, 0XA222
	},			/* 072 % */
	{
		0X410F, 0XA222
	},			/* 073 % */
	{
		0X111F, 0XA222
	},			/* 074 % */
	{
		0X600B, 0XA2A2
	},			/* 075 % */
	{
		0X600B, 0XA2A2
	},			/* 076 % */
	{
		0X00DD, 0XA2A2
	},			/* 077 % */
	{
		0X00DD, 0XA2A2
	},			/* 078 % */
	{
		0X4210, 0XA2A2
	},			/* 079 % */
	{
		0X4210, 0XA68D
	},			/* 080 % */
	{
		0X400F, 0XA68D
	},			/* 081 % */
	{
		0X400F, 0XA68D
	},			/* 082 % */
	{
		0X110F, 0XA68D
	},			/* 083 % */
	{
		0X2210, 0XAAA3
	},			/* 084 % */
	{
		0X2210, 0XAAA3
	},			/* 085 % */
	{
		0X7200, 0XAAA3
	},			/* 086 % */
	{
		0X7200, 0XAAA3
	},			/* 087 % */
	{
		0X4200, 0XB242
	},			/* 088 % */
	{
		0X4200, 0XB242
	},			/* 089 % */
	{
		0X2110, 0XB242
	},			/* 090 % */
	{
		0X2110, 0XB242
	},			/* 091 % */
	{
		0X100F, 0XBB52
	},			/* 092 % */
	{
		0X2200, 0XBB52
	},			/* 093 % */
	{
		0X2200, 0XBB52
	},			/* 094 % */
	{
		0X1110, 0XBB52
	},			/* 095 % */
	{
		0X1110, 0XCB52
	},			/* 096 % */
	{
		0X000B, 0XCBB2
	},			/* 097 % */
	{
		0X000B, 0XCBB2
	},			/* 098 % */
	{
		0X2100, 0XCBB2
	},			/* 099 % */
	{
		0X000F, 0X0808
	}			/* 100 % */
};

#define volume_table_size ((unsigned)sizeof(VolumeTable)/(unsigned)sizeof(VolumeTable[0]))

/*-
 *	Constants for Speaker selection
 */
#define	SO_EXTERNAL	0
#define SO_INTERNAL	2

/*-
 *	How many bytes of sound / second
 */
#define SO_BYTES_PER_SECOND	(1024*8)

extern unsigned long int AmdInitialize(char *device, int volume);
extern unsigned long int AmdSelect(int s);
extern unsigned long int AmdWrite(char *buffer, int len);
#endif /* !AMD_H */
