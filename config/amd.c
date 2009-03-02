/*-
 * Sound file for vms
 * Jouk Jansen <joukj@alpha.chem.uva.nl> contributed this
 * which he found at http://axp616.gsi.de:8080/www/vms/mzsw.html
 */

/** amd.c **/

#include <iodef.h>		/* needed for IO$_ functions */
#include <ssdef.h>		/* system condition codes */
#include <descrip.h>		/* VMS Descriptor functions */
#include <string.h>
#include <starlet.h>

#include "amd.h"		/* AMD access functions */

struct dsc$descriptor_s devname;	/* device we are using */
static short int so_chan;	/* Channel to SODRIVER */
char        ReadBuffer[512];	/* Asynchronous read buffer */
unsigned short ReadIOsb[4] =
{0, 0, 0, 0};
int         ReadPending = 0;
int         ReadCompleted = 0;

/*-
 *	Initialize access to the AMD chip
 */
unsigned long int
AmdInitialize(char *device, int volume)
{
	unsigned long int status;
	unsigned long int iosb[2] =
	{0, 0};
	int         p1;

#ifdef DEBUG
	printf("Address of AMD Read buffer is 0x%x\n", &ReadBuffer);
#endif
	/*
	 *    Create the descriptor
	 */
	devname.dsc$w_length = strlen(device);
	devname.dsc$a_pointer = device;
	devname.dsc$b_dtype = DSC$K_DTYPE_T;
	devname.dsc$b_class = DSC$K_CLASS_S;
	/*
	 *    Assign a channel to the device
	 */
	status = sys$assign(&devname, &so_chan, 0, 0, 0);
	if (!(status & 1))
		return (status);
	/*
	 *    Clear MMR1
	 */
	p1 = 0;
	status = sys$qiow(0,	/* efn */
			  so_chan,	/* channel */
			  IO$_ACPCONTROL,	/* function */
			  iosb,	/* iosb */
			  0,	/* ast routine */
			  0,	/* ast param */
			  &p1,	/* Buffer (P1) */
			  sizeof (p1),	/* Buffer length (P2) */
			  AMD_SETREG,	/* IOCTL code (P3) */
			  MAP_MMR1,	/* AMD INDIRECT Register (P4) */
			  0,	/* P5 */
			  0);	/* P6 */

	if (!(status & 1))
		return (status);
	if (!(iosb[0] & 1))
		return (iosb[0]);
	/*
	 *    Clear MMR2
	 */
	p1 = 0;
	status = sys$qiow(0,	/* efn */
			  so_chan,	/* channel */
			  IO$_ACPCONTROL,	/* function */
			  iosb,	/* iosb */
			  0,	/* ast routine */
			  0,	/* ast param */
			  &p1,	/* Buffer (P1) */
			  sizeof (p1),	/* Buffer length (P2) */
			  AMD_SETREG,	/* IOCTL code (P3) */
			  MAP_MMR2,	/* AMD INDIRECT Register (P4) */
			  0,	/* P5 */
			  0);	/* P6 */

	if (!(status & 1))
		return (status);
	if (!(iosb[0] & 1))
		return (iosb[0]);
	/*
	 *    Set Volume to initial 30%
	 */
	p1 = 0x2ABD;
	status = sys$qiow(0,	/* efn */
			  so_chan,	/* channel */
			  IO$_ACPCONTROL,	/* function */
			  iosb,	/* iosb */
			  0,	/* ast routine */
			  0,	/* ast param */
			  &p1,	/* Buffer (P1) */
			  sizeof (p1),	/* Buffer length (P2) */
			  AMD_SETREG,	/* IOCTL code (P3) */
			  MAP_GER,	/* AMD INDIRECT Register (P4) */
			  0,	/* P5 */
			  0);	/* P6 */

	if (!(status & 1))
		return (status);
	if (!(iosb[0] & 1))
		return (iosb[0]);
	/*
	 *    Get MMR1
	 */
	p1 = 0;
	status = sys$qiow(0,	/* efn */
			  so_chan,	/* channel */
			  IO$_ACPCONTROL,	/* function */
			  iosb,	/* iosb */
			  0,	/* ast routine */
			  0,	/* ast param */
			  &p1,	/* Buffer (P1) */
			  sizeof (p1),	/* Buffer length (P2) */
			  AMD_GETREG,	/* IOCTL code (P3) */
			  MAP_MMR1,	/* AMD INDIRECT Register (P4) */
			  0,	/* P5 */
			  0);	/* P6 */

	if (!(status & 1))
		return (status);
	if (!(iosb[0] & 1))
		return (iosb[0]);
	/*
	 *    Set MMR1 to 8
	 */
	p1 = 8;
	status = sys$qiow(0,	/* efn */
			  so_chan,	/* channel */
			  IO$_ACPCONTROL,	/* function */
			  iosb,	/* iosb */
			  0,	/* ast routine */
			  0,	/* ast param */
			  &p1,	/* Buffer (P1) */
			  sizeof (p1),	/* Buffer length (P2) */
			  AMD_SETREG,	/* IOCTL code (P3) */
			  MAP_MMR1,	/* AMD INDIRECT Register (P4) */
			  0,	/* P5 */
			  0);	/* P6 */

	if (!(status & 1))
		return (status);
	if (!(iosb[0] & 1))
		return (iosb[0]);
	/*
	 *    Set GR
	 */
	p1 = 0x92FB;
	status = sys$qiow(0,	/* efn */
			  so_chan,	/* channel */
			  IO$_ACPCONTROL,	/* function */
			  iosb,	/* iosb */
			  0,	/* ast routine */
			  0,	/* ast param */
			  &p1,	/* Buffer (P1) */
			  sizeof (p1),	/* Buffer length (P2) */
			  AMD_SETREG,	/* IOCTL code (P3) */
			  MAP_GR,	/* AMD INDIRECT Register (P4) */
			  0,	/* P5 */
			  0);	/* P6 */

	if (!(status & 1))
		return (status);
	if (!(iosb[0] & 1))
		return (iosb[0]);
	/*
	 *    Get MMR1
	 */
	p1 = 0;
	status = sys$qiow(0,	/* efn */
			  so_chan,	/* channel */
			  IO$_ACPCONTROL,	/* function */
			  iosb,	/* iosb */
			  0,	/* ast routine */
			  0,	/* ast param */
			  &p1,	/* Buffer (P1) */
			  sizeof (p1),	/* Buffer length (P2) */
			  AMD_GETREG,	/* IOCTL code (P3) */
			  MAP_MMR1,	/* AMD INDIRECT Register (P4) */
			  0,	/* P5 */
			  0);	/* P6 */

	if (!(status & 1))
		return (status);
	if (!(iosb[0] & 1))
		return (iosb[0]);
	/*
	 *    Set MMR1 to 12 (0x0C)
	 */
	p1 = 12;
	status = sys$qiow(0,	/* efn */
			  so_chan,	/* channel */
			  IO$_ACPCONTROL,	/* function */
			  iosb,	/* iosb */
			  0,	/* ast routine */
			  0,	/* ast param */
			  &p1,	/* Buffer (P1) */
			  sizeof (p1),	/* Buffer length (P2) */
			  AMD_SETREG,	/* IOCTL code (P3) */
			  MAP_MMR1,	/* AMD INDIRECT Register (P4) */
			  0,	/* P5 */
			  0);	/* P6 */

	if (!(status & 1))
		return (status);
	if (!(iosb[0] & 1))
		return (iosb[0]);
	/*
	 *    Get MMR2
	 */
	p1 = 0;
	status = sys$qiow(0,	/* efn */
			  so_chan,	/* channel */
			  IO$_ACPCONTROL,	/* function */
			  iosb,	/* iosb */
			  0,	/* ast routine */
			  0,	/* ast param */
			  &p1,	/* Buffer (P1) */
			  sizeof (p1),	/* Buffer length (P2) */
			  AMD_GETREG,	/* IOCTL code (P3) */
			  MAP_MMR2,	/* AMD INDIRECT Register (P4) */
			  0,	/* P5 */
			  0);	/* P6 */

	if (!(status & 1))
		return (status);
	if (!(iosb[0] & 1))
		return (iosb[0]);
	/*
	 *    Put it back MMR2
	 */
	status = sys$qiow(0,	/* efn */
			  so_chan,	/* channel */
			  IO$_ACPCONTROL,	/* function */
			  iosb,	/* iosb */
			  0,	/* ast routine */
			  0,	/* ast param */
			  &p1,	/* Buffer (P1) */
			  sizeof (p1),	/* Buffer length (P2) */
			  AMD_SETREG,	/* IOCTL code (P3) */
			  MAP_MMR2,	/* AMD INDIRECT Register (P4) */
			  0,	/* P5 */
			  0);	/* P6 */

	if (!(status & 1))
		return (status);
	if (!(iosb[0] & 1))
		return (iosb[0]);
	/*
	 *    Set GX to 0x0112
	 */
	p1 = 0x0112;
	status = sys$qiow(0,	/* efn */
			  so_chan,	/* channel */
			  IO$_ACPCONTROL,	/* function */
			  iosb,	/* iosb */
			  0,	/* ast routine */
			  0,	/* ast param */
			  &p1,	/* Buffer (P1) */
			  sizeof (p1),	/* Buffer length (P2) */
			  AMD_SETREG,	/* IOCTL code (P3) */
			  MAP_GX,	/* AMD INDIRECT Register (P4) */
			  0,	/* P5 */
			  0);	/* P6 */

	if (!(status & 1))
		return (status);
	if (!(iosb[0] & 1))
		return (iosb[0]);
	/*
	 *    Get MMR1
	 */
	p1 = 0;
	status = sys$qiow(0,	/* efn */
			  so_chan,	/* channel */
			  IO$_ACPCONTROL,	/* function */
			  iosb,	/* iosb */
			  0,	/* ast routine */
			  0,	/* ast param */
			  &p1,	/* Buffer (P1) */
			  sizeof (p1),	/* Buffer length (P2) */
			  AMD_GETREG,	/* IOCTL code (P3) */
			  MAP_MMR1,	/* AMD INDIRECT Register (P4) */
			  0,	/* P5 */
			  0);	/* P6 */

	if (!(status & 1))
		return (status);
	if (!(iosb[0] & 1))
		return (iosb[0]);
	/*
	 *    Set MMR1 to 14 (0x0E)
	 */
	p1 = 14;
	status = sys$qiow(0,	/* efn */
			  so_chan,	/* channel */
			  IO$_ACPCONTROL,	/* function */
			  iosb,	/* iosb */
			  0,	/* ast routine */
			  0,	/* ast param */
			  &p1,	/* Buffer (P1) */
			  sizeof (p1),	/* Buffer length (P2) */
			  AMD_SETREG,	/* IOCTL code (P3) */
			  MAP_MMR1,	/* AMD INDIRECT Register (P4) */
			  0,	/* P5 */
			  0);	/* P6 */

	if (!(status & 1))
		return (status);
	if (!(iosb[0] & 1))
		return (iosb[0]);
	/*
	 *    Set GER (Volume to what the user wants)
	 */
	p1 = VolumeTable[volume - 1].ger_value;
	status = sys$qiow(0,	/* efn */
			  so_chan,	/* channel */
			  IO$_ACPCONTROL,	/* function */
			  iosb,	/* iosb */
			  0,	/* ast routine */
			  0,	/* ast param */
			  &p1,	/* Buffer (P1) */
			  sizeof (p1),	/* Buffer length (P2) */
			  AMD_SETREG,	/* IOCTL code (P3) */
			  MAP_GER,	/* AMD INDIRECT Register (P4) */
			  0,	/* P5 */
			  0);	/* P6 */

	if (!(status & 1))
		return (status);
	if (!(iosb[0] & 1))
		return (iosb[0]);
	/*
	 *    Get MMR1
	 */
	p1 = 0;
	status = sys$qiow(0,	/* efn */
			  so_chan,	/* channel */
			  IO$_ACPCONTROL,	/* function */
			  iosb,	/* iosb */
			  0,	/* ast routine */
			  0,	/* ast param */
			  &p1,	/* Buffer (P1) */
			  sizeof (p1),	/* Buffer length (P2) */
			  AMD_GETREG,	/* IOCTL code (P3) */
			  MAP_MMR1,	/* AMD INDIRECT Register (P4) */
			  0,	/* P5 */
			  0);	/* P6 */

	if (!(status & 1))
		return (status);
	if (!(iosb[0] & 1))
		return (iosb[0]);
	/*
	 *    Set GR (Volume to what the user wants)
	 */
	p1 = VolumeTable[volume - 1].gr_value;
	status = sys$qiow(0,	/* efn */
			  so_chan,	/* channel */
			  IO$_ACPCONTROL,	/* function */
			  iosb,	/* iosb */
			  0,	/* ast routine */
			  0,	/* ast param */
			  &p1,	/* Buffer (P1) */
			  sizeof (p1),	/* Buffer length (P2) */
			  AMD_SETREG,	/* IOCTL code (P3) */
			  MAP_GR,	/* AMD INDIRECT Register (P4) */
			  0,	/* P5 */
			  0);	/* P6 */

	if (!(status & 1))
		return (status);
	if (!(iosb[0] & 1))
		return (iosb[0]);
	/*
	 *    Get MMR1
	 */
	p1 = 0;
	status = sys$qiow(0,	/* efn */
			  so_chan,	/* channel */
			  IO$_ACPCONTROL,	/* function */
			  iosb,	/* iosb */
			  0,	/* ast routine */
			  0,	/* ast param */
			  &p1,	/* Buffer (P1) */
			  sizeof (p1),	/* Buffer length (P2) */
			  AMD_GETREG,	/* IOCTL code (P3) */
			  MAP_MMR1,	/* AMD INDIRECT Register (P4) */
			  0,	/* P5 */
			  0);	/* P6 */

	if (!(status & 1))
		return (status);
	if (!(iosb[0] & 1))
		return (iosb[0]);
	/*
	 *    Return Success
	 */
	return (SS$_NORMAL);
}

/*-
 *	Return the channel number associated with the device
 */
unsigned short
AmdGetChannel(void)
{
	return (so_chan);
}

/*-
 *	Increase or Decrease the Volume
 */
AmdSetVolume(int volume)
{
	unsigned long int status;
	unsigned short iosb[4] =
	{0, 0, 0, 0};
	int         p1;

	/*
	 *    Set GER (Volume to what the user wants)
	 */
	p1 = VolumeTable[volume - 1].ger_value;
	status = sys$qiow(0,	/* efn */
			  so_chan,	/* channel */
			  IO$_ACPCONTROL,	/* function */
			  iosb,	/* iosb */
			  0,	/* ast routine */
			  0,	/* ast param */
			  &p1,	/* Buffer (P1) */
			  sizeof (p1),	/* Buffer length (P2) */
			  AMD_SETREG,	/* IOCTL code (P3) */
			  MAP_GER,	/* AMD INDIRECT Register (P4) */
			  0,	/* P5 */
			  0);	/* P6 */

	if (!(status & 1))
		return (status);
	if (!(iosb[0] & 1))
		return (iosb[0]);
	/*
	 *    Get MMR1
	 */
	p1 = 0;
	status = sys$qiow(0,	/* efn */
			  so_chan,	/* channel */
			  IO$_ACPCONTROL,	/* function */
			  iosb,	/* iosb */
			  0,	/* ast routine */
			  0,	/* ast param */
			  &p1,	/* Buffer (P1) */
			  sizeof (p1),	/* Buffer length (P2) */
			  AMD_GETREG,	/* IOCTL code (P3) */
			  MAP_MMR1,	/* AMD INDIRECT Register (P4) */
			  0,	/* P5 */
			  0);	/* P6 */

	if (!(status & 1))
		return (status);
	if (!(iosb[0] & 1))
		return (iosb[0]);
	/*
	 *    Set GR (Volume to what the user wants)
	 */
	p1 = VolumeTable[volume - 1].gr_value;
	status = sys$qiow(0,	/* efn */
			  so_chan,	/* channel */
			  IO$_ACPCONTROL,	/* function */
			  iosb,	/* iosb */
			  0,	/* ast routine */
			  0,	/* ast param */
			  &p1,	/* Buffer (P1) */
			  sizeof (p1),	/* Buffer length (P2) */
			  AMD_SETREG,	/* IOCTL code (P3) */
			  MAP_GR,	/* AMD INDIRECT Register (P4) */
			  0,	/* P5 */
			  0);	/* P6 */

	if (!(status & 1))
		return (status);
	if (!(iosb[0] & 1))
		return (iosb[0]);
	/*
	 *    Get MMR1
	 */
	p1 = 0;
	status = sys$qiow(0,	/* efn */
			  so_chan,	/* channel */
			  IO$_ACPCONTROL,	/* function */
			  iosb,	/* iosb */
			  0,	/* ast routine */
			  0,	/* ast param */
			  &p1,	/* Buffer (P1) */
			  sizeof (p1),	/* Buffer length (P2) */
			  AMD_GETREG,	/* IOCTL code (P3) */
			  MAP_MMR1,	/* AMD INDIRECT Register (P4) */
			  0,	/* P5 */
			  0);	/* P6 */

	if (!(status & 1))
		return (status);
	if (!(iosb[0] & 1))
		return (iosb[0]);
	/*
	 *    Return Success
	 */
	return (SS$_NORMAL);
}

/*-
 *	Start recording sound
 */
unsigned long int
AmdInitRecord()
{
	unsigned long int status;
	unsigned long int iosb[2] =
	{0, 0};
	int         p1, p2, p3, p4, p5, p6;

	/*
	 *    Set recording level to 100%
	 */
	p1 = 0x0E;
	status = sys$qiow(0,	/* efn */
			  so_chan,	/* channel */
			  IO$_ACPCONTROL,	/* function */
			  iosb,	/* I/O status */
			  0,	/* ast routine */
			  0,	/* ast param */
			  &p1,	/* Buffer (P1) */
			  sizeof (p1),	/* Buffer Length (P2) */
			  AMD_SETREG,	/* IOCTL code (P3) */
			  MAP_GX,	/* AMD Register (P4) */
			  0,	/* P5 */
			  0);	/* P6 */

	if (!(status & 1))
		return (status);
	if (!(iosb[0] & 1))
		return (iosb[0]);
	/*
	 *    Start recording
	 */
	p3 = 2;
	p4 = 0x07D0;
	p5 = 0x14;
	status = sys$qiow(0,	/* efn */
			  so_chan,	/* channel */
			  IO$_ACCESS,	/* function code */
			  iosb,	/* I/O Status */
			  0,	/* ast routine */
			  0,	/* ast param */
			  0,	/* P1 */
			  0,	/* P2 */
			  p3,	/* Single freq tone */
			  p4,	/* frequency */
			  p5,	/* amplitude */
			  0);	/* P6 */

	if (!(status & 1))
		return (status);
	if (!(iosb[0] & 1))
		return (iosb[0]);
	/*
	 *    Retrieve the value of MMR2
	 */
	p1 = 0;
	status = sys$qiow(0,	/* efn */
			  so_chan,	/* channel */
			  IO$_ACPCONTROL,	/* function */
			  iosb,	/* iosb */
			  0,	/* ast routine */
			  0,	/* ast param */
			  &p1,	/* Buffer (P1) */
			  sizeof (p1),	/* Buffer length (P2) */
			  AMD_GETREG,	/* IOCTL code (P3) */
			  MAP_MMR2,	/* AMD INDIRECT Register (P4) */
			  0,	/* P5 */
			  0);	/* P6 */

	if (!(status & 1))
		return (status);
	if (!(iosb[0] & 1))
		return (iosb[0]);
	/*
	 *    Set MMR2 to 0
	 */
	p1 = 0;
	status = sys$qiow(0,	/* efn */
			  so_chan,	/* channel */
			  IO$_ACPCONTROL,	/* function */
			  iosb,	/* iosb */
			  0,	/* ast routine */
			  0,	/* ast param */
			  &p1,	/* Buffer (P1) */
			  sizeof (p1),	/* Buffer length (P2) */
			  AMD_SETREG,	/* IOCTL code (P3) */
			  MAP_MMR2,	/* AMD INDIRECT Register (P4) */
			  0,	/* P5 */
			  0);	/* P6 */

	if (!(status & 1))
		return (status);
	if (!(iosb[0] & 1))
		return (iosb[0]);
	/*
	 *    Issue a stop
	 */
	status = sys$qiow(0,	/* efn */
			  so_chan,	/* channel */
			  IO$_ACPCONTROL,	/* function */
			  iosb,	/* I/O status */
			  0,	/* ast routine */
			  0,	/* ast param */
			  0,	/* P1 */
			  0,	/* P2 */
			  AMD_STOP,	/* IOCTL function code */
			  0,	/* P4 */
			  0,	/* P5 */
			  0);	/* P6 */

	if (!(status & 1))
		return (status);
	if (!(iosb[0] & 1))
		return (iosb[0]);
	/*
	 *    Issue a read start to begin reading data
	 */
	status = sys$qiow(0,	/* efn */
			  so_chan,	/* channel */
			  IO$_ACPCONTROL,	/* function */
			  iosb,	/* I/O status */
			  0,	/* ast routine */
			  0,	/* ast param */
			  0,	/* P1 */
			  0,	/* P2 */
			  AMD_READSTART,	/* IOCTL function code */
			  0,	/* P4 */
			  0,	/* P5 */
			  0);	/* P6 */

	if (!(status & 1))
		return (status);
	if (!(iosb[0] & 1))
		return (iosb[0]);
	/*
	 *    Return success
	 */
	return (SS$_NORMAL);
}

/*-
 *	Select the Internal or External Speaker
 */
unsigned long int
AmdSelect(int s)
{
	unsigned long int status;
	unsigned long int iosb[2];
	int         p1;

	/*
	 *    Set the MMR2 register to indicate speaker
	 */
	p1 = s;
	status = sys$qiow(0,	/* efn */
			  so_chan,	/* channel */
			  IO$_ACPCONTROL,	/* function */
			  iosb,	/* iosb */
			  0,	/* ast routine */
			  0,	/* ast param */
			  &p1,	/* Buffer (P1) */
			  sizeof (p1),	/* Buffer length (P2) */
			  AMD_SETREG,	/* IOCTL code (P3) */
			  MAP_MMR2,	/* AMD INDIRECT Register (P4) */
			  0,	/* P5 */
			  0);	/* P6 */

	if (!(status & 1))
		return (status);
	if (!(iosb[0] & 1))
		return (iosb[0]);

}

/*-
 *  Write a buffer of sound data to the device
 */
unsigned long int
AmdWrite(char *buffer, int len)
{
  unsigned long int status;
  unsigned long iosb[2] =
  {0, 0};

  /*
   *    Write the data to the device
   */
  status = sys$qiow(0,
        so_chan,
        IO$_WRITEVBLK,
        iosb,
        0,
        0,
        buffer, len, 0, 0, 0, 0);

	if (!(status & 1))
		return (status);
	/*
	 *    Return success
	 */
	return (SS$_NORMAL);
}
