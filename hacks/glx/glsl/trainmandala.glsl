// Title:  CC0: Quick hack on the train
// Author: mrange
// URL:    https://www.shadertoy.com/view/mtjyz1
// Date:   08-Aug-2023
// Desc:   CC0: Quick hack on the train
// Travelling on the train I tried to recreate some twitter art
// Unoptimized, hackish and so on but good enough for the train
// 
// The tweet inspiring me: https://twitter.com/SnowEsamosc/status/1688731167451947008

// CC0: Quick hack on the train
// Travelling on the train I tried to recreate some twitter art
// Unoptimized, hackish and so on but good enough for the train

// The tweet inspiring me: https://twitter.com/SnowEsamosc/status/1688731167451947008

const float zoom = log2(1.8);


#define TIME        iTime
#define RESOLUTION  iResolution
#define PI          3.141592654
#define TAU         (2.0*PI)
#define ROT(a)      mat2(cos(a), sin(a), -sin(a), cos(a))

// License: MIT, author: Inigo Quilez, found: https://iquilezles.org/www/articles/distfunctions2d/distfunctions2d.htm
float segment(vec2 p, vec2 a, vec2 b) {
    vec2 pa = p-a, ba = b-a;
    float h = clamp( dot(pa,ba)/dot(ba,ba), 0.0, 1.0 );
    return length( pa - ba*h );
}

// License: MIT OR CC-BY-NC-4.0, author: mercury, found: https://mercury.sexy/hg_sdf/
float mod1(inout float p, float size) {
  float halfsize = size*0.5;
  float c = floor((p + halfsize)/size);
  p = mod(p + halfsize, size) - halfsize;
  return c;
}

float circle(vec2 p, float r) {
  return length(p) - r;
}


vec2 toPolar(vec2 p) {
  return vec2(length(p), atan(p.y, p.x));
}

vec2 toRect(vec2 p) {
  return vec2(p.x*cos(p.y), p.x*sin(p.y));
}

#define REV(x)      exp2((x)*zoom)
#define FWD(x)      (log2(x)/zoom)

vec3 effect(vec2 op, vec2 opp) {
  float aa = 4.0/RESOLUTION.y;
  const float angle = TAU/10.0; 
  const mat2 rot = ROT(0.5*angle);
  vec3 col = vec3(0.0);
  
  op *= ROT(0.125*TIME);
  float od = 1E4;
  
  for (int j = 0; j < 2; ++j){
    float tm = TIME+float(j)*0.5;
    float ctm = floor(tm);
    float ftm = fract(tm);
    float z = REV(ftm);
    vec2 p = op;
    p /= z;
  
    float d = 1E4;
    float n = floor(FWD(length(p)));
    float r0 = REV(n);
    float r1 = REV(n+1.0);
    
    for (int i = 0; i < 2; ++i) {
      vec2 pp = toPolar(p);
      mod1(pp.y, angle);
      vec2 rp = toRect(pp);
      
      float d0 = circle(rp, r0);
      float d1 = circle(rp, r1);
      float d2 = segment(rp, rot*vec2(r0, 0.0), vec2(r1, 0.0));
      float d3 = segment(rp, transpose(rot)*vec2(r0, 0.0), vec2(r1, 0.0));
      d0 = abs(d0);
      d1 = abs(d1);
      d = min(d, d0);
      d = min(d, d1);
      d = min(d, d2);
      d = min(d, d3);
      float gd = d*z;
      vec3 gcol = (1.0+cos(0.5*vec3(0.0, 1.0, 2.0)+op.x*op.y+op.x+TIME+1.6*float(i+j)));
      col += gcol*0.02/(gd+0.0001);
      p *= rot;
    }
    d *= z;
    od = min(od, d);
  }
  od -= aa*0.66;
  col = min(col, 1.0);
  col = mix(col, vec3(0.5), smoothstep(0.0, -aa, od));
  col = 1.0-col;
  col *= smoothstep(0.025, 0.25, length(op));
  col += ((1.0+cos(vec3(0.0, 1.0, 2.0)+TIME))*0.05/(dot(op, op)+0.075));
  col *= smoothstep(1.5, 0.5, length(opp));
  col = sqrt(col);
  return col;  
}

void mainImage( out vec4 fragColor, in vec2 fragCoord )
{
  vec2 q = fragCoord/RESOLUTION.xy;
  vec2 p = -1. + 2. * q;
  vec2 pp = p;
  p.x *= RESOLUTION.x/RESOLUTION.y;
  vec3 col = effect(p, pp);
  fragColor = vec4(col, 1.0);
}
