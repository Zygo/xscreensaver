/* worldpieces, Copyright (c) 2026 Jamie Zawinski <jwz@jwz.org>
 *
 * Permission to use, copy, modify, distribute, and sell this software and its
 * documentation for any purpose is hereby granted without fee, provided that
 * the above copyright notice appear in all copies and that both that
 * copyright notice and this permission notice appear in supporting
 * documentation.  No representations are made about the suitability of this
 * software for any purpose.  It is provided "as is" without express or 
 * implied warranty.
 */

#ifndef __COUNTRIES_H__
#define __COUNTRIES_H__

typedef struct {
  int count;
  const double *points;		/* A list of XY coords (lat/lon degrees) */
} country_path;

typedef struct {
  int count;
  const country_path *paths;	/* Polygon, a list of closed paths */
} country_polys;

typedef struct {
  const char *code;
  const char *name;		/* Official name in English */
  const char *endonym;		/* Official name in official language */
  const char *endonym2;		/* Transliterated */
  int population;
  int area;
  int count;
  const country_polys *polys;	/* MultiPolygon, a list of polygons */
} country_geom;

typedef struct {
  int count;
  const country_geom *geom;
} country_info;

extern const country_info all_countries;

#endif /* __COUNTRIES_H__ */
