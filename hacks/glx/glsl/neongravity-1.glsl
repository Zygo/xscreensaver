// Title:  Abstract gravitational well II
// Author: mrange
// URL:    https://www.shadertoy.com/view/43G3Wc
// Date:   13-Jun-2024
// Desc:   CC0: Abstract gravitational well II
// msm01 reminded me of an old shader that I thought was neat, but it suffered from aliasing effects. Since then, I've learned a few new tricks, so I created this new version. Hopefully, it's an improvement

// CC0: Abstract gravitational well II
//  msm01 reminded me of an old shader that I felt was neat but suffered
//  from alias effects. I have learnt a few new tricks since so I did
//  made this new version. Hopefully improved.
//  The old version: https://www.shadertoy.com/view/dlSXRR

//#define I_DO_NOT_LIKE_THE_FLASH_EFFECT

#define TIME            iTime
#define RESOLUTION      iResolution

#define PI              3.141592654
#define TAU             (2.0*PI)

#define TOLERANCE       0.001
#define MAX_RAY_LENGTH  30.0
#define MAX_RAY_MARCHES 100
#define NORM_OFF        0.05
#define ROT(a)          mat2(cos(a), sin(a), -sin(a), cos(a))

// License: WTFPL, author: sam hocevar, found: https://stackoverflow.com/a/17897228/418488
const vec4 hsv2rgb_K = vec4(1.0, 2.0 / 3.0, 1.0 / 3.0, 3.0);
vec3 hsv2rgb(vec3 c) {
  vec3 p = abs(fract(c.xxx + hsv2rgb_K.xyz) * 6.0 - hsv2rgb_K.www);
  return c.z * mix(hsv2rgb_K.xxx, clamp(p - hsv2rgb_K.xxx, 0.0, 1.0), c.y);
}
// License: WTFPL, author: sam hocevar, found: https://stackoverflow.com/a/17897228/418488
//  Macro version of above to enable compile-time constants
#define HSV2RGB(c)  (c.z * mix(hsv2rgb_K.xxx, clamp(abs(fract(c.xxx + hsv2rgb_K.xyz) * 6.0 - hsv2rgb_K.www) - hsv2rgb_K.xxx, 0.0, 1.0), c.y))

const float hoff      = 0.0;
const vec3 skyCol     = HSV2RGB(vec3(hoff+0.57, 0.70, 0.25));
const vec3 glowCol    = HSV2RGB(vec3(hoff+0.025, 0.85, 0.5));
const vec3 sunCol1    = HSV2RGB(vec3(hoff+0.60, 0.50, 0.5));
const vec3 sunCol2    = HSV2RGB(vec3(hoff+0.05, 0.75, 25.0));
const vec3 diffCol    = HSV2RGB(vec3(hoff+0.60, 0.75, 0.25));
const vec3 sunDir1    = normalize(vec3(3., 3.0, -7.0));

// License: Unknown, author: Matt Taylor (https://github.com/64), found: https://64.github.io/tonemapping/
vec3 aces_approx(vec3 v) {
  v = max(v, 0.0);
  v *= 0.6;
  float a = 2.51;
  float b = 0.03;
  float c = 2.43;
  float d = 0.59;
  float e = 0.14;
  return clamp((v*(a*v+b))/(v*(c*v+d)+e), 0.0, 1.0);
}

// License: MIT, author: Inigo Quilez, found: https://iquilezles.org/articles/spherefunctions/
float sphereD(vec3 ro, vec3 rd, vec4 sph, float dbuffer) {
    float ndbuffer = dbuffer/sph.w;
    vec3  rc = (ro - sph.xyz)/sph.w;
	
    float b = dot(rd,rc);
    float c = dot(rc,rc) - 1.0;
    float h = b*b - c;
    if( h<0.0 ) return 0.0;
    h = sqrt( h );
    float t1 = -b - h;
    float t2 = -b + h;

    if( t2<0.0 || t1>ndbuffer ) return 0.0;
    t1 = max( t1, 0.0 );
    t2 = min( t2, ndbuffer );

    float i1 = -(c*t1 + b*t1*t1 + t1*t1*t1/3.0);
    float i2 = -(c*t2 + b*t2*t2 + t2*t2*t2/3.0);
    return (i2-i1)*(3.0/4.0);
}

// License: MIT OR CC-BY-NC-4.0, author: mercury, found: https://mercury.sexy/hg_sdf/
float mod1(inout float p, float size) {
  float halfsize = size*0.5;
  float c = floor((p + halfsize)/size);
  p = mod(p + halfsize, size) - halfsize;
  return c;
}

// License: CC0, author: Mårten Rånge, found: https://github.com/mrange/glsl-snippets
vec2 toPolar(vec2 p) {
  return vec2(length(p), atan(p.y, p.x));
}

// License: MIT, author: Inigo Quilez, found: https://iquilezles.org/articles/distfunctions/
float rayPlane(vec3 ro, vec3 rd, vec4 p) {
  return -(dot(ro,p.xyz)+p.w)/dot(rd,p.xyz);
}

// License: MIT, author: Inigo Quilez, found: https://iquilezles.org/www/articles/distfunctions2d/distfunctions2d.htm
float box(vec2 p, vec2 b) {
  vec2 d = abs(p)-b;
  return length(max(d,0.0)) + min(max(d.x,d.y),0.0);
}

// License: MIT, author: Inigo Quilez, found: https://iquilezles.org/www/articles/distfunctions2d/distfunctions2d.htm
float parabola(vec2 pos, float k) {
  pos = pos.yx;
  pos.x = abs(pos.x);
  float ik = 1.0/k;
  float p = ik*(pos.y - 0.5*ik)/3.0;
  float q = 0.25*ik*ik*pos.x;
  float h = q*q - p*p*p;
  float r = sqrt(abs(h));
  float x = (h>0.0) ? 
      pow(q+r,1.0/3.0) - pow(abs(q-r),1.0/3.0)*sign(r-q) :
      2.0*cos(atan(r,q)/3.0)*sqrt(p);
    return length(pos-vec2(x,k*x*x)) * sign(pos.x-x);
}

// License: MIT, author: Inigo Quilez, found: https://iquilezles.org/articles/distfunctions/
float parabola(vec3 p, float k, float o) {
  vec2 q = vec2(length(p.xz) - o, p.y);
  return parabola(q, k);
}

float df(vec3 p) {
  return parabola(p, .9, 0.5);
}


vec3 normal(vec3 pos) {
  vec2  eps = vec2(NORM_OFF,0.0);
  vec3 nor;
  nor.x = df(pos+eps.xyy) - df(pos-eps.xyy);
  nor.y = df(pos+eps.yxy) - df(pos-eps.yxy);
  nor.z = df(pos+eps.yyx) - df(pos-eps.yyx);
  return normalize(nor);
}

float rayMarch(vec3 ro, vec3 rd) {
  float t = 0.0;
  const float tol = TOLERANCE;
  vec2 dti = vec2(1e10,0.0);
  int i = 0;
  for (i = 0; i < MAX_RAY_MARCHES; ++i) {
    float d = df(ro + rd*t);
    if (d<dti.x) { dti=vec2(d,t); }
    if (d < TOLERANCE || t > MAX_RAY_LENGTH) {
      break;
    }
    t += d;
  }
  if(i==MAX_RAY_MARCHES) { t=dti.y; };
  return t;
}

vec3 render0(vec3 ro, vec3 rd) {
  vec3 col = vec3(0.0);
  float sd = max(dot(sunDir1, rd), 0.0);
  float sf = 1.0001-sd;
  col += clamp(vec3(0.0025/max(abs(rd.y), 0.5*fwidth(rd.y)))*glowCol, 0.0, 1.0);
  col += 0.75*skyCol*pow((1.0-abs(rd.y)), 8.0);
  col += 2.0*sunCol1*pow(sd, 100.0);
  col += sunCol2*pow(sd, 800.0);

  float tp1  = rayPlane(ro, rd, vec4(vec3(0.0, -1.0, 0.0), 6.0));

  if (tp1 > 0.0) {
    vec3 pos  = ro + tp1*rd;
    vec2 pp = pos.xz;
    float db = box(pp, vec2(5.0, 9.0))-3.0;
    
    col += vec3(4.0)*skyCol*rd.y*rd.y*smoothstep(0.25, 0.0, db);
    col += vec3(0.8)*skyCol*exp(-0.5*max(db, 0.0));
    col += 0.25*sqrt(skyCol)*max(-db, 0.0);
  }

  return clamp(col, 0.0, 10.0);;
}


vec3 render1(vec3 ro, vec3 rd) {
  float t = rayMarch(ro, rd);

  vec3 col = vec3(0.0);

  vec3 p = ro+rd*t;
  vec3 n = normal(p);
  vec3 r = reflect(rd, n);
  float fre = 1.0+dot(rd, n);
  fre *= fre;
  float dif = dot(sunDir1, n); 

  if (t < MAX_RAY_LENGTH) {
    const float ExpBy = log2(1.5);
    float a = 0.2*TIME;
    vec2 pp = toPolar(p.xz);
    pp.y -= TAU/24.0;
    float la = length(pp.x*exp2(ExpBy*fract(a)));
    la = log2(la)/ExpBy;
    float lo = pp.y;
    vec2 fw = fwidth(vec2(la ,lo));
    mod1(la, 1.0);
    mod1(lo, TAU/12.0);

    float gdla = abs(la)-0.01;
    float gdlo = abs(lo)-0.0025;
    
    col = 1E-2*glowCol/max(gdla, 1E-3+3E-1*fw.x);
    col += 1E-2*glowCol/max(gdlo, 1E-3+1E-1*fw.y);
    col *= smoothstep(1.0, 0.98, fre);
    col += mix(0.33, 1.0, fre)*render0(p, r);
    col += sunCol1*dif*dif*diffCol;
    
#ifdef I_DO_NOT_LIKE_THE_FLASH_EFFECT
    const float flash = 0.;
#else
    float flash = exp(-6.*fract(a+0.15));
#endif
    float sd = sphereD(ro, rd, vec4(0.,-0.25,0.,1.+flash), t);
    col += sd*sd*mix(20.,60., flash)*glowCol;
  }

  return col;
}

vec3 effect(vec2 p, vec2 pp) {
  float tm  = TIME*0.5;
  
  vec3 ro = 1.0*vec3(5.0, 3.0, 0.);
  ro.xz *= ROT(-0.1*tm);
  const vec3 la = vec3(0.0, 0.5, 0.0);
  const vec3 up = normalize(vec3(0.0, 1.0, 0.0));

  vec3 ww = normalize(la - ro);
  vec3 uu = normalize(cross(up, ww ));
  vec3 vv = (cross(ww,uu));
  const float fov = tan(TAU/6.);
  vec3 rd = normalize(-p.x*uu + p.y*vv + fov*ww);

  vec3 col = render1(ro, rd);
  col -= 0.015*vec3(3.,2.,1.)*(length(p)+0.25);
  col *= smoothstep(1.5, 0.5, length(pp));
  col = aces_approx(col); 
  col = sqrt(col);
  
  return col;
}

void mainImage( out vec4 fragColor, in vec2 fragCoord ) {
  vec2 q = fragCoord/RESOLUTION.xy;
  vec2 p = -1. + 2. * q;
  vec2 pp = p;
  p.x *= RESOLUTION.x/RESOLUTION.y;
  vec3 col = vec3(0.0);
  col = effect(p, pp);
  fragColor = vec4(col, 1.0);
}

