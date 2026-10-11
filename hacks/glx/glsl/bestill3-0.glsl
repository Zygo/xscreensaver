// Title:  Cloud LIghts
// Author: Matt Vianueva <diatribes@gmail.com>
// URL:    https://www.shadertoy.com/view/wXXBRX
// Date:   29-Oct-2025
// Desc:   Cloud LIghts

// Relicensed as MIT License, by permission, 8-Mar-2026.

#define O(Z,c) ( length(                 /* orb */   \
          p - vec3( sin( t*c*24. ) * 16.,        \
                    sin( t*c*18. ) * 12. + 12.,  \
                    Z+t+t+cos(t*.5)*32. )  ) - c )

void mainImage(out vec4 o, vec2 u) {
    o = vec4(0,0,0,0);
    float i=0.,e=0.,a=0.,d=0.,s=0.,t=iTime;
    vec3  p = iResolution;    
    u = (u-p.xy/2.)/p.y;
    if (abs(u.y) > .4) { o = vec4(0); return; }
    u += vec2(cos(t*.4)*.3, cos(t*.8)*.1);
    
    vec2 v = u - (u.yx*.8+.2-vec2(-.2,.1));;
    float light = dot(v,v);
    
    for(o*=i; i++<80.;
        d += s = min(e, .06 + abs(s)*.3),
        o += 1./s + 1e2/e + 1. / light) 
        for (p = vec3(u*d,d+t+t),
        
            e = max( .8* min( O( 3e1, .03),
                         min( O( 4e1, .06),
                         min( O( 5e1, .09),
                         min( O( 6e1, .12),
                              O( 7e1, .15) )))), .001 ),
        
            s = 6. + p.y,
            a = .05; a < 2.; a += a)
            s -= abs(dot(sin(t+.3*p / a), .6+p-p)) * a;
    
    o = tanh(vec4(1,2,5,0)*o*o /4e6);
}

