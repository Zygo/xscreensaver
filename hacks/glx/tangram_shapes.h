#ifndef TANGRAM_SHAPES_H
#define TANGRAM_SHAPES_H

/* get_sm_tri_dl - Get small triangle Display List */
GLuint tangram_get_sm_tri_dl(int wire);
/* get_lg_tri_dl - Get large triangle Display List */
GLuint tangram_get_lg_tri_dl(int wire);
/* get_md_tri_dl - Get medium triangle Display List */
GLuint tangram_get_md_tri_dl(int wire);
/* get_square_dl - Get square Display List */
GLuint tangram_get_square_dl(int wire);
/* get_rhomboid_dl - Get rhomboid Display List */
GLuint tangram_get_rhomboid_dl(int wire);

#endif
