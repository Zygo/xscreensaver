/*
 * Here is a replacement for getpwuid for VMS. It returns pointers
 * to userid (*pw_name) and owner (*pw_gecos) only. Other fields
 * may be added later.
 * Note that sys$getuai returns owner as a counted string.
 */

#include <stdio.h>
#include <string.h>
#include "pwd.h"

char *t_userid, owner[40];
static char vms_userid[16];
static char vms_owner[40];
static struct passwd vms_passwd;

struct passwd *getpwuid()
{
#define UAI$_OWNER 12
struct  dsc$descriptor_s
{
  unsigned short  dsc$w_length;
  unsigned char   dsc$b_dtype;
  unsigned char   dsc$b_class;
  char            *dsc$a_pointer;
} user_desc = {0, 14, 1, NULL};
  int status, sys$getuai(),length;
  struct {
    short buffer_length;
    short item_code;
    int *buffer_address;
    int *return_length_address;
    int terminator;
  } itmlst;

  t_userid = cuserid((char *) NULL);
  user_desc.dsc$w_length       = strlen(t_userid);
  user_desc.dsc$a_pointer      = t_userid;
  itmlst.buffer_length         = sizeof(owner);
  itmlst.item_code             = UAI$_OWNER;
  itmlst.buffer_address        = (int *)owner;
  itmlst.return_length_address = &length;
  itmlst.terminator            = 0;
  status = sys$getuai(0, 0, &user_desc, &itmlst, 0, 0, 0);
  if(status == 1) {
    length = (int)owner[0];
    strcpy(vms_userid, t_userid);
    strcpy(vms_owner, &owner[1]);
  } else {
    vms_userid[0] = '\0';
    vms_owner[0] = '\0';
  }
  vms_passwd.pw_name = vms_userid;
  vms_passwd.pw_gecos = vms_owner;
  return (&vms_passwd);
}
