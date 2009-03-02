
typedef struct
  {
    int tag;
    char *label;
    void *data;
    int init;
  }
TagList;

#ifndef TRUE
#define TRUE  1
#define FALSE 0
#endif

#define TAG_NULL          0
#define TAG_STRING        1
#define TAG_INT           2
#define TAG_FLOAT         3
#define TAG_LABEL         4
#define TAG_WINDOW_LABEL  5
#define TAG_DONE          99

#define TAG_INIT    1
#define TAG_NOINIT  0

int GetValues (TagList * tags);
