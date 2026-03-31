// Title:  Gimbal Harmonics
// Author: otaviogood
// URL:    https://www.shadertoy.com/view/llS3zd
// Date:   12-May-2015
// Desc:   I like visualizations of different frequencies next to each other. So I tried one with discs. I stuck with the same lighting and materials as my last shader.

/*--------------------------------------------------------------------------------------
License CC0 - http://creativecommons.org/publicdomain/zero/1.0/
To the extent possible under law, the author(s) have dedicated all copyright and related and neighboring rights to this software to the public domain worldwide. This software is distributed without any warranty.
----------------------------------------------------------------------------------------
^This means do anything you want with this code. Because we are programmers, not lawyers.

-Otavio Good
*/

float localTime = 0.0;
float marchCount;

float PI=3.14159265;

vec3 saturate(vec3 a) { return clamp(a, 0.0, 1.0); }
vec2 saturate(vec2 a) { return clamp(a, 0.0, 1.0); }
float saturate(float a) { return clamp(a, 0.0, 1.0); }

vec3 RotateX(vec3 v, float rad)
{
  float cos = cos(rad);
  float sin = sin(rad);
  return vec3(v.x, cos * v.y + sin * v.z, -sin * v.y + cos * v.z);
}
vec3 RotateY(vec3 v, float rad)
{
  float cos = cos(rad);
  float sin = sin(rad);
  return vec3(cos * v.x - sin * v.z, v.y, sin * v.x + cos * v.z);
}
vec3 RotateZ(vec3 v, float rad)
{
  float cos = cos(rad);
  float sin = sin(rad);
  return vec3(cos * v.x + sin * v.y, -sin * v.x + cos * v.y, v.z);
}


/*vec3 GetEnvColor(vec3 rayDir, vec3 sunDir)
{
	vec3 tex = texture(iChannel0, rayDir).xyz;
	tex = tex * tex;	// gamma correct
    return tex;
}*/

// This is a procedural environment map with a giant overhead softbox,
// 4 lights in a horizontal circle, and a bottom-to-top fade.
vec3 GetEnvColor2(vec3 rayDir, vec3 sunDir)
{
    // fade bottom to top so it looks like the softbox is casting light on a floor
    // and it's bouncing back
    vec3 final = vec3(1.0) * dot(-rayDir, sunDir) * 0.5 + 0.5;
    final *= 0.125;
    // overhead softbox, stretched to a rectangle
    if ((rayDir.y > abs(rayDir.x)*1.0) && (rayDir.y > abs(rayDir.z*0.25))) final = vec3(2.0)*rayDir.y;
    // fade the softbox at the edges with a rounded rectangle.
    float roundBox = length(max(abs(rayDir.xz/max(0.0,rayDir.y))-vec2(0.9, 4.0),0.0))-0.1;
    final += vec3(0.8)* pow(saturate(1.0 - roundBox*0.5), 6.0);
    // purple lights from side
    final += vec3(8.0,6.0,7.0) * saturate(0.001/(1.0 - abs(rayDir.x)));
    // yellow lights from side
    final += vec3(8.0,7.0,6.0) * saturate(0.001/(1.0 - abs(rayDir.z)));
    return vec3(final);
}

/*vec3 GetEnvColorReflection(vec3 rayDir, vec3 sunDir, float ambient)
{
	vec3 tex = texture(iChannel0, rayDir).xyz;
	tex = tex * tex;
    vec3 texBack = texture(iChannel0, rayDir).xyz;
    vec3 texDark = pow(texBack, vec3(50.0)).zzz;	// fake hdr texture
    texBack += texDark*0.5 * ambient;
    return texBack*texBack*texBack;
}*/

vec3 camPos = vec3(0.0), camFacing;
vec3 camLookat=vec3(0,0.0,0);

// polynomial smooth min (k = 0.1);
float smin( float a, float b, float k )
{
    float h = clamp( 0.5+0.5*(b-a)/k, 0.0, 1.0 );
    return mix( b, a, h ) - k*h*(1.0-h);
}

vec2 matMin(vec2 a, vec2 b)
{
	if (a.x < b.x) return a;
	else return b;
}

float spinTime;

const float thick = 0.85;
const float spacer = 0.01;
const float thickSpace = thick - spacer;
float Disc(vec3 p, float scale)
{
    float len = length(p.xz);
    // infinite cylinder
    float dist = len - scale;
    // cut out the inner disc
    dist = max(dist, -(len - thick*scale));
    //dist = max(dist, -(length(p.xz + vec2(thick*scale, 0.0)) - thick*scale*0.15));
    // cut off half the disc because that looks cool maybe.
    dist = max(dist, -p.z);
    // make it flat, but with nice rounded edges so reflections look good. (slow)
    dist = -smin(-dist, -(abs(p.y)-0.025), 0.015*scale);
    //dist = max(dist, abs(p.y)-0.025);
    return dist;
}

// Calculate the distance field that defines the object.
vec2 DistanceToObject(vec3 p)
{
    float dist = 1000000.0;
    float currentThick = 8.0;
    float harmonicTime = spinTime*0.125*3.14159*4.0;
    // make 15 discs inside each other
    for (int i = 0; i < 15; i++)
    {
        dist = min(dist, Disc(p, currentThick));
        p = RotateX(p, harmonicTime);
        p = RotateZ(p, harmonicTime);
        // scale down a level
        currentThick *= thickSpace;
    }
    vec2 distMat = vec2(dist, 0.0);
    // ball in center
    distMat = matMin(distMat, vec2(length(p) - 0.45, 6.0));
    return distMat;
}

// dirVec MUST BE NORMALIZED FIRST!!!!
float SphereIntersect(vec3 pos, vec3 dirVecPLZNormalizeMeFirst, vec3 spherePos, float rad)
{
    vec3 radialVec = pos - spherePos;
    float b = dot(radialVec, dirVecPLZNormalizeMeFirst);
    float c = dot(radialVec, radialVec) - rad * rad;
    float h = b * b - c;
    if (h < 0.0) return -1.0;
    return -b - sqrt(h);
}

void mainImage( out vec4 fragColor, in vec2 fragCoord )
{
    localTime = iTime - 0.0;
	// ---------------- First, set up the camera rays for ray marching ----------------
	vec2 uv = fragCoord.xy/iResolution.xy * 2.0 - 1.0;
    float zoom = 1.7;
    uv /= zoom;

	// Camera up vector.
	vec3 camUp=vec3(0,1,0);

	// Camera lookat.
	camLookat=vec3(0,0.0,0);

    // debugging camera
    float mx=iMouse.x/iResolution.x*PI*2.0-0.7 + localTime*3.1415 * 0.0625*0.666;
	float my=-iMouse.y/iResolution.y*10.0 - sin(localTime * 0.31)*0.5;//*PI/2.01;
	camPos += vec3(cos(my)*cos(mx),sin(my),cos(my)*sin(mx))*(12.2);

	// Camera setup.
	vec3 camVec=normalize(camLookat - camPos);
	vec3 sideNorm=normalize(cross(camUp, camVec));
	vec3 upNorm=cross(camVec, sideNorm);
	vec3 worldFacing=(camPos + camVec);
	vec3 worldPix = worldFacing + uv.x * sideNorm * (iResolution.x/iResolution.y) + uv.y * upNorm;
	vec3 rayVec = normalize(worldPix - camPos);

	// ----------------------------------- Animate ------------------------------------
    localTime = iTime*0.125;
    // This is a wave function like a triangle wave, but with flat tops and bottoms.
    // period is 1.0
    float rampStep = min(3.0,max(1.0, abs((fract(localTime)-0.5)*1.0)*8.0))*0.5-0.5;
    rampStep = smoothstep(0.0, 1.0, rampStep);
    // lopsided triangle wave - goes up for 3 time units, down for 1.
    float step31 = (max(0.0, (fract(localTime+0.125)-0.25)) - min(0.0,(fract(localTime+0.125)-0.25))*3.0)*0.333;

    spinTime = step31 + localTime - 0.125;
	// --------------------------------------------------------------------------------
	vec2 distAndMat = vec2(0.5, 0.0);
	float t = 0.0;
	//float inc = 0.02;
	float maxDepth = 21.0;
	vec3 pos = vec3(0,0,0);
    marchCount = 0.0;
    // intersect with sphere first as optimization so we don't ray march more than is needed.
    float hit = SphereIntersect(camPos, rayVec, vec3(0.0), 8.0);
    if (hit >= 0.0)
    {
        t = hit;
        // ray marching time
        for (int i = 0; i < 120; i++)	// This is the count of the max times the ray actually marches.
        {
            pos = camPos + rayVec * t;
            // *******************************************************
            // This is _the_ function that defines the "distance field".
            // It's really what makes the scene geometry.
            // *******************************************************
            distAndMat = DistanceToObject(pos);
            // move along the ray
            t += distAndMat.x;
            if ((t > maxDepth) || (abs(distAndMat.x) < 0.0025)) break;
            //marchCount+= 1.0;
        }
    }
    else
    {
        t = maxDepth + 1.0;
        distAndMat.x = 1000000.0;
    }
    // --------------------------------------------------------------------------------
	// Now that we have done our ray marching, let's put some color on this geometry.

	vec3 sunDir = normalize(vec3(3.93, 10.82, -1.5));
	vec3 finalColor = vec3(0.0);

	// If a ray actually hit the object, let's light it.
	//if (abs(distAndMat.x) < 0.75)
    if (t <= maxDepth)
	{
        // calculate the normal from the distance field. The distance field is a volume, so if you
        // sample the current point and neighboring points, you can use the difference to get
        // the normal.
        vec3 smallVec = vec3(0.005, 0, 0);
        vec3 normalU = vec3(distAndMat.x - DistanceToObject(pos - smallVec.xyy).x,
                           distAndMat.x - DistanceToObject(pos - smallVec.yxy).x,
                           distAndMat.x - DistanceToObject(pos - smallVec.yyx).x);

        vec3 normal = normalize(normalU);

        // calculate 2 ambient occlusion values. One for global stuff and one
        // for local stuff
        float ambientS = 1.0;
        ambientS *= saturate(DistanceToObject(pos + normal * 0.1).x*10.0);
        ambientS *= saturate(DistanceToObject(pos + normal * 0.2).x*5.0);
        ambientS *= saturate(DistanceToObject(pos + normal * 0.4).x*2.5);
        ambientS *= saturate(DistanceToObject(pos + normal * 0.8).x*1.25);
        float ambient = ambientS * saturate(DistanceToObject(pos + normal * 1.6).x*1.25*0.5);
        ambient *= saturate(DistanceToObject(pos + normal * 3.2).x*1.25*0.25);
        ambient *= saturate(DistanceToObject(pos + normal * 6.4).x*1.25*0.125);
        ambient = max(0.035, pow(ambient, 0.3));	// tone down ambient with a pow and min clamp it.
        ambient = saturate(ambient);

        // calculate the reflection vector for highlights
        vec3 ref = reflect(rayVec, normal);
        ref = normalize(ref);

        // Trace a ray for the reflection
        float sunShadow = 1.0;
        float iter = 0.1;
        vec3 nudgePos = pos + ref*0.3;// normal*0.02;	// don't start tracing too close or inside the object
		for (int i = 0; i < 40; i++)
        {
            float tempDist = DistanceToObject(nudgePos + ref * iter).x;
	        sunShadow *= saturate(tempDist*50.0);
            if (tempDist <= 0.0) break;
            //iter *= 1.5;	// constant is more reliable than distance-based
            iter += max(0.00, tempDist)*1.0;
            if (iter > 9.0) break;
        }
        sunShadow = saturate(sunShadow);

        // ------ Calculate texture color ------
        vec3 texColor;
        texColor = vec3(1.0);// vec3(0.65, 0.5, 0.4)*0.1;
        texColor = vec3(0.85, 0.945 - distAndMat.y * 0.15, 0.93 + distAndMat.y * 0.35)*0.951;
        if (distAndMat.y == 6.0) texColor = vec3(0.91, 0.1, 0.41)*10.5;
        //texColor *= mix(vec3(0.3), vec3(1.0), tex3d(pos*0.5, normal).xxx);
        texColor = max(texColor, vec3(0.0));
        texColor *= 0.25;
        //texColor += vec3(1.0, 0.0, 0.0) * 8.0/length(pos);

        // ------ Calculate lighting color ------
        // Start with sun color, standard lighting equation, and shadow
        vec3 lightColor = vec3(0.0);// sunCol * saturate(dot(sunDir, normal)) * sunShadow*14.0;
        // sky color, hemisphere light equation approximation, ambient occlusion
        lightColor += vec3(0.1,0.35,0.95) * (normal.y * 0.5 + 0.5) * ambient * 0.2;
        // ground color - another hemisphere light
        lightColor += vec3(1.0) * ((-normal.y) * 0.5 + 0.5) * ambient * 0.2;


        // finally, apply the light to the texture.
        finalColor = texColor * lightColor;
       // if (distAndMat.y == 6.0) finalColor += vec3(0.91, 0.032, 0.01)*0.2;
        //if (distAndMat.y == ceil(mod(localTime, 4.0))) finalColor += vec3(0.0, 0.41, 0.72)*0.925;

        // reflection environment map - this is most of the light
        vec3 refColor = GetEnvColor2(ref, sunDir)*sunShadow;// * (length(normalU)/smallVec.x-0.875)*8.0;
        finalColor += refColor * 0.35 * ambient;// * sunCol * sunShadow * 9.0 * texColor.g;

        // fog
		finalColor = mix(vec3(1.0, 0.41, 0.41) + vec3(1.0), finalColor, exp(-t*0.0007));
        // visualize length of gradient of distance field to check distance field correctness
        //finalColor = vec3(0.5) * (length(normalU) / smallVec.x);
	}
    else
    {
	    finalColor = GetEnvColor2(rayVec, sunDir);// + vec3(0.1, 0.1, 0.1);
    }
    //finalColor += marchCount * vec3(1.0, 0.3, 0.91) * 0.001;

    // vignette?
    //finalColor *= vec3(1.0) * saturate(1.0 - length(uv/2.5));
    //finalColor *= 1.2;

	// output the final color with sqrt for "gamma correction"
	fragColor = vec4(sqrt(clamp(finalColor, 0.0, 1.0)),1.0);
}



