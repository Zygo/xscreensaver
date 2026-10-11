// Title:  Synthwave City
// Author: 3w36zj6
// URL:    https://www.shadertoy.com/view/7lKyDD
// Date:   26-Aug-2022
// Desc:   Original: https://www.shadertoy.com/view/3t3GDB

// Original: https://www.shadertoy.com/view/3t3GDB
// Original License: CC BY 3.0
// Original Author: Jan Mróz (jaszunio15)

float sdSkyscraper(vec2 p, float w, float h)
{
  vec2 k1 = vec2(0.0, h);
  vec2 k2 = vec2(-w, h);
  p.x = abs(p.x);
  vec2 ca = p - vec2(0.0, h);
  vec2 cb = p - k1 + k2;
  float s = (cb.x < 0.0 && ca.y < 0.0) ? - 1.0 : 1.0;
  return s * (dot(ca, ca));
}

float sun(vec2 uv, float battery)
{
  float val = smoothstep(0.7, 0.69, length(uv));
  float bloom = smoothstep(0.7, 0.0, length(uv));
  float cut = 5.0 * sin((uv.y + iTime * 0.2 * (1.02)) * 60.0)
    + clamp(uv.y * 15.0, -6.0, 6.0);
  cut = clamp(cut, 0.0, 1.0);
  return clamp(val * cut, 0.0, 1.0) + bloom * 0.6;
}

float grid(vec2 uv, float battery)
{
  vec2 size = vec2(uv.y, uv.y * uv.y * 0.2) * 0.01;
  uv += vec2(0.0, iTime * 4.0 * (battery + 0.05));
  uv = abs(fract(uv) - 0.5);
  vec2 lines = smoothstep(size, vec2(0.0), uv);
  lines += smoothstep(size * 5.0, vec2(0.0), uv) * 0.4 * battery;
  return clamp(lines.x + lines.y, 0.0, 3.0);
}

void mainImage(out vec4 fragColor, in vec2 fragCoord)
{
  vec2 uv = (2.0 * fragCoord.xy - iResolution.xy) / iResolution.y;
  float battery = 1.0;

  float fog = smoothstep(0.2, -0.05, abs(uv.y + 0.2));
  vec3 col = vec3(0.0, 0.1, 0.2);
  if (uv.y < -0.2)
  {
    uv.y = 3.0 / (abs(uv.y + 0.2) + 0.05);
    uv.x *= uv.y * 1.0;
    float gridVal = grid(uv, battery);
    col = mix(col, vec3(1.0, 0.25, 0.5), gridVal);
  }
  else
  {
    uv.y -= 0.34;
    col = vec3(1.0, 0.4, 0.4);
    float sunVal = sun(uv, battery);
    col = mix(col, vec3(1.0, 0.85, 0.3), uv.y * 2.5 + 0.2);
    col = mix(vec3(0.0, 0.0, 0.0), col, sunVal);
    //uv.y += 0.37;
    col += vec3(0.1 + 1.25 * (1.0 - smoothstep(-0.2, 0.8, (0.2 + uv.y))), 0.15 * (1.0 - smoothstep(-0.2, 0.8, (0.2 + uv.y))), 0.7 - 0.45 * (smoothstep(-0.2, 0.8, (0.2 + uv.y))));
    //col += vec3(0.75,0.1,0.45);

    // bldg
    uv.y -= 0.2;
    float bldgD = max(-uv.y * 1.2 + 0.18, 0.0);
    float bldgVal1 = sdSkyscraper(uv + vec2(0.1 * mod(0.0 + iTime, 40.0) - 2.0, 0.5), 0.1, 0.15);
    col = mix(col, mix(vec3(0.0, 0.0, 0.25), vec3(1.0, 0.0, 0.5), bldgD), step(bldgVal1, 0.0));
    float bldgVal2 = sdSkyscraper(uv + vec2(0.1 * mod(2.5 + iTime, 40.0) - 2.0, 0.5), 0.1, 0.2);
    col = mix(col, mix(vec3(0.0, 0.0, 0.25), vec3(1.0, 0.0, 0.5), bldgD), step(bldgVal2, 0.0));
    float bldgVal3 = sdSkyscraper(uv + vec2(0.1 * mod(6.0 + iTime, 40.0) - 2.0, 0.5), 0.2, 0.05);
    col = mix(col, mix(vec3(0.0, 0.0, 0.25), vec3(1.0, 0.0, 0.5), bldgD), step(bldgVal3, 0.0));
    float bldgVal4 = sdSkyscraper(uv + vec2(0.1 * mod(0.0 + iTime, 40.0) - 2.0, 0.5), 0.02, 0.35);
    col = mix(col, mix(vec3(0.0, 0.0, 0.25), vec3(1.0, 0.0, 0.5), bldgD), step(bldgVal4, 0.0));

    float bldgVal5 = sdSkyscraper(uv + vec2(0.1 * mod(10.5 + iTime, 40.0) - 2.0, 0.5), 0.2, 0.25);
    col = mix(col, mix(vec3(0.0, 0.0, 0.25), vec3(1.0, 0.0, 0.5), bldgD), step(bldgVal5, 0.0));
    float bldgVal6 = sdSkyscraper(uv + vec2(0.1 * mod(13.0 + iTime, 40.0) - 2.0, 0.5), 0.2, 0.12);
    col = mix(col, mix(vec3(0.0, 0.0, 0.25), vec3(1.0, 0.0, 0.5), bldgD), step(bldgVal6, 0.0));
    float bldgVal7 = sdSkyscraper(uv + vec2(0.1 * mod(16.0 + iTime, 40.0) - 2.0, 0.5), 0.05, 0.2);
    col = mix(col, mix(vec3(0.0, 0.0, 0.25), vec3(1.0, 0.0, 0.5), bldgD), step(bldgVal7, 0.0));
    float bldgVal8 = sdSkyscraper(uv + vec2(0.1 * mod(17.0 + iTime, 40.0) - 2.0, 0.5), 0.08, 0.3);
    col = mix(col, mix(vec3(0.0, 0.0, 0.25), vec3(1.0, 0.0, 0.5), bldgD), step(bldgVal8, 0.0));

    float bldgVal9 = sdSkyscraper(uv + vec2(0.1 * mod(20.0 + iTime, 40.0) - 2.0, 0.5), 0.18, 0.4);
    col = mix(col, mix(vec3(0.0, 0.0, 0.25), vec3(1.0, 0.0, 0.5), bldgD), step(bldgVal9, 0.0));
    float bldgVal10 = sdSkyscraper(uv + vec2(0.1 * mod(24.5 + iTime, 40.0) - 2.0, 0.5), 0.23, 0.02);
    col = mix(col, mix(vec3(0.0, 0.0, 0.25), vec3(1.0, 0.0, 0.5), bldgD), step(bldgVal10, 0.0));
    float bldgVal11 = sdSkyscraper(uv + vec2(0.1 * mod(27.8 + iTime, 40.0) - 2.0, 0.5), 0.05, 0.315);
    col = mix(col, mix(vec3(0.0, 0.0, 0.25), vec3(1.0, 0.0, 0.5), bldgD), step(bldgVal11, 0.0));
    float bldgVal12 = sdSkyscraper(uv + vec2(0.1 * mod(30.0 + iTime, 40.0) - 2.0, 0.5), 0.14, 0.1);
    col = mix(col, mix(vec3(0.0, 0.0, 0.25), vec3(1.0, 0.0, 0.5), bldgD), step(bldgVal12, 0.0));

    float bldgVal13 = sdSkyscraper(uv + vec2(0.1 * mod(32.2 + iTime, 40.0) - 2.0, 0.5), 0.06, 0.03);
    col = mix(col, mix(vec3(0.0, 0.0, 0.25), vec3(1.0, 0.0, 0.5), bldgD), step(bldgVal13, 0.0));
    float bldgVal14 = sdSkyscraper(uv + vec2(0.1 * mod(34.1 + iTime, 40.0) - 2.0, 0.5), 0.09, 0.02);
    col = mix(col, mix(vec3(0.0, 0.0, 0.25), vec3(1.0, 0.0, 0.5), bldgD), step(bldgVal14, 0.0));
    float bldgVal15 = sdSkyscraper(uv + vec2(0.1 * mod(37.0 + iTime, 40.0) - 2.0, 0.5), 0.1, 0.315);
    col = mix(col, mix(vec3(0.0, 0.0, 0.25), vec3(1.0, 0.0, 0.5), bldgD), step(bldgVal15, 0.0));
    float bldgVal16 = sdSkyscraper(uv + vec2(0.1 * mod(36.9 + iTime, 40.0) - 2.0, 0.5), 0.14, 0.27);
    col = mix(col, mix(vec3(0.0, 0.0, 0.25), vec3(1.0, 0.0, 0.5), bldgD), step(bldgVal16, 0.0));

    float bldgVal = min(min(min(min(bldgVal1, bldgVal2), min(bldgVal3, bldgVal4)), min(min(bldgVal5, bldgVal6), min(bldgVal7, bldgVal8))), min(min(min(bldgVal9, bldgVal10), min(bldgVal11, bldgVal12)), min(min(bldgVal13, bldgVal14), min(bldgVal15, bldgVal16))));

    //col += mix( col, mix(vec3(1.0, 0.12, 0.8), vec3(0.0, 0.0, 0.2), clamp(uv.y * 3.5 + 3.0, 0.0, 1.0)), step(0.0, bldgVal) );
  }

  col += fog * fog * fog;
  col = mix(vec3(0.75, 0.1, 0.45) * 0.2, col, battery * 0.7);

  fragColor = vec4(col, 1.0);
}

