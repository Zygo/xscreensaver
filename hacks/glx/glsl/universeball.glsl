// Title:  Universe Ball 2
// Author: Matt Vianueva <diatribes@gmail.com>
// URL:    https://www.shadertoy.com/view/WcGcWV
// Date:   08-Dec-2025
// Desc:   Universe Ball 2

// Relicensed as MIT License, by permission, 8-Mar-2026.

// inspired by @Jaenam's gem shaders
// e.g., https://www.shadertoy.com/view/t3SyzV

// can play with color here
#define PALETTE vec3(6,4,2)

void mainImage(out vec4 o, vec2 u) {
    float n,i,s,t=iTime*.2, d,v;
    vec3  q,p = iResolution, c;
    u = (u+u-p.xy)/p.y;
    vec2 l = u - (u.yx*.9+.3-vec2(-.35,.15));    

    c = vec3(0);  // jwz
    d = 0.;
    i = 0.;

    for(; i++ < 5e1 && d < 5e1;
        d += s = min(q.y=.01+.6*abs(24. - length(q.xy)),
                     v = max(s, dot(abs(fract(p)-.5), vec3(.04)))),
        c +=(1.+cos(p.z+PALETTE))/v
          +  d*vec3(5,2,1)/q.y/1e1
          +  7.*vec3(3,4,1)/length(l)
    )
        for(q = p = vec3(u * d, d - 16.),
            s = length(p)-8.,
            p.xy *= mat2(cos(t+p.z*.6+vec4(0,33,11,0))),
            p += cos(t+p.zxy)+cos(t+p.yzx*s)/s/4.,
            p += .5*cos(t+dot(cos(t+p), p) *  p),
            n = .02; n < 2.; n *= 1.6
        )
            q.y -= abs(dot(sin(4.*t+.3*q / n ), q-q+n));

    c = mix(c, c.yzx, smoothstep(2., .1, length(u)*1.));
    o.rgb = tanh(c*c/6e7/length(u-.3)+.1*length(u));
}
