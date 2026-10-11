// Title:  Nox Fire
// Author: Matt Vianueva <diatribes@gmail.com>
// URL:    https://www.shadertoy.com/view/wfG3Dz
// Date:   24-May-2025
// Desc:   Nox Fire

// Relicensed as MIT License, by permission, 8-Mar-2026.

/*

    -4 from FabriceNeyret2
    
    Thanks :D !

*/

void mainImage(out vec4 o, vec2 u) {
    o = vec4(0,0,0,0);
    float i=0., d=0., s=0., n=0., t=iTime;
    vec3 p = iResolution;
    u = (u-p.xy/2.)/p.y;
    for(o*=i; i++<1e2; ) {
        // march, p = ro + rd * d, p.z += t*4;
        p = vec3(u * d, d + t*4.);
        // turbulence
        p += cos(p.z+t+p.yzx*.5)*.6;
        // modulate tunnel radius
        s = 4.+sin(t*.7)*4.-length(p.xy);
        // rotate
        p.xy *= mat2(cos(t+vec4(0,33,11,0)));
        // noise loop
        for (n = 1.6; n < 32.; n += n )
            // subtract noise from tunnel dist
            s -= abs(dot(sin( p.z + t + p*n ), vec3(1.12))) / n;
        // accumulate distance
        d += s = .01 + abs(s)*.1;
        // grayscale color
        o += 1. / s;
    }
    // o*o to increase saturation,
    // divide by d for depth
    // colorize
    o = tanh(vec4(5,2,1,1) * o * o / d / 2e6);
}


/* Original

void mainImage(out vec4 o, vec2 u) {
    float i, d, s, n, f, t=iTime;
    vec3 p = iResolution;
    u = (u-p.xy/2.)/p.y;
    for(o*=i; i++<1e2; ) {
        // march, p = ro + rd * d, p.z += t*4;
        p = vec3(u * d, d + t*4.);
        // turbulence
        p += cos(p.z+t+p.yzx*.5)*.6;
        // modulate tunnel radius
        s = 4.+sin(t*.7)*4.-length(p.xy);
        // rotate
        p.xy *= mat2(cos(t+vec4(0,33,11,0)));
        // noise loop
        for (n = .1; n < 2.;
            // subtract noise from tunnel dist
            s -= abs(dot(sin(p.z+t+p * n * 16.), vec3( .07))) / n,
            // grow noise
            n += n);
        // accumulate distance
        d += s = .01 + abs(s)*.1;
        // grayscale color
        o += 1. / s;
    }
    // o*o to increase saturation,
    // divide by d for depth
    // colorize
    o = tanh(vec4(5,2,1,1) * o * o / d / 2e6);
}

*/
