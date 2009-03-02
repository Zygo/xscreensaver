#include "gllist.h"

void renderList(struct gllist *list){
	for(;list!=NULL;list=list->next){
		glInterleavedArrays(list->format,0,list->data);
		glDrawArrays(list->primitive,0,list->points);
	}
}
