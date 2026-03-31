// Title:  Bubble Colors
// Author: Matt Vianueva <diatribes@gmail.com>
// URL:    https://www.shadertoy.com/view/wcGXWR
// Date:   20-Jun-2025
// Desc:   Bubble Colors

void mainImage(out vec4 o, vec2 u) 
{
    o = vec4(0,0,0,0);
    float i=0.,r=0.,s=0.,d=0.,n=0.,t=iTime;
    vec3  p = iResolution; 
    u = (u-p.xy/2.)/p.y;
    for (o*=i;i++<9e1; 
         d += s = .005 + abs(r)*.2,
         o += (1.+cos(.1*p.z+vec4(3,1,0,0))) / s)
        for(p = vec3(u * d, d + t*16.),
            r = 50.-abs(p.y)+ cos(t - dot(u,u) * 6.)*3.3,
            n = .08;
            n < .8;
            n *= 1.4)
            r -= abs(dot(sin(.3*t+.8*p*n), .7 +p-p )) / n;
    o = tanh(o / 2e3);
}
