// Title:  Desert Duo
// Author: Matt Vianueva <diatribes@gmail.com>
// URL:    https://www.shadertoy.com/view/3cXyzB
// Date:   20-Aug-2025
// Desc:   Desert Duo

// Relicensed as MIT License, by permission, 8-Mar-2026.

void mainImage(out vec4 o, vec2 u) {

    o = vec4(0,0,0,0);
    float
          // raymarch iterator
          i=0.,
          // total distance
          d=0.,
          // signed distance
          s=0.,
          // entity (orb) distance
          e=0.,
          // time (orb movement, camera sway, etc.)
          t=iTime,
          // flight time, z and x coord speed for flight
          zt = t * 12.,
          xt = t * 13.;
          
    vec3 p = iResolution;
    
    // scale coords and move the camera around a bit
    u = (u+u-p.xy)/p.y+cos(t*.3)*vec2(.4,.2);
    
    // clear o, march up to 128
    for(o*=i; i++<128.;
        
        // accumulate distance of orb, plane or clouds
        d += s = min(e, min(1.+ p.y*.6, 5.-p.y*.05)),
        
        // accumulate brightness
        o += s + 3./e)
        
        // noise start
        for(p = vec3(u*d,d+zt), // p = ro + rd * d, p.z += zt;

            // entity (orb), a sphere
            e = length(p - vec3(
                sin(sin(t*.2)+t*.4)*4.,
                sin(sin(t*1.3)+t*.2)*2.,
                14.+zt+cos(t*.5)*8.))-.1,

            // move to the side
            p.x -= xt,

            // start noise at .02, until 2, grow by s += s
            s = .02;
            s < 2.;
            s += s)
                 // apply noise
                 p += abs(dot(sin(p * s), p-p+.12)) / s;
    
    // make our angled sun beam light thing
    u += (u.yx*.7+.2-vec2(-1.,.1));
    
    // tanh tonemap, color, brightness, light
    o = tanh(vec4(5,2,1,0)*o*o/d/1e3/length(u));
}

