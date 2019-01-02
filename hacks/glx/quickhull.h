/* quickhull, Copyright (c) 2016 Karim Naaji, karim.naaji@gmail.com

  Permission is hereby granted, free of charge, to any person obtaining a copy
  of this software and associated documentation files (the "Software"), to deal
  in the Software without restriction, including without limitation the rights
  to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
  copies of the Software, and to permit persons to whom the Software is
  furnished to do so, subject to the following conditions:

  The above copyright notice and this permission notice shall be included in all
  copies or substantial portions of the Software.

  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
  IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
  FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
  AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
  LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
  OUT OF OR IN CONNECTION WITH THE
*/

#ifndef __QUICKHULL_H__
#define __QUICKHULL_H__

typedef struct qh_vertex {
  double x;
  double y;
  double z;
} qh_vertex_t;

typedef qh_vertex_t qh_vec3_t;

typedef struct qh_mesh {
  qh_vertex_t* vertices;
  qh_vec3_t* normals;
  unsigned int* indices;
  unsigned int* normalindices;
  unsigned int nindices;
  unsigned int nvertices;
  unsigned int nnormals;
} qh_mesh_t;

extern qh_mesh_t qh_quickhull3d(qh_vertex_t const* vertices,
                                unsigned int nvertices);

extern void qh_free_mesh(qh_mesh_t mesh);

#ifdef QUICKHULL_FILES
extern void qh_mesh_export(qh_mesh_t const* mesh, char const* filename);
#endif

#endif /* __QUICKHULL_H__ */
