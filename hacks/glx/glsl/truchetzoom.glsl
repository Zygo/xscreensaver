// Title:  Another truchet experiment
// Author: mrange
// URL:    https://www.shadertoy.com/view/4cBcDy
// Date:   05-Aug-2024
// Desc:   CC0: Another truchet experiment
//   That's it for tonight

// CC0: Another truchet experiment
//  That's it for tonight

#define DISTORT


#define TIME        iTime
#define RESOLUTION  iResolution
#define ROT(a)      mat2(cos(a), sin(a), -sin(a), cos(a))

const float
  pi    = acos(-1.)
, tau   = pi*2.
, phi   = (1.+sqrt(5.))*0.5
, phi2  = phi*phi
, phi3  = phi*phi2
, phi4  = phi2*phi2
, iphi  = 1./phi
, iphi2 = 1./phi2
, iphi3 = 1./phi3
, iphi4 = 1./phi4
;

float geometric(float a, float r) {
  return a/(1.-r);
}

float igeometric(float a, float r, float x) {
  return log2(1.-x*(1.-r)/a)/log2(r);
}

vec2 geometric2(float a, float r, float n) {
  float rn = pow(r, n);
  float rn1 = rn*r;
  return (a/(1.-r))*(1.-vec2(rn, rn1));
}

float badSquare(vec2 p, float b) {
  vec2 d = abs(p)-b;
  return max(d.x,d.y);
}

vec4 boxySpiralCoord(vec2 p, float z, out float side) {
  float px = p.x;
  float ax = abs(px);
  float sx = sign(px);
  float a = px > 0. ? phi2 : 1.;
  a *= z;
  float gdx     = geometric(a, iphi4); 
  ax            -= gdx;
  float x       = igeometric(a, iphi4, -ax);
  float nx      = floor(x);
  vec2  lx      = geometric2(a, iphi4, nx)-gdx;
  float minx    = lx.x;
  float maxx    = lx.y;
  float radiusx = (maxx-minx)*0.5;
  float meanx   = minx+radiusx;
  
  p -= vec2(-1.,1./3.)*meanx*sx;
  side = sx;
  return vec4(p, radiusx, 2.*nx+(sx>0.?0.:1.));
}

vec2 toSmith(vec2 p)  {
  // z = (p + 1)/(-p + 1)
  // (x,y) = ((1+x)*(1-x)-y*y,2y)/((1-x)*(1-x) + y*y)
  float d = (1.0 - p.x)*(1.0 - p.x) + p.y*p.y;
  float x = (1.0 + p.x)*(1.0 - p.x) - p.y*p.y;
  float y = 2.0*p.y;
  return vec2(x,y)/d;
}

vec2 fromSmith(vec2 p)  {
  // z = (p - 1)/(p + 1)
  // (x,y) = ((x+1)*(x-1)+y*y,2y)/((x+1)*(x+1) + y*y)
  float d = (p.x + 1.0)*(p.x + 1.0) + p.y*p.y;
  float x = (p.x + 1.0)*(p.x - 1.0) + p.y*p.y;
  float y = 2.0*p.y;
  return vec2(x,y)/d;
}


vec2 transform(vec2 p) {
#ifdef DISTORT  
  vec2 sp0 = toSmith(p-0.);
  vec2 sp1 = toSmith(p+vec2(1.0)*ROT(0.12*TIME));
  vec2 sp2 = toSmith(p-vec2(1.0)*ROT(0.23*TIME));
  p = fromSmith(sp0+sp1-sp2);
  p *= ROT(-TIME*0.125);
#endif

  return p;
}

float segment(vec2 p, vec3 sz) {
  p.x = abs(p.x);
  p.x -= sz.x;
  
  float d = 
      p.x > 0.
    ? length(p)
    : abs(p.y)
    ;
  d -= sz.y;
  d = abs(d);
  d -= sz.z;

  return d;

}

float big(float anim, float n) {
  float a = 0.125*pi*n+tau*anim*2.;
  return 0.75+0.25*smoothstep((0.5), 0.9, sin(a));
}

vec3 effect(vec3 col, vec2 p) {
  const float aas = 1.;
  vec2 np = p + aas/RESOLUTION.y;
  vec2 tp = transform(p);
  vec2 ntp = transform(np);
  float aa = sqrt(2.0)/aas*distance(tp, ntp);
  p = tp;


  float anim = TIME*0.125;

  float pft = fract(anim);
  float nft = floor(anim);
  float piz = exp2(log2(phi4)*pft);
  float sx;
  float sy;
  vec4 gcx = boxySpiralCoord(p, piz,sx);
  vec4 gcy = boxySpiralCoord(vec2(p.y,-p.x), phi*piz,sy);

  float dbx = badSquare(gcx.xy, gcx.z);
  float dby = badSquare(gcy.xy, gcy.z);

  float nx = 1.+2.*gcx.w+4.*nft;
  float ny = 2.*gcy.w+4.*nft;

  float db;
  vec4 gc;
  float n;
  float s; 
  if (dbx < dby) {
    db = dbx;
    gc = gcx;
    n = nx;
    s = sx;
  } else {
    db = dby;
    gc = gcy;
    n = ny;
    s = sy;
  }
  
  if (s == -1.) {
    gc.xy = -gc.xy;
  }
 
  float l3 = length(gc.xy-gc.z*vec2(1.,-1.));
  float l4 = length(gc.xy+gc.z*vec2(1.,-1.));
  float l5 = length(gc.xy-gc.z*vec2(-1.,-1.+2.*iphi));

  float f = gc.z*iphi*0.7;
  float cr = gc.z*0.02;
  float ds3 = l3 - big(anim, n)*f*phi2*phi;
  float ds4 = l4 - big(anim, n+3.)*f;
  float ds5 = l5 - big(anim, n+4.)*iphi*f;


  float dbkg, dfgd;

  float w = 0.025*gc.z; 
  float h = 0.12*gc.z;

  dbkg = segment((gc.xy-gc.z).yx, vec3(h*phi4,f*phi,w*phi3));
  dbkg = min(dbkg, segment((gc.xy+gc.z), vec3(h*phi3,f,w*phi2)));
  dbkg = min(dbkg, abs(ds4)-w);
  dbkg = min(dbkg, abs(ds5)-w*iphi);

  dbkg = min(dbkg, l5 - cr);
  dbkg = min(dbkg, l4 - cr*phi);
  dbkg = min(dbkg, l3 - cr*phi4);

  float sw = w;
  dfgd = segment((gc.xy-gc.z*vec2(-1.,-1.+2.*(iphi+iphi4))).yx, vec3(h,f*iphi3,w*iphi));
  float dfgd_ = abs(ds3)-w*phi3;
  if (dfgd_ < dfgd) {
    dfgd = dfgd_;
  } else {
    sw *= iphi3;
  }

  const vec3 ocol = vec3(0.);
  vec3 bcol = vec3(sqrt(0.5));
  const vec3 fcol = vec3(sqrt(0.5));

  if (dbkg < dfgd) {
    bcol *= 1.-exp(-max(dfgd/sw,0.));
  }
  
  col = mix(col, bcol, smoothstep(aa, -aa, dbkg));
  col = mix(col, fcol, smoothstep(aa, -aa, dfgd));

  return col;
}

void mainImage( out vec4 fragColor, in vec2 fragCoord ) {
  vec2 q = fragCoord/RESOLUTION.xy;
  vec2 p = -1. + 2. * q;
  p += 1E-3;
  p.x *= RESOLUTION.x/RESOLUTION.y;

  vec2 fc = fragCoord;
  float thickness = 4.;  // jwz
  vec3 col = vec3(mod(fc.y,thickness*2.) < thickness ? 0.05 : 0.0);
  col = effect(col, p);


  col = sqrt(col);
  
  fragColor = vec4(col, 1.0);
}


