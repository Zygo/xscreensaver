// Title:  Rig Rekt
// Author: Matt Vianueva <diatribes@gmail.com>
// URL:    https://www.shadertoy.com/view/3XKfDV
// Date:   26-Feb-2026
// Desc:   Rig Rekt

// Relicensed as MIT License, by permission, 8-Mar-2026.

#define R(a) mat2(cos(a + vec4(0,33,11,0)))

float tunnel(vec3 p) {
    p = abs(p);
    return 4. - max(p.x, p.y/2.);
}

float box(vec3 p, float i) {
    p = abs(fract(p/i)*i - i/2.) - i*.08;
    return min(p.x, min(p.y, p.z));
}

float boxen(vec3 p) {
    float d = -9e9, i = 1e1;
    p.xy *= R(.5);
    for(; i > .2; i *= .2)
        p.xz *= R(i),
        d = max(d, box(p, i));
    return d;
}

float map(vec3 p) {
    return max(tunnel(p), boxen(p));
}

void mainImage(out vec4 o, vec2 u) {
    o = vec4(0,0,0,0);
    
    float i=0.,d=0.,s=0.,m=0.,k=0.,t = iTime;
    
    vec3 p = iResolution;
    u = (u+u-p.xy)/p.y;
    if (abs(u.y) > .75) { o *=i; return; };

    vec3 D = normalize(vec3(u, 1));
    vec2 v = (.1*sin(iTime))+u + (u.yx*.8+.2-vec2(-1.,.1));

    for(o*=i; i++<64.;) {
        p = D * d;
        p.z += iTime;
        m = map(p);
        
        for(s = .01; s < .4; s += s )
            p += abs(dot(sin(.3*p.z+t+.7*p / s ), vec3(s/4.)));
        
        d += s = min(m, k = .005+.3*abs(p.y+1.5)),
        o += 6e1*vec4(1,1.2,1,0)*s
          + .5*vec4(1,1.1,1,0)/k;
    }
    
    o = tanh(o/1.3e3/exp(d/6e1)/length(v));
}

