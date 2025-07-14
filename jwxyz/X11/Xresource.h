#pragma once

typedef struct jwxyz_XrmOptionDescRec {
  const char *option;
  const char *specifier;
  const char *argKind;
  const char *value;
} jwxyz_XrmOptionDescRec;

typedef struct jwxyz_XrmOptionDescRec XrmOptionDescRec;
#define XrmoptionNoArg  0
#define XrmoptionSepArg 1 