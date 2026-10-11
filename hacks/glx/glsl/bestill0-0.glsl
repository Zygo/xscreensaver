// Title:  Be Still
// Author: Matt Vianueva <diatribes@gmail.com>
// URL:    https://www.shadertoy.com/view/tfXcRn
// Date:   14-Aug-2025
// Desc:   Be Still

// Relicensed as MIT License, by permission, 8-Mar-2026.

void mainImage(out vec4 o, vec2 u) {
    o = vec4(0,0,0,0);
    float d=0.,a=0.,e=0.,i=0.,s=0.,t = iTime*.5;
    vec3  p = iResolution;    
    
    // scale coords
    u = (u+u-p.xy)/p.y;
    
    // cinema bars
    if (abs(u.y) > .8) { o = vec4(0); return; }
    
    // camera movement
    u += vec2(cos(t*.4)*.3, cos(t*.8)*.1);
    
    for(o*=i; i++<1e2;
    
        // entity (orb)
        e = length(p - vec3(
            sin(sin(t*.2)+t*.4) * 2.,
            1.+sin(sin(t*1.3)+t*.2) *1.23,
            12.+t+cos(t*.5)*8.))-.1,
        
        // accumulate distance
        d += s = min(.01+.4*abs(s),e=max(.8*e, .01)),
        
        // grayscale color
        o += 1./(s+e*4.))
        
        // noise loop start, march
        for (p = vec3(u*d,d+t), // p = ro + rd *d, p.z + t;
            
            // diagonal plane
            s =4.+p.y+p.x*.3,
            
            // noise starts at .42 up to 16.,
            // grow by a+=a
            a = .42; a < 16.; a += a)
            
            // apply noise
            s -= abs(dot(sin(.6*t+p * a ), .18+p-p)) / a;
    
    // tanh tonemap, brightness, light off-screen
    u += (u.yx*.7+.2-vec2(-1.,.1));
    o = tanh(vec4(4,2,1,0)*o/1e1/dot(u,u));
}
