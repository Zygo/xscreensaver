// Title:  Downfall
// Author: Matt Vianueva <diatribes@gmail.com>
// URL:    https://www.shadertoy.com/view/w3sBWl
// Date:   01-Nov-2025
// Desc:   Downfall

// Relicensed as MIT License, by permission, 8-Mar-2026.

void mainImage(out vec4 o, vec2 u) {
    
    o = vec4(0,0,0,0);
    float i=0.,d=0.,s=0.,t = iTime;
    
    vec3 p = iResolution;
    mat2 r = mat2(cos(1.2+vec4(0,33,11,0)));
    
    u = (u+u-p.xy)/p.y;
    
    for(o=vec4(0); i++<1e2;) {
        p = vec3(u * d, d-24.),
        p.yz *= r;
        p.z += t*3e1;
        
        for(s = .03; s < 4.; s += s )
            p.yz -= abs(dot(sin(t+t+.32*p / s ), vec3(s)));
        
        p *= vec3(.2, .6, 1),
        d += s = .3+.3*abs(2. - length(p.xy)),
        o += 1./s;
    }
    
    o = tanh(o*o/1e4);

}

