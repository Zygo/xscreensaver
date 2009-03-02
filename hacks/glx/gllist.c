#include "gllist.h"

void renderList(const struct gllist *list, int wire_p){
	for(;list!=NULL;list=list->next){
		glInterleavedArrays(list->format,0,list->data);
		glDrawArrays((wire_p ? GL_LINE_LOOP : list->primitive),
                             0,list->points);
	}
}
