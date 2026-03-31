// Title:  Water [237]
// Author: Matt Vianueva <diatribes@gmail.com>
// URL:    https://www.shadertoy.com/view/tXjXDy
// Date:   03-Nov-2025
// Desc:   Water [248]

// Relicensed as MIT License, by permission, 8-Mar-2026.

/*
    -3 by @FabriceNeyret2
   
    -11 by @bug (very very slight visual change)
    
    thanks!!  :D

*/

void mainImage( out vec4 o, vec2 u ) {
    o = vec4(0,0,0,0);
    float s=.3,i=0.,n=0.;
    vec3 r = iResolution,p=vec3(0,0,0);
    for(u = (u-r.xy/2.)/r.y-s; i++ < 32. && ++s>.001;)
        for (p += vec3(u*s,s),s = p.y,
            n =.01; n < 1.;n+=n)
            s += abs(dot(sin(p.z + iTime + p/n),  r/r)) * n*.1;
    o = tanh(i*vec4(5,2,1,0)/length(u-.1)/5e2);
}
