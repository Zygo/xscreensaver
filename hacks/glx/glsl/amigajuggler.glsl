// amijuggler, June 2026 Brian J. Bernstein <brian@dronefone.com>
//
// A real-time recreation of Eric Graham's 1987 Amiga "Juggler" demo: a GLSL
// port of his recursive ray tracer (rt1.c). The robot, the three balls, the
// checkerboard floor and the mirror reflections are all generated and ray
// traced procedurally, driven only by iTime -- no scene data is uploaded.
// Inspired by the awesome work done by http://github.com/AlphaPixel, and was
// the basis for getting the accuracy of the recreated GLSL code.
//
// This is the Shadertoy-API form (one self-contained "mainImage"), intended
// to run under XScreenSaver's "xshadertoy" host.
//
// Permission to use, copy, modify, distribute, and sell this software and its
// documentation for any purpose is hereby granted without fee, provided that
// the above copyright notice appear in all copies and that both that
// copyright notice and this permission notice appear in supporting
// documentation.  No representations are made about the suitability of this
// software for any purpose.  It is provided "as is" without express or
// implied warranty.
//

// ---------------------------------------------------------------------------
// Tuning parameters
// ---------------------------------------------------------------------------
#define BEAT        0.808       // seconds between throws (master tempo)
#define BALL_RADIUS 0.6
#define ARC_HEIGHT  3.8         // apex height of the high toss
#define ARM_UPPER   1.5         // shoulder -> elbow length
#define ARM_FORE    2.4         // elbow -> hand length
#define HIGH_F      (2.0/3.0)   // fraction of a ball's cycle spent in the high toss
#define BALL_LIFT   0.68        // ball center above the hand (so they touch)
#define BOB_AMP     0.22        // vertical body-bounce amplitude
#define SWAY_AMP    0.105       // horizontal sway amplitude
#define FOOT_SPREAD 0.95        // foot half-spread
#define KNEE_AMP    0.45        // knee-bend / crouch amplitude
#define SPIN_PERIOD 32.0        // seconds per full body rotation when spinning

// ---------------------------------------------------------------------------
// Nifty options (defaults: one juggler, mirror balls, no roll, fixed facing).
// ---------------------------------------------------------------------------
#define DO_SPIN   1             // 1 = rotate the robot about the vertical axis
#define DO_BOING  0             // 0 = mirror balls, 1 = red/white "boing" balls
#define BALL_ROLL 0.0           // boing tumble turns per cycle (0 = none)
#define JUGGLER_COUNT 1         // total jugglers; 1 = just the foreground one
const float STATIC_YAW = radians(-30.0);   // fixed facing (~7 o'clock)

// Background-crowd placement (only used when JUGGLER_COUNT > 1). Deterministic
// hashing stands in for the standalone's CPU rejection sampling.
#define BG_MIN_FROM_MAIN 10.0   // extras stay at least this far from the main figure
#define BG_FAN_RADIUS    48.0   // ... and at most this far (frontal fan)
#define BG_FAN_DEG       140.0  // angular spread of the fan in front of the camera
#define BG_WANDER_AMP    1.6    // slow positional drift amplitude

#define PI 3.14159265358979

// Surface types (match rt1.c).
#define T_DULL   0.0
#define T_BRIGHT 1.0
#define T_MIRROR 2.0
#define T_BOING  3.0

#define MAX_BOUNCES 6
const float BIG   = 1.0e10;
const float SMALL = 1.0e-3;

// World, baked from buildScene().
const vec3  OBS_POS     = vec3(-11.5, -2.6, 3.8);
const float CAM_ALT     = radians(1.0);
const float CAM_AZ      = radians(12.7);
const float CAM_FL      = 0.98;                  // 0.028 * 35 (robot.dat lens)
const vec3  TILE0       = vec3(1.5, 1.5, 0.0);   // yellow
const vec3  TILE1       = vec3(0.0, 1.5, 0.0);   // green
const float TILE_SIZE   = 4.0;
const vec3  AMBIENT     = vec3(0.30);
const vec3  SKY_HOR     = vec3(0.7, 0.7, 1.0);
const vec3  SKY_ZEN     = vec3(0.1, 0.1, 1.0);
const vec3  LAMP_POS    = vec3(-100.0, 50.0, 150.0);
const float LAMP_RADIUS = 15.0;
const vec3  LAMP_COLOR  = vec3(23117.66);        // lampColor * "lampfac" exposure

// Colors (robot.dat).
const vec3 C_LIMB  = vec3(1.0, 0.7, 0.7);
const vec3 C_TORSO = vec3(1.0, 0.1, 0.1);
const vec3 C_HEAD  = vec3(1.0, 0.7, 0.7);
const vec3 C_FACE  = vec3(0.2, 0.1, 0.1);
const vec3 C_EYE   = vec3(0.1, 0.1, 1.0);
const vec3 C_BALL  = vec3(0.9, 0.9, 0.9);

// ---------------------------------------------------------------------------
// Procedural scene buffer. For this GLSL version, we're rebuilding it per
// pixel into a global array instead of building a sphere list on the CPU and
// uploading it.
// ---------------------------------------------------------------------------
#define NSPH (JUGGLER_COUNT * 96)
vec4  gSph[NSPH * 2];
int   gNum;
int   gBallStart;       // index of the first juggled ball
float gRoll[3];         // per-ball boing tumble angle (radians)

void addS(vec3 p, float r, vec3 col, float type) {
    gSph[gNum * 2]     = vec4(p, r);
    gSph[gNum * 2 + 1] = vec4(col, type);
    gNum++;
}
vec3  sP(int i) { return gSph[i * 2].xyz; }
float sR(int i) { return gSph[i * 2].w; }
vec3  sC(int i) { return gSph[i * 2 + 1].xyz; }
float sT(int i) { return gSph[i * 2 + 1].w; }

// `count` interpolated beads between a and b (endpoints added by the caller).
void addChain(vec3 a, float ra, vec3 b, float rb, int count, vec3 col, float type) {
    for (int i = 1; i <= count; i++) {
        float u = float(i) / float(count + 1);
        addS(mix(a, b, u), ra + (rb - ra) * u, col, type);
    }
}

// Touching beads from a to b, radius tapering ra->rb (spacing shrinks with the
// beads so there are no gaps). Bounded loop for portability.
void addTaperedChain(vec3 a, float ra, vec3 b, float rb, vec3 col, float type) {
    float L = length(b - a);
    if (L < 1.0e-4) return;
    vec3 u = (b - a) / L;
    float d = ra;
    for (int i = 0; i < 24; i++) {
        if (d >= L) break;
        float r = ra + (rb - ra) * (d / L);
        addS(a + u * d, r, col, type);
        float rNext = ra + (rb - ra) * min((d + 2.0 * r) / L, 1.0);
        d += (r + rNext) * 0.85;
    }
}

// One bounce per beat: top on the hit (a=0), bottom mid-flight, back up by next.
float crouchPhase(float t) { return 0.5 - 0.5 * cos(2.0 * PI * (t / BEAT)); }

// Cone lean: full sway at the head, ~25% at the bottom of the torso, ~0 at hips.
float swayScale(float z) { return clamp(0.25 + 0.75 * (z - 3.3) / (6.1 - 3.3), 0.0, 1.0); }

// --- Arm kinematics (sgn = +1 left, -1 right) ---
struct Arm { vec3 shoulder; vec3 elbow; vec3 hand; };

Arm armPose(float sgn, float a, float bob, float sway) {
    float c     = cos(2.0 * PI * (a - 0.85));
    float theta = radians(30.0);                // upper arm ~5:00 (elbow ~90 deg)
    float phi   = radians(7.35 + 12.15 * c);    // forearm vertical stroke
    float psi   = radians(5.0);                 // slight outward yaw
    Arm r;
    r.shoulder = vec3(0.0, sgn * 0.7 + sway, 5.1 + bob);
    r.elbow = vec3(0.0,
                   r.shoulder.y + ARM_UPPER * sgn * sin(theta),
                   r.shoulder.z - ARM_UPPER * cos(theta));
    r.hand = vec3(r.elbow.x - ARM_FORE * cos(psi) * cos(phi),
                  r.elbow.y + ARM_FORE * sgn * sin(psi) * cos(phi),
                  r.elbow.z - ARM_FORE * sin(phi));
    return r;
}
vec3 handHome(float sgn) { return armPose(sgn, 0.0, 0.0, 0.0).hand; }

// One projectile flight A->B over time T under constant gravity g
vec3 flight(vec3 a, vec3 b, float tau, float T, float g) {
    float k  = tau / T;
    float vz = (b.z - a.z) / T + 0.5 * g * T;
    return vec3(a.x + (b.x - a.x) * k,
                a.y + (b.y - a.y) * k,
                a.z + vz * tau - 0.5 * g * tau * tau);
}

// Ball position: HIGH toss right->left (2/3 cycle), LOW pass left->right (1/3)
vec3 ballPos(int ball, float t, float bob, float sway) {
    float P = 3.0 * BEAT;
    float u = t / P + float(ball) / 3.0;
    u -= floor(u);
    vec3 left  = handHome(1.0);
    vec3 right = handHome(-1.0);
    left.z  += bob + BALL_LIFT;  right.z += bob + BALL_LIFT;
    left.y  += sway;             right.y += sway;
    float Thi = HIGH_F * P;
    float g   = 8.0 * ARC_HEIGHT / (Thi * Thi);
    if (u < HIGH_F) {
        float tau = (u / HIGH_F) * Thi;
        return flight(right, left, tau, Thi, g);
    } else {
        float Tlo = (1.0 - HIGH_F) * P;
        float tau = ((u - HIGH_F) / (1.0 - HIGH_F)) * Tlo;
        return flight(left, right, tau, Tlo, g);
    }
}

// Build the figure (local frame, standing at the origin) for local time tl.
// Returns the index where its three balls begin.
int buildFigure(float tl) {
    float crouch = crouchPhase(tl);
    float bob    = -BOB_AMP * crouch;
    float knee   =  KNEE_AMP * crouch;
    float tau    = tl / BEAT; tau -= floor(tau);
    float sway   = SWAY_AMP * sin(2.0 * PI * tau);
    float bt     = (DO_BOING == 1) ? T_BOING : T_MIRROR;

    // head, face, eyes, neck
    addS(vec3( 0.0,  sway * swayScale(6.1),  6.1  + bob), 0.5,  C_HEAD, T_BRIGHT);
    addS(vec3( 0.02, sway * swayScale(6.12), 6.12 + bob), 0.5,  C_FACE, T_BRIGHT);
    addS(vec3(-0.4,  0.2 + sway * swayScale(6.1), 6.1 + bob), 0.15, C_EYE, T_BRIGHT);
    addS(vec3(-0.4, -0.2 + sway * swayScale(6.1), 6.1 + bob), 0.15, C_EYE, T_BRIGHT);
    addS(vec3( 0.0,  sway * swayScale(5.5),  5.5  + bob), 0.2,  C_LIMB, T_BRIGHT);

    // torso chain (top sways with the head, bottom ~25%)
    vec3 t0 = vec3(0.0, sway * swayScale(4.6), 4.6 + bob);
    vec3 t1 = vec3(0.0, sway * swayScale(3.3), 3.3 + bob);
    addS(t0, 0.8, C_TORSO, T_BRIGHT);
    addS(t1, 0.6, C_TORSO, T_BRIGHT);
    addChain(t0, 0.8, t1, 0.6, 5, C_TORSO, T_BRIGHT);

    // legs
    for (int side = 0; side < 2; side++) {
        float sgn = (side == 0) ? 1.0 : -1.0;
        vec3 hip  = vec3(0.0, sgn * 0.6 + sway * swayScale(2.9), 2.9 + bob);
        vec3 foot = vec3(0.0, sgn * FOOT_SPREAD, 0.0);
        vec3 kne  = vec3(-0.25 - knee, sgn * 0.78 + sway * swayScale(1.5), 1.5 + bob * 0.5);
        addS(hip,  0.2, C_LIMB, T_BRIGHT);
        addS(kne,  0.2, C_LIMB, T_BRIGHT);
        addS(foot, 0.1, C_LIMB, T_BRIGHT);
        addChain(hip, 0.2, kne, 0.2, 6, C_LIMB, T_BRIGHT);
        addChain(kne, 0.2, foot, 0.1, 7, C_LIMB, T_BRIGHT);
    }

    // arms. both hands pop together once per beat
    float aArm = tl / BEAT; aArm -= floor(aArm);
    float armSway = sway * swayScale(5.1);
    for (int side = 0; side < 2; side++) {
        float sgn = (side == 0) ? 1.0 : -1.0;
        Arm arm = armPose(sgn, aArm, bob, armSway);
        addS(arm.shoulder, 0.2, C_LIMB, T_BRIGHT);
        addS(arm.elbow,    0.2, C_LIMB, T_BRIGHT);
        addS(arm.hand,     0.1, C_LIMB, T_BRIGHT);
        addChain(arm.shoulder, 0.2, arm.elbow, 0.2, 6, C_LIMB, T_BRIGHT);
        addTaperedChain(arm.elbow, 0.2, arm.hand, 0.1, C_LIMB, T_BRIGHT);
    }

    // the three juggled balls
    int ballStart = gNum;
    for (int i = 0; i < 3; i++)
        addS(ballPos(i, tl, bob, armSway), BALL_RADIUS, C_BALL, bt);
    return ballStart;
}

// Transform the spheres [start, gNum): scale about the origin, turn to face
// yaw, drop on the ground at ofs. (Only the most-recently-built juggler)
void transformRange(int start, float yaw, float scale, float ballScale,
                    int ballStart, vec2 ofs) {
    float c = cos(yaw), s = sin(yaw);
    for (int i = start; i < NSPH; i++) {
        if (i >= gNum) break;
        vec3 p = gSph[i * 2].xyz; float r = gSph[i * 2].w;
        p *= scale; r *= scale;
        if (i >= ballStart) r *= ballScale;
        float x = p.x, y = p.y;
        p.x = c * x - s * y + ofs.x;
        p.y = s * x + c * y + ofs.y;
        gSph[i * 2] = vec4(p, r);
    }
}

// Build one juggler at local time tl and place it; returns its ball-start index.
int appendJuggler(float tl, float yaw, float scale, vec2 ofs) {
    int start = gNum;
    int ballStart = buildFigure(tl);
    transformRange(start, yaw, scale, 1.0, ballStart, ofs);
    return ballStart;
}

// deterministic pseudo-random in [0,1) from a seed
float hash11(float p) { return fract(sin(p * 12.9898) * 43758.5453); }

void buildScene(float t) {
    gNum = 0; gBallStart = 0;

    // --- main juggler at the origin (fixed facing, or continuous spin) ---
    float mainYaw = STATIC_YAW;
#if DO_SPIN
    mainYaw += 2.0 * PI * (t / SPIN_PERIOD);
#endif
    gBallStart = appendJuggler(t, mainYaw, 1.0, vec2(0.0));

    // --- background crowd: each on its own clock, drifting slowly in a frontal
    //     fan, never near the main figure. Hash placement stands in for the
    //     standalone's CPU rejection sampling. (no-op when JUGGLER_COUNT == 1)
    for (int j = 1; j < JUGGLER_COUNT; j++) {
        float fj  = float(j);
        float rad = mix(BG_MIN_FROM_MAIN, BG_FAN_RADIUS, hash11(fj * 1.7 + 3.1));
        float ang = (hash11(fj * 2.3 + 9.7) - 0.5) * radians(BG_FAN_DEG);
        vec2  base = vec2(rad * cos(ang), rad * sin(ang));
        float off = hash11(fj * 4.1 + 1.3) * (3.0 * BEAT);   // phase offset
        float spd = 0.85 + 0.30 * hash11(fj * 5.9 + 6.2);    // tempo +/-15%
        float Tx  = mix(16.0, 28.0, hash11(fj * 7.3 + 2.8)); // wander periods
        float Ty  = mix(16.0, 28.0, hash11(fj * 9.2 + 0.5));
        float pxh = hash11(fj * 10.1 + 7.7) * 2.0 * PI;
        float pyh = hash11(fj * 11.5 + 5.1) * 2.0 * PI;
        float yawj = hash11(fj * 6.3 + 4.4) * 2.0 * PI;      // random facing
        vec2 ofs = base + BG_WANDER_AMP * vec2(sin(2.0 * PI * (t / Tx) + pxh),
                                               cos(2.0 * PI * (t / Ty) + pyh));
        appendJuggler(t * spd + off, yawj, 1.0, ofs);
    }

    // per-ball boing tumble for the MAIN juggler (whole turns keep it seamless)
    float P = 3.0 * BEAT;
    for (int i = 0; i < 3; i++) {
        float u = t / P + float(i) / 3.0; u -= floor(u);
        gRoll[i] = 2.0 * PI * BALL_ROLL * u;
    }
}

// ---------------------------------------------------------------------------
// Ray tracer (ported from Eric Graham's rt1.c / raytrace.frag)
// ---------------------------------------------------------------------------
float intSphere(vec3 ro, vec3 rd, vec3 c, float radius) {
    vec3 oc = ro - c;
    float a = dot(rd, rd);
    float b = 2.0 * dot(oc, rd);
    float cc = dot(oc, oc) - radius * radius;
    float d = b * b - 4.0 * a * cc;
    if (d <= 0.0) return -1.0;
    d = sqrt(d);
    float t = -(b + d) / (2.0 * a);
    if (t < SMALL) t = (d - b) / (2.0 * a);
    return (t > SMALL) ? t : -1.0;
}

float intGround(vec3 ro, vec3 rd) {
    if (rd.z == 0.0) return -1.0;
    float t = -ro.z / rd.z;
    return (t > SMALL) ? t : -1.0;
}

int gingham(vec3 p) {
    float a = radians(25.0);
    float xr = p.x * cos(a) - p.y * sin(a);
    float yr = p.x * sin(a) + p.y * cos(a);
    int ix = int(floor(xr / TILE_SIZE + 0.5));
    int iy = int(floor(yr / TILE_SIZE + 0.5));
    int m = (ix + iy) - 2 * ((ix + iy) / 2);   // (ix+iy) % 2, sign-safe
    return (m < 0) ? m + 2 : m;
}

bool shadowed(vec3 p, vec3 lp, int skip) {
    vec3 rd = lp - p;
    for (int k = 0; k < NSPH; k++) {
        if (k >= gNum) break;
        if (k == skip) continue;
        float t = intSphere(p, rd, sP(k), sR(k));
        if (t > SMALL && t < 1.0) return true;
    }
    return false;
}

vec3 shade(vec3 pos, vec3 normal, vec3 color, int skip) {
    const vec3 zenith = vec3(0.0, 0.0, 1.0);
    float diffuse = (dot(zenith, normal) + 2.3) * 0.303;
    vec3 brite = diffuse * AMBIENT * color;
    vec3 lp = LAMP_POS - pos;
    float cosi = dot(lp, normal);
    if (cosi > 0.0 && !shadowed(pos, LAMP_POS, skip)) {
        float r = length(lp);
        cosi = cosi / (r * r * r);
        brite += cosi * color * LAMP_COLOR;
    }
    return brite;
}

bool glint(vec3 pos, vec3 normal, vec3 incident, int skip) {
    vec3 lp = LAMP_POS - pos;
    if (dot(lp, normal) <= 0.0) return false;
    if (shadowed(pos, LAMP_POS, skip)) return false;
    vec3 refv = reflect(incident, normal);
    float t = dot(lp, refv);
    t = t * t / (dot(lp, lp) * dot(refv, refv));
    return t > 0.95;
}

vec3 skybrite(vec3 rd) {
    float sin2 = rd.z * rd.z / dot(rd, rd);
    float cos2 = 1.0 - sin2;
    return cos2 * SKY_HOR + sin2 * SKY_ZEN;
}

vec3 rotateY(vec3 v, float a) {
    float c = cos(a), s = sin(a);
    return vec3(c * v.x + s * v.z, v.y, -s * v.x + c * v.z);
}

vec3 boingColor(vec3 n) {
    const float N_LON = 12.0;
    const float N_LAT = 6.0;
    float lon = atan(n.y, n.x) / (2.0 * PI) + 0.5;
    float lat = asin(clamp(n.z, -1.0, 1.0)) / PI + 0.5;
    int iu = int(floor(lon * N_LON));
    int iv = int(floor(lat * N_LAT));
    int m = (iu + iv) - 2 * ((iu + iv) / 2);
    return (m == 0) ? vec3(1.0, 0.12, 0.12) : vec3(1.0, 1.0, 1.0);
}

vec3 trace(vec3 ro, vec3 rd) {
    vec3 throughput = vec3(1.0);
    for (int bounce = 0; bounce < MAX_BOUNCES; bounce++) {
        float tmin = BIG;
        int hit = -1;
        for (int k = 0; k < NSPH; k++) {
            if (k >= gNum) break;
            float t = intSphere(ro, rd, sP(k), sR(k));
            if (t > 0.0 && t < tmin) { tmin = t; hit = k; }
        }
        float tl = intSphere(ro, rd, LAMP_POS, LAMP_RADIUS);
        bool lampHit = (tl > 0.0 && tl < tmin);
        float tg = intGround(ro, rd);
        bool groundHit = (tg > 0.0 && tg < tmin);

        if (lampHit)
            return throughput * (LAMP_COLOR / (LAMP_RADIUS * LAMP_RADIUS));
        if (groundHit) {
            vec3 pos = ro + rd * tg;
            int k = gingham(pos);
            vec3 col = (k == 0) ? TILE0 : TILE1;
            return throughput * shade(pos, vec3(0.0, 0.0, 1.0), col, -1);
        }
        if (hit >= 0) {
            vec3 pos = ro + rd * tmin;
            vec3 normal = (pos - sP(hit)) / sR(hit);
            vec3 color = sC(hit);
            float ty = sT(hit);
            if (ty == T_BOING) {
                int bi = hit - gBallStart;
                float roll = (bi == 0) ? gRoll[0] : (bi == 1) ? gRoll[1] : gRoll[2];
                vec3 col = boingColor(rotateY(normal, -roll));
                vec3 lit = shade(pos, normal, col, hit);
                lit += col * 0.15;
                vec3 lp = LAMP_POS - pos;
                if (dot(lp, normal) > 0.0 && !shadowed(pos, LAMP_POS, hit)) {
                    vec3 refv = reflect(rd, normal);
                    float s = max(dot(normalize(refv), normalize(lp)), 0.0);
                    lit += vec3(0.45) * pow(s, 40.0);
                }
                return throughput * clamp(lit, 0.0, 1.0);
            }
            if (ty == T_MIRROR) {
                if (dot(normal, rd) >= 0.0) return throughput * vec3(0.0);
                throughput *= color;
                ro = pos + normal * SMALL;
                rd = reflect(rd, normal);
                continue;
            }
            if (ty == T_BRIGHT && glint(pos, normal, rd, hit))
                return throughput * vec3(1.0);
            return throughput * shade(pos, normal, color, hit);
        }
        return throughput * skybrite(rd);
    }
    return vec3(0.0);
}

void mainImage(out vec4 fragColor, in vec2 fragCoord) {
    buildScene(iTime);

    // camera (matches buildScene): fixed observer, ~35mm lens, 0.75 vertical
    // extent so framing is resolution-independent.
    vec3 viewdir = vec3(cos(CAM_AZ) * cos(CAM_ALT),
                        sin(CAM_AZ) * cos(CAM_ALT),
                        sin(CAM_ALT));
    vec3 uhat = vec3(sin(CAM_AZ), -cos(CAM_AZ), 0.0);
    vec3 vhat = vec3(-cos(CAM_AZ) * sin(CAM_ALT),
                     -sin(CAM_AZ) * sin(CAM_ALT),
                      cos(CAM_ALT));
    float py = 0.75 / iResolution.y;
    float x = (fragCoord.x - 0.5 * iResolution.x) * py;
    float y = (fragCoord.y - 0.5 * iResolution.y) * py;
    vec3 rd = normalize(viewdir * CAM_FL + y * vhat + x * uhat);

    fragColor = vec4(clamp(trace(OBS_POS, rd), 0.0, 1.0), 1.0);
}

