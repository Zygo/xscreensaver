/* xscreensaver, Copyright (c) 2002 Jamie Zawinski <jwz@jwz.org>
 * Utility functions to create "marching cubes" meshes from 3d fields.
 *
 * Permission to use, copy, modify, distribute, and sell this software and its
 * documentation for any purpose is hereby granted without fee, provided that
 * the above copyright notice appear in all copies and that both that
 * copyright notice and this permission notice appear in supporting
 * documentation.  No representations are made about the suitability of this
 * software for any purpose.  It is provided "as is" without express or 
 * implied warranty.
 */

#ifndef __MARCHING_H__
#define __MARCHING_H__

/* Given a function capable of generating a value at any XYZ position,
   creates OpenGL faces for the solids defined.

   init_fn is called at the beginning for initial, and returns an object.
   free_fn is called at the end.

   compute_fn is called for each XYZ in the specified grid, and returns
   the double value of that coordinate.  If smoothing is on, then
   compute_fn will also be called twice more for each emitted vertex,
   in order to calculate vertex normals (so don't assume it will only
   be called with values falling on the grid boundaries.)

   Points are inside an object if the are less than `isolevel', and
   outside otherwise.

   If polygon_count is specified, the number of faces generated will be
   returned there.
*/
extern void
marching_cubes (int grid_size,     /* density of the mesh */
                double isolevel,   /* cutoff point for "in" versus "out" */
                int wireframe_p,   /* wireframe, or solid */
                int smooth_p,      /* smooth, or faceted */

                void * (*init_fn)    (double grid_size, void *closure1),
                double (*compute_fn) (double x, double y, double z,
                                      void *closure2),
                void   (*free_fn)    (void *closure2),
                void *closure1,

                unsigned long *polygon_count);

#endif /* __MARCHING_H__ */
