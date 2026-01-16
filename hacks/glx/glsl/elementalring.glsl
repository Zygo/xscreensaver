// Title:  Elemental Ring
// Author: otaviogood
// URL:    https://www.shadertoy.com/view/MsVXDt
// Date:   19-Jul-2016
// Desc:   This shader was inspired by https://www.shadertoy.com/view/MtBSWR. I wanted to do a 3d version.

/*--------------------------------------------------------------------------------------
License CC0 - http://creativecommons.org/publicdomain/zero/1.0/
To the extent possible under law, the author(s) have dedicated all copyright and related and neighboring rights to this software to the public domain worldwide. This software is distributed without any warranty.
----------------------------------------------------------------------------------------
^ This means do ANYTHING YOU WANT with this code. Because we are programmers, not lawyers.
-Otavio Good
*/

// ---------------- Config ----------------
// This is an option that lets you render high quality frames for screenshots. It enables
// stochastic antialiasing and motion blur automatically for any shader.
//#define NON_REALTIME_HQ_RENDER
const float frameToRenderHQ = 11.0; // Time in seconds of frame to render
const float antialiasingSamples = 16.0; // 16x antialiasing - too much might make the shader compiler angry.

//#define MANUAL_CAMERA


// --------------------------------------------------------
// These variables are for the non-realtime block renderer.
float localTime = 0.0;
float seed = 1.0;

// ---- noise functions ----
float v31(vec3 a)
{
    return a.x + a.y * 37.0 + a.z * 521.0;
}
float v21(vec2 a)
{
    return a.x + a.y * 37.0;
}
float Hash11(float a)
{
    return fract(sin(a)*10403.9);
}
float Hash21(vec2 uv)
{
    float f = uv.x + uv.y * 37.0;
    return fract(sin(f)*104003.9);
}
vec2 Hash22(vec2 uv)
{
    float f = uv.x + uv.y * 37.0;
    return fract(cos(f)*vec2(10003.579, 37049.7));
}
vec2 Hash12(float f)
{
    return fract(cos(f)*vec2(10003.579, 37049.7));
}
float Hash1d(float u)
{
    return fract(sin(u)*143.9);	// scale this down to kill the jitters
}
float Hash2d(vec2 uv)
{
    float f = uv.x + uv.y * 37.0;
    return fract(sin(f)*104003.9);
}
float Hash3d(vec3 uv)
{
    float f = uv.x + uv.y * 37.0 + uv.z * 521.0;
    return fract(sin(f)*110003.9);
}

const float PI=3.14159265;

vec3 saturate(vec3 a) { return clamp(a, 0.0, 1.0); }
vec2 saturate(vec2 a) { return clamp(a, 0.0, 1.0); }
float saturate(float a) { return clamp(a, 0.0, 1.0); }

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

// ---- shapes defined by distance fields ----
// See this site for a reference to more distance functions...
// https://iquilezles.org/articles/distfunctions
// box distance field
float sdBox(vec3 p, vec3 radius)
{
  vec3 dist = abs(p) - radius;
  return min(max(dist.x, max(dist.y, dist.z)), 0.0) + length(max(dist, 0.0));
}

// simple cylinder distance field
float cyl(vec2 p, float r)
{
    return length(p) - r;
}

const float TAU = 2.0 * PI;

float glow = 0.0;
// This is the function that makes the geometry.
// The input is a position in space.
// The output is the distance to the nearest surface and a material number
vec2 DistanceToObject(vec3 p)
{
    float time = localTime*2.0;
    float cylRadBig = 1.0;
    float cylRadSmall = 0.05;
    float freq = 4.0;
    float braidRad = 0.15;
    float angle = atan(p.z, p.x);
    float cylDist = length(p.xz) - cylRadBig;
    vec3 cylWarp = vec3(cylDist, p.y, angle);
    float amp = sin(angle + time) * 0.5 + 0.5;

    float theta = angle*freq;
    vec2 wave1 = vec2(sin(theta), cos(theta)) * braidRad;
    wave1 *= amp;
    //float d = length(cylWarp.xy + wave1) - cylRadSmall;
    float d = sdBox(vec3(cylWarp.xy + wave1, 0.0), vec3(cylRadSmall));
    float final = d;

    theta = angle*freq + TAU / 3.0;
    vec2 wave2 = vec2(sin(theta), cos(theta)) * braidRad;
    wave2 *= amp;
    //d = length(cylWarp.xy + wave2) - cylRadSmall;
    d = sdBox(vec3(cylWarp.xy + wave2, 0.0), vec3(cylRadSmall));
    final = smin(final, d, 0.1);

    theta = angle*freq - TAU / 3.0;
    vec2 wave3 = vec2(sin(theta), cos(theta)) * braidRad;
    wave3 *= amp;
    //d = length(cylWarp.xy + wave3) - cylRadSmall;
    d = sdBox(vec3(cylWarp.xy + wave3, 0.0), vec3(cylRadSmall));
    final = smin(final, d, 0.1);

    vec2 matd = vec2(final, fract((angle+time) / TAU+0.5));
    float sliver = cyl(cylWarp.xy, 0.03);
    glow += 1.0 / (sliver * sliver * sliver * sliver + 1.0);
    //sliver = max(sliver, abs(fract(cylWarp.z*freq-2.0*localTime)-0.5)-0.3);
    matd = matMin(matd, vec2(sliver, -1.0));
    return matd;
}

// Input is UV coordinate of pixel to render.
// Output is RGB color.
vec3 RayTrace(in vec2 fragCoord )
{
    glow = 0.0;
	// -------------------------------- animate ---------------------------------------
	vec3 camPos, camUp, camLookat;
	// ------------------- Set up the camera rays for ray marching --------------------
    // Map uv to [-1.0..1.0]
	vec2 uv = fragCoord.xy/iResolution.xy * 2.0 - 1.0;
    float zoom = 2.2;
    uv /= zoom;

    // Camera up vector.
	camUp=vec3(0,1,0);

	// Camera lookat.
	camLookat=vec3(0);

    // debugging camera
    float mx=iMouse.x/iResolution.x*PI*2.0;
	float my=-iMouse.y/iResolution.y*10.0;
#ifndef MANUAL_CAMERA
    camPos = vec3(0.0);
    camPos.y = sin(localTime*0.125)*3.0;
    camPos.z = cos(localTime*0.125)*3.0;
    camUp.y = camPos.z;
    camUp.z = -camPos.y;
    camUp = normalize(camUp);
#else
	camPos = vec3(cos(my)*cos(mx),sin(my),cos(my)*sin(mx))*3.0;
#endif

	// Camera setup.
	vec3 camVec=normalize(camLookat - camPos);
	vec3 sideNorm=normalize(cross(camUp, camVec));
	vec3 upNorm=cross(camVec, sideNorm);
	vec3 worldFacing=(camPos + camVec);
	vec3 worldPix = worldFacing + uv.x * sideNorm * (iResolution.x/iResolution.y) + uv.y * upNorm;
	vec3 rayVec = normalize(worldPix - camPos);

	// ----------------------------- Ray march the scene ------------------------------
	vec2 distMat = vec2(1.0, 0.0);
	float t = 0.0 + Hash2d(uv)*1.6;	// random dither glow by moving march count start position
	const float maxDepth = 6.0; // farthest distance rays will travel
	vec3 pos = vec3(0,0,0);
    const float smallVal = 0.000625;
    float marchCount = 0.0;
	// ray marching time
    for (int i = 0; i < 80; i++)	// This is the count of the max times the ray actually marches.
    {
        // Step along the ray.
        pos = camPos + rayVec * t;
        // This is _the_ function that defines the "distance field".
        // It's really what makes the scene geometry. The idea is that the
        // distance field returns the distance to the closest object, and then
        // we know we are safe to "march" along the ray by that much distance
        // without hitting anything. We repeat this until we get really close
        // and then break because we have effectively hit the object.
        distMat = DistanceToObject(pos);

        // Move along the ray.
        // Leave room for error by multiplying in case distance function isn't exact.
        t += distMat.x * 0.8;
        // If we are very close to the object, let's call it a hit and exit this loop.
        if ((t > maxDepth) || (abs(distMat.x) < smallVal)) break;

        // Glow if we're close to the part of the ring with the braid.
        float cyc = (-sin(distMat.y * TAU))*0.5+0.7;
        // This function is similar to a gaussian fall-off of glow when you're close
        // to an object.
        // http://thetamath.com/app/y=(1)/((x*x+1))
        marchCount += cyc / (distMat.x * distMat.x + 1.0);
    }

	// --------------------------------------------------------------------------------
	// Now that we have done our ray marching, let's put some color on this geometry.

    // Save off ray-march glows so they don't get messed up when we call the distance
    // function again to get the normal
	float glowSave = glow;
    float marchSave = marchCount;
    marchCount = 0.0;
    glow = 0.0;

    // default to blueish background color.
	vec3 finalColor = vec3(0.09, 0.15, 0.35);

	// If a ray actually hit the object, let's light it.
    if (t <= maxDepth)
	{
        // calculate the normal from the distance field. The distance field is a volume, so if you
        // sample the current point and neighboring points, you can use the difference to get
        // the normal.
        vec3 smallVec = vec3(smallVal, 0, 0);
        vec3 normalU = vec3(distMat.x - DistanceToObject(pos - smallVec.xyy).x,
                           distMat.x - DistanceToObject(pos - smallVec.yxy).x,
                           distMat.x - DistanceToObject(pos - smallVec.yyx).x);

        vec3 texColor = vec3(0.0, 0.0, 0.1);
        if (distMat.y < 0.0) texColor = vec3(0.6, 0.3, 0.1)*110.0;

        finalColor = texColor;
        // visualize length of gradient of distance field to check distance field correctness
        //finalColor = vec3(0.5) * (length(normalU) / smallVec.x);
	}
    // add the ray marching glows
    finalColor += vec3(0.3, 0.5, 0.9) * glowSave*0.00625;
    finalColor += vec3(1.0, 0.5, 0.3) * marchSave*0.05;

    // vignette
    finalColor *= vec3(1.0) * saturate(1.0 - length(uv/2.5));

	// output the final color without gamma correction - will do gamma later.
	return vec3(saturate(finalColor));
}

#ifdef NON_REALTIME_HQ_RENDER
// This function breaks the image down into blocks and scans
// through them, rendering 1 block at a time. It's for non-
// realtime things that take a long time to render.

// This is the frame rate to render at. Too fast and you will
// miss some blocks.
const float blockRate = 20.0;
void BlockRender(in vec2 fragCoord)
{
    // blockSize is how much it will try to render in 1 frame.
    // adjust this smaller for more complex scenes, bigger for
    // faster render times.
    const float blockSize = 64.0;
    // Make the block repeatedly scan across the image based on time.
    float frame = floor(iTime * blockRate);
    vec2 blockRes = floor(iResolution.xy / blockSize) + vec2(1.0);
    // ugly bug with mod.
    //float blockX = mod(frame, blockRes.x);
    float blockX = fract(frame / blockRes.x) * blockRes.x;
    //float blockY = mod(floor(frame / blockRes.x), blockRes.y);
    float blockY = fract(floor(frame / blockRes.x) / blockRes.y) * blockRes.y;
    // Don't draw anything outside the current block.
    if ((fragCoord.x - blockX * blockSize >= blockSize) ||
    	(fragCoord.x - (blockX - 1.0) * blockSize < blockSize) ||
    	(fragCoord.y - blockY * blockSize >= blockSize) ||
    	(fragCoord.y - (blockY - 1.0) * blockSize < blockSize))
    {
        discard;
    }
}
#endif

void mainImage( out vec4 fragColor, in vec2 fragCoord )
{
#ifdef NON_REALTIME_HQ_RENDER
    // Optionally render a non-realtime scene with high quality
    BlockRender(fragCoord);
#endif

    // Do a multi-pass render
    vec3 finalColor = vec3(0.0);
#ifdef NON_REALTIME_HQ_RENDER
    for (float i = 0.0; i < antialiasingSamples; i++)
    {
        const float motionBlurLengthInSeconds = 1.0 / 60.0;
        // Set this to the time in seconds of the frame to render.
	    localTime = frameToRenderHQ;
        // This line will motion-blur the renders
        localTime += Hash11(v21(fragCoord + seed)) * motionBlurLengthInSeconds;
        // Jitter the pixel position so we get antialiasing when we do multiple passes.
        vec2 jittered = fragCoord.xy + vec2(
            Hash21(fragCoord + seed),
            Hash21(fragCoord*7.234567 + seed)
            );
        // don't antialias if only 1 sample.
        if (antialiasingSamples == 1.0) jittered = fragCoord;
        // Accumulate one pass of raytracing into our pixel value
	    finalColor += RayTrace(jittered);
        // Change the random seed for each pass.
	    seed *= 1.01234567;
    }
    // Average all accumulated pixel intensities
    finalColor /= antialiasingSamples;
#else
    // Regular real-time rendering
    localTime = iTime;
    finalColor = RayTrace(fragCoord);
#endif

    fragColor = vec4(sqrt(clamp(finalColor, 0.0, 1.0)),1.0);
}



