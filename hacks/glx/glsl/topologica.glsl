// Title:  Topologica
// Author: otaviogood
// URL:    https://www.shadertoy.com/view/4djXzz
// Date:   20-Aug-2014
// Desc:   At some point I stopped understanding exactly what makes this work, but it just kept getting cooler. Mostly, it's stepping through a low frequency noise function that ramps up to a 1/x pulse. But then there are lots of tweaks on top of that.

/*--------------------------------------------------------------------------------------
License CC0 - http://creativecommons.org/publicdomain/zero/1.0/
To the extent possible under law, the author(s) have dedicated all copyright and related and neighboring rights to this software to the public domain worldwide. This software is distributed without any warranty.
----------------------------------------------------------------------------------------
^ This means do ANYTHING YOU WANT with this code. Because we are programmers, not lawyers.
-Otavio Good
*/

// various noise functions
float Hash3d(vec3 uv)
{
    float f = uv.x + uv.y * 37.0 + uv.z * 521.0;
    return fract(cos(f*3.333)*100003.9);
}
float mixP(float f0, float f1, float a)
{
    return mix(f0, f1, a*a*(3.0-2.0*a));
}
const vec2 zeroOne = vec2(0.0, 1.0);
float noise(vec3 uv)
{
    vec3 fr = fract(uv.xyz);
    vec3 fl = floor(uv.xyz);
    float h000 = Hash3d(fl);
    float h100 = Hash3d(fl + zeroOne.yxx);
    float h010 = Hash3d(fl + zeroOne.xyx);
    float h110 = Hash3d(fl + zeroOne.yyx);
    float h001 = Hash3d(fl + zeroOne.xxy);
    float h101 = Hash3d(fl + zeroOne.yxy);
    float h011 = Hash3d(fl + zeroOne.xyy);
    float h111 = Hash3d(fl + zeroOne.yyy);
    return mixP(
        mixP(mixP(h000, h100, fr.x), mixP(h010, h110, fr.x), fr.y),
        mixP(mixP(h001, h101, fr.x), mixP(h011, h111, fr.x), fr.y)
        , fr.z);
}

float PI=3.14159265;
#define saturate(a) clamp(a, 0.0, 1.0)
// Weird for loop trick so compiler doesn't unroll loop
// By making the zero a variable instead of a constant, the compiler can't unroll the loop and
// that speeds up compile times by a lot.
#define ZERO_TRICK max(0, -iFrame)

float Density(vec3 p)
{
    float final = noise(p*0.06125);
    float other = noise(p*0.06125 + 1234.567);
    other -= 0.5;
    final -= 0.5;
    final = 0.1/(abs(final*final*other));
    final += 0.5;
    return final*0.0001;
}

void mainImage( out vec4 fragColor, in vec2 fragCoord )
{
	// ---------------- First, set up the camera rays for ray marching ----------------
	vec2 uv = fragCoord.xy/iResolution.xy * 2.0 - 1.0;

	// Camera up vector.
	vec3 camUp=vec3(0,1,0); // vuv

	// Camera lookat.
	vec3 camLookat=vec3(0,0.0,0);	// vrp

	float mx=iMouse.x/iResolution.x*PI*2.0 + iTime * 0.01;
	float my=-iMouse.y/iResolution.y*10.0 + sin(iTime * 0.03)*0.2+0.2;//*PI/2.01;
	vec3 camPos=vec3(cos(my)*cos(mx),sin(my),cos(my)*sin(mx))*(200.2); 	// prp

	// Camera setup.
	vec3 camVec=normalize(camLookat - camPos);//vpn
	vec3 sideNorm=normalize(cross(camUp, camVec));	// u
	vec3 upNorm=cross(camVec, sideNorm);//v
	vec3 worldFacing=(camPos + camVec);//vcv
	vec3 worldPix = worldFacing + uv.x * sideNorm * (iResolution.x/iResolution.y) + uv.y * upNorm;//scrCoord
	vec3 relVec = normalize(worldPix - camPos);//scp

	// --------------------------------------------------------------------------------
	float t = 0.0;
	float inc = 0.02;
	float maxDepth = 70.0;
	vec3 pos = vec3(0,0,0);
    float density = 0.0;
	// ray marching time
    for (int i = ZERO_TRICK; i < 37; i++)	// This is the count of how many times the ray actually marches.
    {
        if ((t > maxDepth)) break;
        pos = camPos + relVec * t;
        float temp = Density(pos);

        inc = 1.9 + temp*0.05;	// add temp because this makes it look extra crazy!
        density += temp * inc;
        t += inc;
    }

	// --------------------------------------------------------------------------------
	// Now that we have done our ray marching, let's put some color on this.
	vec3 finalColor = vec3(0.01,0.1,1.0)* density*0.2;

	// output the final color with sqrt for "gamma correction"
	fragColor = vec4(sqrt(clamp(finalColor, 0.0, 1.0)),1.0);
}

