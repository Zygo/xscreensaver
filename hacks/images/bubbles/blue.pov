#include "colors.inc"
#include "shapes.inc"
#include "textures.inc"

/* The following make the field of view as wide as it is high
 * Thus, you should have the -W and -H command line options
 * equal to each other. */
camera {
        location <5.8, 0, 0>
	up <0, 1, 0>
	right <1, 0, 0>
        look_at <0, 0, 0>
}

sphere {
        <0,0,0>, 2.5
	texture { Blue_Agate 
	scale <0.7, 0.7, 0.7> }
	finish { phong 1 }
}

light_source {<6, 1, 0> color White}
