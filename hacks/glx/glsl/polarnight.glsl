// Title:  Polar Night
// Author: supervitas
// URL:    https://www.shadertoy.com/view/3ss3R4
// Date:   18-Dec-2018
// Desc:   Simple raymarching terrain, thanks nimitiz for aurora

// Relicensed as MIT License, by permission, 28-Jul-2026.


#define MAX_MARCHING_STEPS 256
#define MAX_DIST 6. // far
#define EPSILON 0.001
#define PI 3.1415926535

#define u_time iTime
#define u_resolution iResolution


float random(vec2 p)
{
    vec3 p3  = fract(vec3(p.xyx) * .1031);
    p3 += dot(p3, p3.yzx + 33.33);
    return fract((p3.x + p3.y) * p3.z);
}


vec3 noise(vec2 p) {
  vec2 i = floor(p);
  vec2 f = fract(p);

  vec2 df = 20.0*f*f*(f*(f-2.0)+1.0);
  f = f*f*f*(f*(f*6.-15.)+10.);

  float a = random(i + vec2(0.5));
  float b = random(i + vec2(1.5, 0.5));
  float c = random(i + vec2(.5, 1.5));
  float d = random(i + vec2(1.5, 1.5));

  float k = a - b - c + d;
  float n = mix(mix(a, b, f.x), mix(c, d, f.x), f.y);

  return vec3(n, vec2(b - a + k * f.y, c - a + k * f.x) * df);
}

mat2 terrainProps = mat2(0.8,-0.4, 0.5,0.8);
float fbmM(vec2 p) {
  vec2 df = vec2(0.0);
  float f = 0.0;
  float w = 0.5;

  for (int i = 0; i < 8; i++) {
    vec3 n = noise(p);
    df += n.yz;
    f += abs(w * n.x / (1.0 + dot(df, df)));
    w *= 0.5;
    p = 2. * terrainProps * p;
  }
  return f;
}

float fbmH(vec2 p) {
  vec2 df = vec2(0.0);
  float f = 0.0;
  float w = 0.5;

  for (int i = 0; i < 12; i++) {
    vec3 n = noise(p);
    df += n.yz;
    f += abs(w * n.x / (1.0 + dot(df, df)));
    w *= 0.5;
    p = 2. * terrainProps * p;
  }
  return f;
}


float fbmL(vec2 p) {
  vec2 df = vec2(0.0);
  float f = 0.0;
  float w = 0.5;

  for (int i = 0; i < 2; i++) {
    vec3 n = noise(p);
    df += n.yz;
    f += abs(w * n.x / (1.0 + dot(df, df)));
    w *= 0.5;
    p = 2. * terrainProps * p;
  }
  return f;
}



float map(vec3 p) {
    float scene = p.y;
    
    float h = fbmM(p.xz);	
    scene -= h;

  	return scene;
}


float raymarch(vec3 ro, vec3 rd) {
  float d = 0.;
  float t = 0.;
  for (int i = 0; i < MAX_MARCHING_STEPS; i++) {
    d = map(ro + t * rd);
    if (d < EPSILON * t || t > MAX_DIST) break;
    t += 0.5 * d;
  }

  return d < EPSILON * t ? t : -1.;
}

vec3 normal(vec3 pos, float t) {
	vec2  eps = vec2( 0.002*t, 0.0 );
    return normalize( vec3( fbmH(pos.xz-eps.xy) - fbmH(pos.xz+eps.xy),
                            2.0*eps.x,
                            fbmH(pos.xz-eps.yx) - fbmH(pos.xz+eps.yx) ) );
}

struct light {
  vec3 lightPosition;
  vec3 amibnetColor;
  float ambientIntencity;
  vec3 directLightColor;
  vec3 directLightIntencity;
};

vec3 diffuseLight(vec3 k_d, vec3 p, vec3 eye, vec3 lightPos, vec3 lightIntensity) {
  vec3 N = normal(p, 0.01);
  vec3 L = normalize(lightPos - p);

  float dotLN = dot(L, N);

  if (dotLN < 0.0) {
    return vec3(0.0, 0.0, 0.0);
  }

  return lightIntensity * (k_d * dotLN);
}

vec3 calcLights(light data, vec3 p, vec3 eye) {
  vec3 ambientColor = data.ambientIntencity * data.amibnetColor;
  vec3 phongColor = diffuseLight(data.directLightColor, p, eye, data.lightPosition, data.directLightIntencity);

  return ambientColor + phongColor;
}

mat3 calcLookAtMatrix(vec3 origin, vec3 target, float roll) {
  vec3 rr = vec3(sin(roll), cos(roll), 0.0);
  vec3 ww = normalize(target - origin);
  vec3 uu = cross(ww, rr);
  vec3 vv = cross(uu, ww);

  return mat3(uu, vv, ww);
}

void setColor(vec3 p, vec3 n, out vec3 color) {
  float a = smoothstep(0.440 * n.y, 0.816 * n.y, fbmM(p.xz));
  vec3 ground = vec3(0.046,0.043,0.100);
  color = mix(vec3(1.), ground, a);  
}

mat2 mm2(in float a){float c = cos(a), s = sin(a);return mat2(c,s,-s,c);}
float tri(in float x){return clamp(abs(fract(x)-.5),0.01,0.49);}
vec2 tri2(in vec2 p){return vec2(tri(p.x)+tri(p.y),tri(p.y+tri(p.x)));}

float fbmAurora(vec2 p, float spd) {
    float z = 1.8;
    float z2 = 2.5;
	float rz = 0.;
    p *= mm2(p.x * 0.06);
    vec2 bp = p;
	for (float i = 0.; i < 5.; i++ ) {
        vec2 dg = tri2(bp*1.85)*.75;
        dg *= mm2(u_time*spd);
        p -= dg/z2;

        bp *= 1.3;
        z2 *= .45;
        z *= .42;
		p *= 1.21 + (rz-1.0)*.02;
        
        rz += tri(p.x+tri(p.y))*z;
        p*= sin(u_time * 0.05) * cos(u_time * 0.01);
	}
    return clamp(1. / pow(rz * 20., 1.3), 0.,1.);
}


vec4 aurora( vec3 rd) {
    vec4 col = vec4(0);
    vec4 avgCol = vec4(0);    

    for (float i=0.; i < 50.; i++) {
        float of = 0.006*random(gl_FragCoord.xy)*smoothstep(0.,15., i);
        float pt = ((.8+pow(i,1.4)*.002)) / (rd.y * 2. + 0.4);
        pt -= of;
    	vec3 bpos = 5.5 + pt * rd;
        vec2 p = bpos.zx;
        float rzt = fbmAurora(p, 0.06);
        vec4 col2 = vec4(0,0,0, rzt);
        col2.rgb = (sin(1.-vec3(2.15,-.5, 1.2) +i * 0.043) * 0.5 + 0.5)*rzt;
        avgCol = mix(avgCol, col2, .5);
        col += avgCol * exp2(-i*0.065 - 2.5) * smoothstep(0., 5., i);
    }
    col *= (clamp(rd.y*15.+.4,0.,1.));
 
    return smoothstep(0.,1.1,pow(col,vec4(1.))*1.5);
}

vec3 stars(vec2 p) {
    float r = fbmL(p * 20.  );
    float isStar = step(0.707, r);
    return vec3(r) * isStar;
}

void setSkyColor(vec2 uv, out vec3 color, vec3 dir) {
   color = mix(vec3(0.006,0.026,0.095), vec3(0.007,0.011,0.035), uv.y);
   color += stars(dir.xz / dir.y);
   color += aurora(dir).rgb;
}

void mainImage( out vec4 fragColor, in vec2 fragCoord )
{
  vec2 uv = fragCoord.xy / u_resolution.xy;
  vec2 p = (-u_resolution.xy + 2.0 * gl_FragCoord.xy) / u_resolution.y;

  float speed = 0.002;
  float terrainEndTime = abs(sin(u_time * speed));
  vec3 ro = vec3(mix(0., 100., terrainEndTime), 1.2, mix(0., 100., terrainEndTime)); // cause endless will have problems with floats
    
  float minHeight = 0.2 + 1.1 * fbmL(ro.xz);
  ro.y = minHeight;

  float a = sin(u_time * speed * 2.);	
  vec3 target = ro + vec3(mix(0.9, 1.64, (sin(u_time * 0.25  ))),
                          mix(-.1, .1, abs(sin(u_time * 0.125 + cos(u_time * 0.0125) ))),
                          a); // revert camera when near to end
  mat3 cam = calcLookAtMatrix(ro, target, 0.);
  vec3 rd = cam * normalize(vec3(p.xy, 1.064));

  vec3 color = vec3(0.0);
  float scene = raymarch(ro, rd);
  vec3 point = ro + scene * rd;
  if (scene > -1.) {
    light light1 = light(
      ro + vec3(10., 150., 100.), // light position
      vec3(0.931,0.975,0.906), 0.412, // ambient color - ambient intencity
      vec3(0.254,1.000,0.777), vec3(0.162,0.555,0.560)); // direct light color - direct light intencity


    vec3 nor = normal(point, scene);

    setColor(point, nor, color);

    color *= calcLights(light1, point, ro);
  } else {
    point = ro + scene * rd;
    setSkyColor(uv, color, rd);
  }

  color = pow(color, vec3(1. / 2.2)); // gamma correction
  color = smoothstep(0., 1.,color);

  fragColor = vec4(color,1.0);
}
