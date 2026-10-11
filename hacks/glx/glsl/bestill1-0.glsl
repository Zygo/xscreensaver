// Title:  Everything is Temporary
// Author: Matt Vianueva <diatribes@gmail.com>
// URL:    https://www.shadertoy.com/view/w32BDD
// Date:   06-Nov-2025
// Desc:   Everything is Temporary

// Relicensed as MIT License, by permission, 8-Mar-2026.

/*

    -24 chars by @FabriceNeyret2
    
    ty!!   :D

*/

void mainImage(out vec4 o, vec2 u) {
    o = vec4(0,0,0,0);
    float d=0.,a=0.,e=0.,i=0.,s=0.,t = iTime*.5;
    vec3  p = iResolution;    
    
    // scale coords
    u = (u+u-p.xy)/p.y;
    
    // cinema bars
    if (abs(u.y) > .8) { o *= 0.; return; }
 
    vec2 v = u.yx*.7 + vec2(1.2,.1);
    float l1 = 2./length(u + v),
          l2 = 2./length(u - v);
 
    // camera movement
    u += cos(t*vec2(.4,.8)) * vec2(.3,.1);
    for(o*=0.; i++<1e2;
    
        // entity (orb)
        e = length(p - vec3(
            sin(sin(t*.2)+t*.4) * 2.,
            1.+sin(sin(t*1.3)+t*.2) *1.23,
            12.+t+cos(t*.5)*8.))-.1,
        
        // accumulate distance
        d += s = min(.01+.4*abs(s),e=max(.8*e, .01)),
        
        // grayscale color
        o += 1e2/(s+e*4.)+ l1 + l2)
        
        // noise loop start, march
        for (p = vec3(u*d,d+t), // p = ro + rd *d, p.z + t;
            
            // plane
            s =5.+p.y+cos(p.x*.1)*4.,
            
            // noise starts at .01 up to 3.,
            // grow by a+=a
            a = .01; a < 3.; a += a)
            
            // apply noise
            s -= abs(dot(sin(.2*p.z+.6*t+p / a ), .4+p-p)) * a;
    
    // tanh tonemap, brightness
    o = tanh(vec4(5,2,1,0)*o*o*o/1e9);
}

























/*
void mainImage(out vec4 o, vec2 u) {
    float d,a,e,i,s,t = iTime*.5;
    vec3  p = iResolution;    
    
    // scale coords
    u = (u+u-p.xy)/p.y;
    
    // cinema bars
    if (abs(u.y) > .8) { o = vec4(0); return; }
 
 
    float l1 = 2./length(u + (u.yx*.7+.2-vec2(-1.,.1))),
          l2 = 2./length(u - (u.yx*.7+.2-vec2(-1.,.1)));
 
 
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
        o += 1e2/(s+e*4.)+ l1 + l2)
        
        // noise loop start, march
        for (p = vec3(u*d,d+t), // p = ro + rd *d, p.z + t;
            
            // diagonal plane
            s =5.+p.y+cos(p.x*.1)*4.,
            
            // noise starts at .42 up to 16.,
            // grow by a+=a
            a = .01; a < 3.; a += a)
            
            // apply noise
            s -= abs(dot(sin(.2*p.z+.6*t+p / a ), .4+p-p)) * a;
    
    // tanh tonemap, brightness, light off-screen
    o = tanh(vec4(5,2,1,0)*o*o*o/1e9);
}
*/
