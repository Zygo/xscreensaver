// Title:  Trizm
// Author: Matt Vianueva <diatribes@gmail.com>
// URL:    https://www.shadertoy.com/view/3fcBD8
// Date:   14-Dec-2025
// Desc:   Trizm

// Relicensed as MIT License, by permission, 8-Mar-2026.

/*
    I thought @OldEclipse's "Cyber Conduits"
    was so cool I had to play with triangle
    waves more.
    
    @OldEclipse - "Cyber Conduits"
        https://www.shadertoy.com/view/tf3fR7

    Can also use them to create cool surfaces:
        https://www.shadertoy.com/view/tXX3RX

    Which was learned from @Shane's "Abstract Corridor":
        https://www.shadertoy.com/view/MlXSWX

    See also forked shader here for similar thing to this shader:
        https://www.shadertoy.com/view/3ftBWr

*/

void mainImage(out vec4 o, vec2 u) {

    o = vec4(0,0,0,0);
    float i=0., // raymarch iterator
          s=0., // sample distance
          t = iTime * .5,
          d = .05*dot(fract(sin(u)), sin(u))
            + (9.+2.*sin(t)); // total distance, modulate starting point
          
    vec3  p,r = iResolution;
    
    // 20 iterations
    for (o *= i; i++ < 2e1; ) {
        // get position
        p = vec3((u+u-r.xy)/r.y * d, d );
        
        // spin by time, twist by dist
        p.xy *= mat2(cos(.1*p.z+.1*t+vec4(0,33,11,0)));

        // triangle wave distortion loop
        // arcsin(sin(x)) makes a triangle wave
        for(s=0.; s++<3.;)
            p.xy -= asin(sin(.6*t+p.yx*s))/s;

        // accumulate distance to a triangle wave
        // based on p, which has been distorted by tri waves above,
        // this triangle wave is done using fract
        d += s =  dot(abs(fract(p)-.5), vec3(.08));

        // accumulate color
        o += 14./s + (1e1*(1.+cos(p.y*.1+p.z+vec4(3,1,0,0)))/s)
            // plasma'ish lights
          * abs(1. / dot(cos(t+t+p*.35),vec3(1)));
    }
    // tanh tonemap, brightness
    o = tanh(o/3e4);
}
