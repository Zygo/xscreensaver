// Title:  Neon Triangulator
// Author: mrange
// URL:    https://www.shadertoy.com/view/tXGGRD
// Date:   09-Jul-2025
// Desc:   CC0: Neon triangulator
//   Haven't done a synthwave shader in awhile so here goes.

// CC0: Neon triangulator
//  Haven't done a synthwave shader in awhile so here goes.

#define ROT(a)      mat2(cos(a), sin(a), -sin(a), cos(a))

// License: WTFPL, author: sam hocevar, found: https://stackoverflow.com/a/17897228/418488
const vec4 hsv2rgb_K = vec4(1.0, 2.0 / 3.0, 1.0 / 3.0, 3.0);
vec3 hsv2rgb(vec3 c) {
  vec3 p = abs(fract(c.xxx + hsv2rgb_K.xyz) * 6.0 - hsv2rgb_K.www);
  return c.z * mix(hsv2rgb_K.xxx, clamp(p - hsv2rgb_K.xxx, 0.0, 1.0), c.y);
}
// License: WTFPL, author: sam hocevar, found: https://stackoverflow.com/a/17897228/418488
//  Macro version of above to enable compile-time constants
#define HSV2RGB(c)  (c.z * mix(hsv2rgb_K.xxx, clamp(abs(fract(c.xxx + hsv2rgb_K.xyz) * 6.0 - hsv2rgb_K.www) - hsv2rgb_K.xxx, 0.0, 1.0), c.y))

const float 
    PI            = acos(-1.)
  , TAU           = 2.*PI
  , MaxDistance   = 10.
  , Tolerance     = 3e-3
  , NormalEpsilon = 1e-3
  , MaxIter       = 77.
  , LightDiv      = 3.
  , SimplexOff    = sqrt(2.)/3. 
  , MaxBounces    = 4.
  ;

const vec3 
    sunCol   = HSV2RGB(vec3(.72,.9,4e-4))
  , absCol   = HSV2RGB(vec3(.75,.33,.6))
  ;

// License: Unknown, author: Matt Taylor (https://github.com/64), found: https://64.github.io/tonemapping/
vec3 aces_approx(vec3 v) {
  const float 
      a = 2.51
    , b = 0.03
    , c = 2.43
    , d = 0.59
    , e = 0.14
    ;
  v = max(v, 0.)*.6;
  return clamp((v*(a*v+b))/(v*(c*v+d)+e), 0.0, 1.0);
}

// License: MIT, author: Inigo Quilez, found: https://iquilezles.org/www/articles/distfunctions2d/distfunctions2d.htm
float box(vec2 p, vec2 b) {
  vec2 d = abs(p)-b;
  return length(max(d,0.0)) + min(max(d.x,d.y),0.0);
}

// License: Unknown, author: Unknown, found: don't remember
float hash(float co) {
  return fract(sin(co*12.9898) * 13758.5453);
}

// License: Unknown, author: Unknown, found: don't remember
vec2 hash2(vec2 p) {
  p = vec2(dot (p, vec2 (127.1, 311.7)), dot (p, vec2 (269.5, 183.3)));
  return fract(sin(p)*43758.5453123);
}

vec2 shash2(vec2 p) {
  return hash2(p)*2.-1.;
}

// License: MIT, author: Inigo Quilez, found: https://iquilezles.org/articles/sdfxor/
//  Based on jt's: https://www.shadertoy.com/view/ssG3WK
float xor(float a, float b) {
  return max( min(a,b), -max(a,b) );
}

float boxPattern(vec2 p, float hh, vec2 sz) {
  const float 
      zz = 2.0
    ;
  float 
      d = 1E3
    , z = 2.
    , dd
    ;
  p /= z;
  for (float i = 0.; i < 3.; ++i) {
    vec2 
        n = round(p)
      , h = shash2(n + i+hh)
      , c = p - n
      ;
    d = xor(d, box(c - h * .4, sz)/z);
    z *= zz;                 
    p *= zz;
    p += vec2(3.14,2.71);              
  }

  return d;              
}

float modPolar3(inout vec2 p) {
  const vec2 r = normalize(vec2(sqrt(3.), -1.));
  float n = p.y < 0.0 ? -1. : 1.;
  vec2 ap = vec2(p.x, abs(p.y));
  float d = dot(ap, r);
  if (d > 0.) return 0.;
  ap -= 2. * d * r;
  ap.y *= -n;
  p = ap;
  return n;
}

// Inputs
vec2 g_h;
// Outputs
vec3 g_gd;
vec2 g_d ;
vec3 g_lp;
vec3 g_pp;
vec2 g_c ;

float df(vec3 p) {
  const mat2 
      R1  = ROT(PI/3.)
    ; 
  const vec2 
      off2 = R1*vec2(SimplexOff,0.0)
    ;

  float loff = -2.*g_h.x*iTime;
  vec3 op = p;
  p.z -= loff;
  float R = round(p.z/LightDiv)*LightDiv;
  g_lp = vec3(0,0,loff+round((p.z+LightDiv*.47)/LightDiv)*LightDiv);
  p.z -= R;
  p.xy = -p.yx;
  float 
      n = modPolar3(p.xy)
    , d0 = SimplexOff*.5-p.x
    , d1 = length(p.xz-.44*vec2(SimplexOff,0.))-.001
    , d2
    , d3 = 1E3
    , d4 = 1E3
    , d5 = 
        length(op.xy-vec2(0,-.1)+.1*sin(-TAU*g_h*iTime+2.*op.z*vec2(1, sqrt(2.))))
      + smoothstep(-sqrt(.5),-.5, sin((op.z+iTime+g_h.y*(iTime+123.))/TAU))*.2
    , d = d0
    ;
  if (g_h.x > 0.) {
    d3 = d5;
    d4 = d5*.25;
  }
  d = min(d, d1);
  g_pp = vec3(p.y,op.z,n);
  p.y = abs(p.y);

  d2= length(p.xy-off2) - .01;
  d = min(d, d2);
  d = min(d, d3);
  g_gd = min(g_gd, vec3(d1,d2,d4));
  g_d = vec2(d0,d3);
  g_c = vec2(n,floor((op.z-.05)/LightDiv));
  return d;
}

float rayMarch(vec3 ro, vec3 rd, float init, out float i) {
  float t=init;
  for (i=0.;i<MaxIter;++i) {
    vec3 p = ro+t*rd;
    float d=df(p);
    if (t > MaxDistance)return MaxDistance;
    if (d < Tolerance)  return t;
    t += d;
  }
  return t;
}

vec3 normal(vec3 pos) {
  const vec2 
      eps = vec2(NormalEpsilon, 0.0)
    ;
  return normalize(vec3(
      df(pos+eps.xyy)-df(pos-eps.xyy)
    , df(pos+eps.yxy)-df(pos-eps.yxy)
    , df(pos+eps.yyx)-df(pos-eps.yyx))
    );
}

vec3 render(vec3 ro, vec3 rd, float noise) {
  const float 
      aa = 0.00025
    ;
  float 
      nn    = 0.
    , initt = .2*noise
    ;
  vec3 
      col  =vec3(0)
    , cabs =vec3(1.)
    ;
  for (float l=0.;l<MaxBounces;++l) {
    if (dot(vec3(1),cabs) < .01)
      break;
    float 
        h0 = hash(nn)
      , h1 = fract(8667.*h0)
      ;
    g_h = vec2(h0,h1);
    g_gd = vec3(1E3);
    float 
        i
      , t = rayMarch(ro, rd, initt,i)
      ;
    vec2 
        d2 = g_d
      , c  = g_c
      ;
    vec3 
         gd = g_gd
       , pp = g_pp
       , lp = g_lp
       , p  = ro+rd*t
       ;
    float d = df(p);
    vec3 
        ld = normalize(lp-p)
      , n  = normal(p)
      , r  = reflect(rd,n)
      ;
    vec2 
        sz = .5*sqrt(hash2(c+1.23))
      ;
    if (c.x+c.y < 3.) {
      sz = vec2(-1);
    }
    float 
        pd  = boxPattern(pp.xy,h0, sz)
      , tol = Tolerance*3.
      , phit= d2.x <= tol ? 1. : 0. 
      , pf  = smoothstep(aa, -aa, pd)*phit
      , mgd = min(min(gd.x,gd.y),gd.z)
      ;    
    if (t < MaxDistance) {
      vec3 
          ccol     = vec3(0)
        , difCol0  = hsv2rgb(vec3(.7+.15*(3.*p.y-p.x),.9,.25))
        , glowCol0 = 2e-4*difCol0
        , glowCol1 = hsv2rgb(vec3(.6+.2*h1,.9,2e-4))
        ;
      ccol += difCol0*pow(max(dot(n,ld),0.), mix(2.,4., pf));
      ccol += difCol0*pow(max(dot(reflect(rd,n),ld),0.), mix(40.,80., pf));
      ccol += glowCol0/max(gd.x*gd.x, 5e-6);
      ccol += glowCol0/max(gd.y*gd.y, 5e-5)*(1.+sin(1e2*p.z));
      ccol += glowCol1/max(gd.z*gd.z, 5e-6);
      ccol *= cabs;

      col += ccol;
      rd = r;
      cabs *= absCol*pf;
      nn += pp.z*1.2+1.23;
      ro = p+n*.02;
      initt = noise*.02;
      
      if (mgd < tol) {
        cabs = vec3(0.);
      }
    } else {
      break;
    }
  }
  return col;
}


vec3 effect(vec2 p, float noise) {
  const vec3
      up = vec3(0, 1, 0)
    , ww = normalize(vec3(0, 0, 1))
    , uu  = normalize(cross(up, ww))
    , vv  = cross(ww,uu)
    ;
  vec3
      ro = vec3(0,0,iTime)
    , rd  = normalize(p.y*vv -p.x*uu + 2.*ww)
    , col = render(ro, rd, noise)
    ;
  col += sunCol/(1.00001+dot(abs(rd), normalize(vec3(0,0,-1))));    

  col -= .02*vec3(2,3,1)*(length(p)+.25);
  col = aces_approx(col);
  col = sqrt(col);
  return col;
}

void mainImage(out vec4 O, vec2 C) {
  vec2 r = iResolution.xy, p = (2.*C-r)/r.y;
  O = vec4(effect(p, fract(dot(C,sin(C)))), 1);
}

