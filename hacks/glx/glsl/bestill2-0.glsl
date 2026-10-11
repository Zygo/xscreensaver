// Title:  Night Cloud Dance
// Author: Matt Vianueva <diatribes@gmail.com>
// URL:    https://www.shadertoy.com/view/3cjcWD
// Date:   31-Aug-2025
// Desc:   Night Cloud Dance

// Relicensed as MIT License, by permission, 8-Mar-2026.

void mainImage(out vec4 o, vec2 u) {
    o = vec4(0,0,0,0);
    float d=0.,a=0.,e=0.,i=0.,s=0.,t = iTime*.5;
    vec3  ep, p = iResolution;    
    
    // scale coords
    u = (u+u-p.xy)/p.y;
    
    u += vec2(cos(t*.4)*.3, cos(t*.8)*.1);
    
    for(o*=i; i++<1e2;

        // accumulate distance
        d += s = min(.02+.6*abs(s),e=max(.8*e, .01)),
        
        // grayscale color
        o += 1./(s+e*2.))
        
        // noise loop start, march
        for (p = vec3(u*d,d+t), // p = ro + rd *d, p.z + t;
    
            // entity (orb) position
            ep = p - vec3(
                sin(sin(t)+t*.4) * 8.,
                sin(sin(t)+t*.2) *2.,
                16.+t+cos(t)*8.),
                
            // orb sphere
            e = length(ep) - .1,
                    
            // plane, mix with entity/orb
            s = mix(e*.02,4.+p.y, smoothstep(0., 12., length(ep))),
            
            // noise params
            a = .4; a < 8.; a *= 1.4)
            
            // apply noise
            s -= abs(dot(cos(t+.2*p.z+p * a ), .11+p-p)) / a;
    
    // tanh tonemap, blue tint, brightness, moon
    o = tanh(vec4(1,2,6,0)*o/1e1/length(u-.65));
}
