/* Juggler3D, Copyright (c) 2005-2008 Brian Apps <brian@jugglesaver.co.uk>
 *
 * Permission to use, copy, modify, distribute, and sell this software and its
 * documentation for any purpose is hereby granted without fee, provided that
 * the above copyright notice appear in all copies and that both that copyright
 * notice and this permission notice appear in supporting documentation.  No
 * representations are made about the suitability of this software for any
 * purpose.  It is provided "as is" without express or implied warranty. */

#undef countof
#define countof(x) (sizeof((x))/sizeof((*x)))

#define DEFAULTS    \
    "*delay: 20000\n*showFPS: False\n*wireframe: False\n"

# define refresh_juggler3d 0
# define release_juggler3d 0
#include "xlockmore.h"
#include "gltrackball.h"

#ifdef USE_GL /* whole file */

/* A selection of macros to make functions from math.h return single precision
 * numbers.  Arguably it's better to work at a higher precision and cast it
 * back but littering the code with casts makes it less readable -- without
 * the casts you can get tons of warnings from the compiler (particularily
 * MSVC which enables loss of precision warnings by default) */
 
#define cosf(a) (float)(cos((a)))
#define sinf(a) (float)(sin((a)))
#define tanf(a) (float)(tan((a)))
#define sqrtf(a) (float)(sqrt((a)))
#define powf(a, b) (float)(pow((a), (b)))

#undef max
#undef min

#define max(a, b) ((a) > (b) ? (a) : (b))
#define min(a, b) ((a) < (b) ? (a) : (b))


/******************************************************************************
 *
 * The code is broadly split into the following parts:
 *
 *  - Engine.  The process of determining the position of the juggler and 
 *        objects being juggled at an arbitrary point in time.  This is
 *        independent from any drawing code.
 *  - Sites.  The process of creating a random site swap pattern or parsing
 *        a Juggle Saver compatible siteswap for use by the engine.  For an
 *        introduction to juggling site swaps check out
 *         http://www.jugglingdb.com/
 *  - Rendering.  OpenGL drawing code that animates the juggler.
 *  - XScreenSaver.  Interface code to get thing working as a GLX hack.
 *  
 *****************************************************************************/


/*****************************************************************************
 *
 * Data structures
 *
 *****************************************************************************/

/* POS is used to represent the position of a hand when it catches or throws
 * an object; as well as the orientation of the object.  The rotation and
 * elevation are specified in degrees.  These angles are not normalised so that
 * it is possible to specify how the object spins and rotates as it is thrown
 * from the 'From' position to the 'To' position.
 * 
 * Because this is the position of the hand some translation is required with
 * rings and clubs to get the centre of rotation position. */

typedef struct
{
    float x;
    float y;
    float z;
    float Rot;
    float Elev;
} POS;


/* An array of THROW_INFOs are configured with each entry corresponding to the
 * position in the site swap (In fact we double up odd length patterns to ensure
 * there is left/right symmetry).  It allows us to quickly determine where an
 * object and the hands are at a given time.  The information is specified in
 * terms of throws, and positions where throws aren't present (0's and 2's) are
 * simply ignored.
 * 
 * TotalTime - The count of beats before this object is thrown again.  Typically
 *    this is the same as the weight of the throw but where an object is held it
 *    is longer.  e.g. the first throw of the site 64242.7. will be 10, 6 for
 *    throw and 4 (two 2's) for the carry.
 * TimeInAir - The weight of the throw.
 * PrevThrow - zero based index into array of THROW_INFOs of the previous throw.
 *     e.g. for the throw '8' in the site 345678..... the PrevThrow is 1
 *    (i.e. the 4)
 * FromPos, FromVelocity, ToPos, ToVelocity - The position and speeds at the
 *    start and end of the throw.  These are used to generate a spline while
 *    carrying an object and while moving the hand from a throw to a catch.
 * NextForHand - Number of beats before the hand that throws this object will
 *    throw another object.  This is always going to be at least 2.  When there
 *    are gaps in the pattern (0's) or holds (2's) NextForHand increases. */

typedef struct
{
    int TotalTime;
    int TimeInAir;
    int PrevThrow;

    POS FromPos;
    POS FromVelocity;
    POS ToPos;
    POS ToVelocity;

    int NextForHand;
} THROW_INFO;


/* OBJECT_POSITION works with the array of THROW_INFOs to allow us to determine
 * exactly where an object or hand is.
 *
 * TimeOffset - The total number of beats expired when the object was thrown.
 * ThrowIndex - The zero based index into the THROW_INFO array for the current
 *     throw.
 * ObjectType - One of the OBJECT_XX defines.
 * TotalTwist - Only relevant for OBJECT_BALL, this is the total amount the ball
 *     has twisted while in the air.  When segmented balls are drawn you see a 
 *     spinning effect similar to what happens when you juggle beanbags.  */

#define OBJECT_DEFAULT 0
#define OBJECT_BALL 1
#define OBJECT_CLUB 2
#define OBJECT_RING 3

typedef struct
{
    int TimeOffset;
    int ThrowIndex;
    float TotalTwist;
    int ObjectType;
} OBJECT_POSITION;


/* PATTERN_INFO is the main structure that holds the information about a 
 * juggling pattern. 
 *
 * pThrowInfo is an array of ThrowLen elements that describes each throw in the
 *     pattern.
 * pObjectInfo gives the current position of all objects at a given instant.
 *     These values are updated as the pattern is animated.
 * LeftHand and RightHand describe the current positions of each of the 
 *     juggler's hands.
 * MaxWeight is the maximum weight of the all throws in pThrowInfo.
 * Height and Alpha are parameters that describe how objects fall under the
 *     influence of gravity.  See SetHeightAndAlpha() for the gory details. */

typedef struct
{
    THROW_INFO* pThrowInfo;
    int ThrowLen;
    
    OBJECT_POSITION* pObjectInfo;
    int Objects;

    OBJECT_POSITION LeftHand;
    OBJECT_POSITION RightHand;
    
    int MaxWeight;

    float Height;
    float Alpha;
} PATTERN_INFO;


/* EXT_SITE_INFO is used to initialise a PATTERN_INFO object using a Juggle
 * Saver compatible site swap.  These contain additional information about the
 * type of object thrown, the positions of throw and catch etc. */

#define HAS_FROM_POS 1
#define HAS_TO_POS 2
#define HAS_SNATCH 4
#define HAS_SPINS 8

typedef struct
{
    unsigned Flags;
    int Weight;
    int ObjectType;
    POS FromPos;
    POS ToPos;
    float SnatchX;
    float SnatchY;
    int Spins;
} EXT_SITE_INFO;


/* RENDER_STATE is used to co-ordinate the OpenGL rendering of the juggler and
 * objects:
 * pPattern - The pattern to be juggled
 * CameraElev - The elevation angle (in degrees) that the camera is looking
 *    along.  0 is horizontal and a +ve angle is looking down.  This value
 *    should be between -90 and +90.
 * AspectRatio - Window width to height ratio.
 * DLStart - The number for the first display list created, any others directly
 *    follow this.
 * Time - Animation time (in beats)
 * TranslateAngle - Cumulative translation (in degrees) for the juggling figure.
 * SpinAngle- Cumulative spin (in degrees) for the juggling figure.
 */

typedef struct
{
    PATTERN_INFO* pPattern;
    float CameraElev;
    float AspectRatio;
    int DLStart;
    
    float Time;
    float TranslateAngle;
    float SpinAngle;
    
    trackball_state *trackball;
    Bool button_down_p;

} RENDER_STATE;


/*****************************************************************************
 *
 * Engine
 *
 ****************************************************************************
 *
 * The main purpose of the engine is to work out the exact position of all the
 * juggling objects and the juggler's hands at any point in time.  The motion
 * of the objects can be split into two parts: in the air and and being carried.
 *
 * While in the air, the motion is governed by a standard parabolic trajectory.
 * The only minor complication is that the engine has no fixed concept of
 * gravity, instead it using a term called Alpha that varies according to the
 * pattern (see SetHeightAndAlpha). 
 *
 * The motion while an object is carried comes from fitting a spline through the
 * catch and throw points and maintaining the catch and throw velocities at
 * each end.  In the simplest case this boils down to cubic Bezier spline.  The
 * only wrinkle occurs when a ball is being carried for a long time.  The simple 
 * cubic spline maths produces a curve that goes miles away -- here we do a
 * bit of reparameterisation so things stay within sensible bounds.
 * (On a related note, this scheme is _much_ simpler than the Juggle Saver
 * one.  Juggle Saver achieves 2nd order continuity and much care is taken
 * to avoid spline ringing.)
 * 
 * The motion of the hands is identical to the ball carrying code. It uses two
 * splines: one while an object is being carried; and another when it moves from
 * the previous throw to the next catch.
 */
 
static const float CARRY_TIME = 0.56f;
static const float PI = 3.14159265358979f;


/* While a ball is thrown it twists slighty about an axis, this routine gives
 * the total about of twist for a given ball throw. */
 
static float GetBallTwistAmount(const THROW_INFO* pThrow)
{
    if (pThrow->FromPos.x > pThrow->ToPos.x)
        return 18.0f * powf(pThrow->TimeInAir, 1.5);
    else
        return -18.0f * powf(pThrow->TimeInAir, 1.5);
}


static float NormaliseAngle(float Ang)
{
    if (Ang >= 0.0f)
    {
        int i = (int) (Ang + 180.0f) / 360;
        return Ang - 360.0f * i;
    }
    else
    {
        int i = (int)(180.0f - Ang) / 360;
        return Ang + i * 360.0f;
    }
}


/* The interpolate routine for ball carrying and hand motion.  We are given the
 * start (P0) and end (P1) points and the velocities at these points, the task
 * is to form a function P(t) such that:
 *    P(0) = P0
 *    P(TLen) = P1
 *    P'(0) = V0
 *    P'(TLen) = V1
 */

static POS InterpolatePosition(
    const POS* pP0, const POS* pV0, const POS* pP1, const POS* pV1,
    float TLen, float t)
{
    POS p;
    float a, b, c, d, tt, tc;
    
    /* The interpolation is based on a simple cubic that achieves 1st order
     * continuity at the end points.  However the spline can become too long if
     * the TLen parameter is large.  In this case we cap the curve's length (fix
     * the shape) and then reparameterise time to achieve the continuity
     * conditions. */

    tc = CARRY_TIME;
    
    if (TLen > tc)
    {
        /* The reparameterisation tt(t) gives:
         *  tt(0) = 0, tt(TLen) = tc, tt'(0) = 1, tt'(TLen) = 1
         * and means we can set t = tt(t), TLen = tc and then fall through
         * to use the normal cubic spline fit.
         *    
         * The reparameterisation is based on two piecewise quadratics, one
         * that goes from t = 0 to t = TLen / 2 and the other, mirrored in
         * tt and t that goes from t = TLen / 2 to t = TLen.
         * Because TLen > tc we can arrange for tt to be unique in the range if
         * we specify the quadratic in tt.  i.e. t = A * tt ^ 2 + B * tt + C.
         *
         * Considering the first piece and applying initial conditions.
         *   tt = 0 when t = 0   =>  C = 0
         *   tt' = 1 when t = 0   =>  B = 1
         *   tt = tc / 2 when t = TLen / 2  =>  A = 2 * (TLen - tc) / tc^2
         *
         * writing in terms of t
         *   tt = (-B + (B ^ 2 + 4At) ^ 0.5) / 2A
         * or
         *   tt = ((1 + 4At) ^ 0.5 - 1) / 2A */
        
        float A = 2.0f * (TLen - tc) / (tc * tc);
        
        if (t > TLen / 2.0f)
            t = tc - (sqrtf(1.0f + 4.0f * A * (TLen - t)) - 1.0f) / (2.0f * A);
        else
            t = (sqrtf(1.0f + 4.0f * A * t) - 1.0f) / (2.0f * A);
        
        TLen = tc;
    }
    
    /* The cubic spline takes the form:
     *   P(t) = p0 * a(t) + v0 * b(t) + p1 * c(t) + v1 * d(t)
     * where p0 is the start point, v0 the start velocity, p1 the end point and
     * v1 the end velocity.  a(t), b(t), c(t) and d(t) are cubics in t.
     * We can show that:
     *
     *  a(t) = 2 * (t / TLen) ^ 3 - 3 * (t / TLen) ^ 2 + 1
     *  b(t) = t ^ 3 / TLen ^ 2 - 2 * t ^ 2 / TLen + t
     *  c(t) = -2 * (t / TLen) ^ 3 + 3 * (t / TLen) ^ 2
     *  d(t) = t ^ 3 / TLen ^ 2 - t ^ 2 / TLen
     *
     * statisfy the boundary conditions:
     *    P(0) = p0, P(TLen) = p1, P'(0) = v0 and P'(TLen) = v1  */
    
    tt = t / TLen;
    
    a = tt * tt * (2.0f * tt - 3.0f) + 1.0f;
    b = t * tt * (tt - 2.0f) + t;
    c = tt * tt * (3.0f - 2.0f * tt);
    d = t * tt * (tt - 1.0f);

    p.x = a * pP0->x + b * pV0->x + c * pP1->x + d * pV1->x;
    p.y = a * pP0->y + b * pV0->y + c * pP1->y + d * pV1->y;
    p.z = a * pP0->z + b * pV0->z + c * pP1->z + d * pV1->z;

    p.Rot = a * NormaliseAngle(pP0->Rot) + b * pV0->Rot + 
        c * NormaliseAngle(pP1->Rot) + d * pV1->Rot;
    p.Elev = a * NormaliseAngle(pP0->Elev) + b * pV0->Elev + 
        c * NormaliseAngle(pP1->Elev) + d * pV1->Elev;

    return p;
}


static POS InterpolateCarry(
    const THROW_INFO* pThrow, const THROW_INFO* pNext, float t)
{
    float CT = CARRY_TIME + pThrow->TotalTime - pThrow->TimeInAir;
    return InterpolatePosition(&pThrow->ToPos, &pThrow->ToVelocity,
        &pNext->FromPos, &pNext->FromVelocity, CT, t);
}


/* Determine the position of the hand at a point in time. */

static void GetHandPosition(
    PATTERN_INFO* pPattern, int RightHand, float Time, POS* pPos)
{
    OBJECT_POSITION* pObj = 
        RightHand == 0 ? &pPattern->LeftHand : &pPattern->RightHand;
    THROW_INFO* pLastThrow;
    
    /* Upon entry, the throw information for the relevant hand may be out of
     * sync.  Therefore we advance through the pattern if required. */

    while (pPattern->pThrowInfo[pObj->ThrowIndex].NextForHand + pObj->TimeOffset 
        <= (int) Time)
    {
        int w = pPattern->pThrowInfo[pObj->ThrowIndex].NextForHand;
        pObj->TimeOffset += w;
        pObj->ThrowIndex = (pObj->ThrowIndex + w) % pPattern->ThrowLen;
    }

    pLastThrow = &pPattern->pThrowInfo[pObj->ThrowIndex];

    /* The TimeInAir will only ever be 2 or 0 if no object is ever thrown by
     * this hand.  In normal circumstances, 2's in the site swap are coalesced
     * and added to TotalTime of the previous throw.  0 is a hole and means that
     * an object isn't there.  In this case we just hold the hand still. */
    if (pLastThrow->TimeInAir == 2 || pLastThrow->TimeInAir == 0)
    {
        pPos->x = pLastThrow->FromPos.x;
        pPos->y = pLastThrow->FromPos.y;
    }
    else
    {
        /* The hand is either moving to catch the next object or carrying the
         * next object to its next throw position.  The way THROW_INFO is
         * structured means the relevant information for the object we're going
         * to catch is held at the point at which it was thrown 
         * (pNextThrownFrom).  We can't go straight for it and instead have to
         * look at the object we've about to throw next and work out where it
         * came from. */
        
        THROW_INFO* pNextThrow = &pPattern->pThrowInfo[
            (pObj->ThrowIndex + pLastThrow->NextForHand) % pPattern->ThrowLen];
        
        THROW_INFO* pNextThrownFrom = 
            &pPattern->pThrowInfo[pNextThrow->PrevThrow];
        
        /* tc is a measure of how long the object we're due to catch is being
         * carried for.  We use this to work out if we've actually caught it at
         * this moment in time. */
        
        float tc = CARRY_TIME + 
            pNextThrownFrom->TotalTime - pNextThrownFrom->TimeInAir;
        
        Time -= pObj->TimeOffset;

        if (Time > pLastThrow->NextForHand - tc)
        {
            /* carrying this ball to it's new location */
            *pPos = InterpolateCarry(pNextThrownFrom,
                pNextThrow, (Time - (pLastThrow->NextForHand - tc)));
        }
        else
        {
            /* going for next catch */
            *pPos = InterpolatePosition(
                &pLastThrow->FromPos, &pLastThrow->FromVelocity, 
                &pNextThrownFrom->ToPos, &pNextThrownFrom->ToVelocity,
                pLastThrow->NextForHand - tc, Time);
        }
    }
}


static float SinDeg(float AngInDegrees)
{
    return sinf(AngInDegrees * PI / 180.0f);
}


static float CosDeg(float AngInDegrees)
{
    return cosf(AngInDegrees * PI / 180.0f);
}


/* Offset the specified position to get the centre of the object based on the
 * the handle length and the current orientation */

static void OffsetHandlePosition(const POS* pPos, float HandleLen, POS* pResult)
{
    pResult->x = pPos->x + HandleLen * SinDeg(pPos->Rot) * CosDeg(pPos->Elev);
    pResult->y = pPos->y + HandleLen * SinDeg(pPos->Elev);
    pResult->z = pPos->z + HandleLen * CosDeg(pPos->Rot) * CosDeg(pPos->Elev);
    pResult->Elev = pPos->Elev;
    pResult->Rot = pPos->Rot;
}


static void GetObjectPosition(
    PATTERN_INFO* pPattern, int Obj, float Time, float HandleLen, POS* pPos)
{
    OBJECT_POSITION* pObj = &pPattern->pObjectInfo[Obj];
    THROW_INFO* pThrow;
    
    /* Move through the pattern, if required, such that pThrow corresponds to
     * the current throw for this object. */

    while (pPattern->pThrowInfo[pObj->ThrowIndex].TotalTime + pObj->TimeOffset
        <= (int) Time)
    {
        int w = pPattern->pThrowInfo[pObj->ThrowIndex].TotalTime;
        pObj->TimeOffset += w;
        pObj->TotalTwist = NormaliseAngle(pObj->TotalTwist + 
            GetBallTwistAmount(&pPattern->pThrowInfo[pObj->ThrowIndex]));
        
        pObj->ThrowIndex = (pObj->ThrowIndex + w) % pPattern->ThrowLen;
    }

    pThrow = &pPattern->pThrowInfo[pObj->ThrowIndex];

    if (pThrow->TimeInAir == 2 || pThrow->TimeInAir == 0)
    {
        *pPos = pThrow->FromPos;
        OffsetHandlePosition(pPos, HandleLen, pPos);
    }
    else
    {
        float tc = pThrow->TimeInAir - CARRY_TIME;
        float BallTwist = GetBallTwistAmount(pThrow);
        Time -= pObj->TimeOffset;
        if (Time < tc)
        {
            /* object in air */
            POS From, To;
            float t, b;

            t = Time / tc;
            
            OffsetHandlePosition(&pThrow->FromPos, HandleLen, &From);
            OffsetHandlePosition(&pThrow->ToPos, HandleLen, &To);

            b = (To.y - From.y) / tc + pPattern->Alpha * tc;
            
            pPos->x = (1.0f - t) * From.x + t * To.x;
            pPos->z = (1.0f - t) * From.z + t * To.z;
            pPos->y = -pPattern->Alpha * Time * Time + b * Time + From.y;
            
            if (pObj->ObjectType == OBJECT_BALL)
                pPos->Rot = pObj->TotalTwist + t * BallTwist;
            else
            {
                /* We describe the rotation of a club (or ring) with an
                 * elevation and rotation but don't include a twist.
                 * If we ignore twist for the moment, the orientation at a
                 * rotation of r and an elevation of e can be also be expressed
                 * by rotating the object a further 180 degrees and sort of
                 * mirroring the rotation, e.g.:
                 *    rot = r + 180 and elev = 180 - e
                 * We can easily show that the maths holds, consider the
                 * x, y ,z position of the end of a unit length club.
                 *    y = sin(180 - e) = sin(e)
                 *    x = cos(180 - e) * sin(r + 180) = -cos(e) * - sin(r)
                 *    z = cos(180 - e) * cos(r + 180) = -cos(e) * - cos(r)
                 * When a club is thrown these two potential interpretations
                 * can produce unexpected results.
                 * The approach we adopt is that we try and minimise the amount
                 * of rotation we give a club -- normally this is what happens
                 * when juggling since it's much easier to spin the club.
                 *
                 * When we come to drawing the object the two interpretations
                 * aren't identical, one causes the object to twist a further
                 * 180 about its axis.  We avoid the issue by ensuring our
                 * objects have rotational symmetry of order 2 (e.g. we make
                 * sure clubs have an even number of stripes) this makes the two
                 * interpretations appear identical. */

                float RotAmt = NormaliseAngle(To.Rot - From.Rot);

                if (RotAmt < -90.0f)
                {
                    To.Elev += 180  - 2 * NormaliseAngle(To.Elev);
                    RotAmt += 180.0f;
                }
                else if (RotAmt > 90.0f)
                {
                    To.Elev += 180 - 2 * NormaliseAngle(To.Elev);
                    RotAmt -= 180.0f;
                }

                pPos->Rot = From.Rot + t * RotAmt;
            }

            pPos->Elev = (1.0f - t) * From.Elev + t * To.Elev;

        }
        else
        {
            THROW_INFO* pNextThrow = &pPattern->pThrowInfo[
                   (pObj->ThrowIndex + pThrow->TotalTime) % pPattern->ThrowLen];

            *pPos = InterpolateCarry(pThrow, pNextThrow, Time - tc);

            if (pObj->ObjectType == OBJECT_BALL)
                pPos->Rot = pObj->TotalTwist + BallTwist;

            OffsetHandlePosition(pPos, HandleLen, pPos);
        }
    }
}


/* Alpha is used to represent the acceleration due to gravity (in fact
 * 2 * Alpha is the acceleration).  Alpha is adjusted according to the pattern
 * being juggled.  My preference is to slow down patterns with lots of objects
 * -- they move too fast in realtime.  Also I prefer to see a balance between
 * the size of the figure and the height of objects thrown -- juggling patterns
 * with large numbers of objects under real gravity can mean balls are lobbed
 * severe heights.  Adjusting Alpha achieves both these goals.
 *
 * Basically we pick a height we'd like to see the biggest throw reach and then
 * adjust Alpha to meet this. */

static void SetHeightAndAlpha(PATTERN_INFO* pPattern, 
    const int* Site, const EXT_SITE_INFO* pExtInfo, int Len)
{
    float H;
    int MaxW = 5;
    int i;
    
    if (Site != NULL)
    {
        for (i = 0; i < Len; i++)
            MaxW = max(MaxW, Site[i]);
    }
    else
    {
        for (i = 0; i < Len; i++)
            MaxW = max(MaxW, pExtInfo[i].Weight);
    }
    
    /* H is the ideal max height we'd like our objects to reach.  The formula
     * was developed by trial and error and was simply stolen from Juggle Saver.
     * Alpha is then calculated from the classic displacement formula:
     *   s = 0.5at^2 + ut  (where a = 2 * Alpha)
     * We know u (the velocity) is zero at the peak, and the object should fall
     * H units in half the time of biggest throw weight.
     * Finally we determine the proper height the max throw reaches since this
     * may not be H because capping may be applied (e.g. for max weights less
     * than 5). */
    
    H = 8.0f * powf(MaxW / 2.0f, 0.8f) + 5.0f;
    pPattern->Alpha = (2.0f * H) / powf(max(5, MaxW) - CARRY_TIME, 2.0f);
    pPattern->Height = pPattern->Alpha * powf((MaxW - CARRY_TIME) * 0.5f, 2);
}


/* Where positions and spin info is not specified, generate suitable default
 * values. */

static int GetDefaultSpins(int Weight)
{
    if (Weight < 3)
        return 0;
    else if (Weight < 4)
        return 1;
    else if (Weight < 7)
        return 2;
    else
        return 3;
}


static void GetDefaultFromPosition(unsigned char Side, int Weight, POS* pPos)
{
    if (Weight > 4 && Weight % 2 != 0)
        pPos->x = Side ?  -0.06f : 0.06f;
    else if (Weight == 0 || Weight == 2)
        pPos->x = Side ? 1.6f :  -1.6f;
    else
        pPos->x = Side? 0.24f :  -0.24f;

    pPos->y = (Weight == 2 || Weight == 0) ? -0.25f : 0.0f;

    pPos->Rot = (Weight % 2 == 0 ? -23.5f : 27.0f) * (Side ? -1.0f : 1.0f);

    pPos->Elev = Weight == 1 ? -30.0f : 0.0f;
    pPos->z = 0.0f;
}


static void GetDefaultToPosition(unsigned char Side, int Weight, POS* pPos)
{
    if (Weight == 1)
        pPos->x = Side ?  -1.0f : 1.0f;
    else if (Weight % 2 == 0)
        pPos->x = Side ? 2.8f :  -2.8f;
    else
        pPos->x = Side?  -3.1f : 3.1f;

    pPos->y = -0.5f;

    pPos->Rot = (Side ? -35.0f : 35.0f) * (Weight % 2 == 0 ? -1.0f : 1.0f);
    
    if (Weight < 2)
        pPos->Elev = -30.0f;

    else if (Weight < 4)
        pPos->Elev = 360.0f - 50.0f;
    else if (Weight < 7)
        pPos->Elev = 720.0f - 50.0f;
    else
        pPos->Elev = 360.0f * GetDefaultSpins(Weight) - 50.0f;
    pPos->z = 0.0f;
}


/* Update the members of PATTERN_INFO for a given juggling pattern.  The pattern
 * can come from an ordinary siteswap (Site != NULL) or from a Juggle Saver
 * compatible pattern that contains, position and object info etc. 
 * We assume that patterns are valid and have at least one object (a site of
 * zeros is invalid).  The ones we generate randomly are safe. */

static void InitPatternInfo(PATTERN_INFO* pPattern,
    const int* Site, const EXT_SITE_INFO* pExtInfo, int Len)
{
    /* Double up on the length of the site if it's of an odd length. 
     * This way we can store position information: even indices are on one
     * side and odds are on the other. */
    int InfoLen = Len % 2 == 1 ? Len * 2 : Len;
    int i;
    THROW_INFO* pInfo = (THROW_INFO*) calloc(InfoLen, sizeof(THROW_INFO));
    int Objects = 0;
    unsigned char* pUsed;
    
    pPattern->MaxWeight = 0;
    pPattern->ThrowLen = InfoLen;
    pPattern->pThrowInfo = pInfo;
    
    SetHeightAndAlpha(pPattern, Site, pExtInfo, Len);

    /* First pass through we assign the things we know about for sure just by
     * looking at the throw weight at this position.  This includes TimeInAir;
     * the throw and catch positions; and throw and catch velocities.
     * Other information, like the total time for the throw (i.e. when the
     * object is thrown again) relies on how the rest of the pattern is 
     * structured and we defer this task for successive passes and just make
     * guesses at this stage. */
    
    for (i = 0; i < InfoLen; i++)
    {
        float t1;
        int w = pExtInfo != NULL ? pExtInfo[i % Len].Weight : Site[i % Len];

        pInfo[i].TotalTime = pInfo[i].TimeInAir = w;
        pInfo[(w + i) % Len].PrevThrow = i;

        /* work out where we are throwing this object from and where it's going
         * to land. */

        if (pExtInfo == NULL || (pExtInfo[i % Len].Flags & HAS_FROM_POS) == 0)
            GetDefaultFromPosition(i % 2, w, &pInfo[i].FromPos);
        else
            pInfo[i].FromPos = pExtInfo[i % Len].FromPos;

        if (pExtInfo == NULL || (pExtInfo[i % Len].Flags & HAS_TO_POS) == 0)
            GetDefaultToPosition(i % 2, w, &pInfo[i].ToPos);
        else
            pInfo[i].ToPos = pExtInfo[i % Len].ToPos;

        /* calculate the velocity the object is moving at the start and end
         * points -- this information is used to interpolate the hand position
         * and to determine how the object is moved while it's carried to the 
         * next throw position.
         *
         * The throw motion is governed by a parabola of the form:
         *   y(t) = a * t ^ 2 + b * t + c
         * Assuming at the start of the throw y(0) = y0; when it's caught
         * y(t1) = y1; and the accelation is -2.0 * alpha the equation can be
         * rewritten as:
         *   y(t) = -alpha * t ^ 2 + (alpha * t1 + (y1 - y0) / t1) * t + y0
         * making the velocity:
         *   y'(t) = -2.0 * alpha * t + (alpha * t1 + (y1 - y0) / t1)
         * To get the y component of velocity first we determine t1, which is
         * the throw weight minus the time spent carrying the object.  Then
         * perform the relevant substitutions into the above.
         * (note: y'(t) = y'(0) - 2.0 * alpha * t)
         * 
         * The velocity in the x direction is constant and can be simply
         * obtained from:
         *   x' = (x1 - x0) / t1
         * where x0 and x1 are the start and end x-positions respectively.
         */

        t1 = w - CARRY_TIME;

        pInfo[i].FromVelocity.y = pPattern->Alpha * t1 + 
            (pInfo[i].ToPos.y - pInfo[i].FromPos.y) / t1;
        pInfo[i].ToVelocity.y = 
            pInfo[i].FromVelocity.y - 2.0f * pPattern->Alpha * t1;
        pInfo[i].FromVelocity.x = pInfo[i].ToVelocity.x = 
            (pInfo[i].ToPos.x - pInfo[i].FromPos.x) / t1;
        pInfo[i].FromVelocity.z = pInfo[i].ToVelocity.z = 
            (pInfo[i].ToPos.z - pInfo[i].FromPos.z) / t1;
        pInfo[i].FromVelocity.Rot = pInfo[i].ToVelocity.Rot =
            (pInfo[i].ToPos.Rot - pInfo[i].FromPos.Rot) / t1;
        pInfo[i].FromVelocity.Elev = pInfo[i].ToVelocity.Elev =
            (pInfo[i].ToPos.Elev - pInfo[i].FromPos.Elev) / t1;


        if (pExtInfo != NULL && (pExtInfo[i % Len].Flags & HAS_SNATCH) != 0)
        {
            pInfo[i].ToVelocity.x = pExtInfo[i % Len].SnatchX;
            pInfo[i].ToVelocity.y = pExtInfo[i % Len].SnatchY;
        }

        if (pExtInfo != NULL && (pExtInfo[i % Len].Flags & HAS_SPINS) != 0)
        {
            pInfo[i].ToPos.Elev = 360.0f * pExtInfo[i % Len].Spins +
                NormaliseAngle(pInfo[i].ToPos.Elev);
        }

        Objects += w;
        if (w > pPattern->MaxWeight)
            pPattern->MaxWeight = w;
    }

    Objects /= InfoLen;

    /* Now we go through again and work out exactly how long it is before the
     * object is thrown again (ie. the TotalTime) typically this is the same
     * as the time in air, however when we have a throw weight of '2' it's
     * treated as a hold and we increase the total time accordingly. */

    for (i = 0; i < InfoLen; i++)
    {
        if (pInfo[i].TimeInAir != 2)
        {
            int Next = pInfo[i].TimeInAir + i;
            while (pInfo[Next % InfoLen].TimeInAir == 2)
            {
                Next += 2;
                pInfo[i].TotalTime += 2;
            }

            /* patch up the Prev index.  We don't bother to see if this
             * is different from before since it's always safe to reassign it */
            pInfo[Next % InfoLen].PrevThrow = i;
        }
    }

    /* then we work our way through again figuring out where the hand goes to
     * catch something as soon as it has thrown the current object. */

    for (i = 0; i < InfoLen; i++)
    {
        if (pInfo[i].TimeInAir != 0 && pInfo[i].TimeInAir != 2)
        {
            /* what we're trying to calculate is how long the hand that threw
             * the current object has to wait before it throws another.
             * Typically this is two beats later.  However '0' in the site swap
             * represents a gap in a catch, and '2' represents a hold.  We skip
             * over these until we reach the point where a ball is actually
             * thrown. */
            int Wait = 2;
            while (pInfo[(i + Wait) % InfoLen].TimeInAir == 2 || 
                pInfo[(i + Wait) % InfoLen].TimeInAir == 0)
            {
                Wait += 2;
            }
            pInfo[i].NextForHand = Wait;
        }
        else
        {
            /* Be careful to ensure the current weight isn't one we're trying
             * to step over; otherwise we could potentially end up in an 
             * infinite loop.  The value we assign may end up being used
             * in patterns with infinite gaps (e.g. 60) or infinite holds
             * (e.g. 62) in both cases, setting a wait of 2 ensures things
             * are well behaved. */
            pInfo[i].NextForHand = 2;
        }
    }

    /* Now work out the starting positions for the objects.  To do this we
     * unweave the initial throws so we can pick out the individual threads. */

    pUsed = (unsigned char*) 
        malloc(sizeof(unsigned char) * pPattern->MaxWeight);
    pPattern->Objects = Objects;
    pPattern->pObjectInfo = (OBJECT_POSITION*) calloc(
        Objects, sizeof(OBJECT_POSITION));

    for (i = 0; i < pPattern->MaxWeight; i++)
        pUsed[i] = 0;

    for (i = 0; i < pPattern->MaxWeight; i++)
    {
        int w = pInfo[i % InfoLen].TimeInAir;
        if (pUsed[i] == 0 &&  w != 0)
        {
            Objects--;
            pPattern->pObjectInfo[Objects].TimeOffset = i;
            pPattern->pObjectInfo[Objects].ThrowIndex = i % InfoLen;
            pPattern->pObjectInfo[Objects].TotalTwist = 0.0f;

            if (pExtInfo != NULL && 
                pExtInfo[i % Len].ObjectType != OBJECT_DEFAULT)
            {
                pPattern->pObjectInfo[Objects].ObjectType =
                    pExtInfo[i % Len].ObjectType;
            }
            else
            {
                pPattern->pObjectInfo[Objects].ObjectType = (1 + random() % 3);
            }
        }

        if (w + i < pPattern->MaxWeight)
            pUsed[w + i] = 1;
        
    }

    pPattern->LeftHand.TimeOffset = pPattern->LeftHand.ThrowIndex = 0;
    pPattern->RightHand.TimeOffset = pPattern->RightHand.ThrowIndex = 1;
    
    free(pUsed);
}


static void ReleasePatternInfo(PATTERN_INFO* pPattern)
{
    free(pPattern->pObjectInfo);
    free(pPattern->pThrowInfo);
}


/*****************************************************************************
 *
 * Sites
 *
 ****************************************************************************/

/* Generate a random site swap.  We assume that MaxWeight >= ObjCount and
 * Len >= MaxWeight. */
 
static int* Generate(int Len, int MaxWeight, int ObjCount)
{
    int* Weight = (int*) calloc(Len, sizeof(int));
    int* Used = (int*) calloc(Len, sizeof(int));
    int* Options = (int*) calloc(MaxWeight + 1, sizeof(int));
    int nOpts;
    int i, j;

    for (i = 0; i < Len; i++)
        Weight[i] = Used[i] = -1;
    
    /* Pick out a unique the starting position for each object.  -2 is put in
     * the Used array to signify this is a starting position. */

    while (ObjCount > 0)
    {
        nOpts = 0;
        for (j = 0; j < MaxWeight; j++)
        {
            if (Used[j] == -1)
                Options[nOpts++] = j;
        }

        Used[Options[random() % nOpts]] = -2;
        ObjCount--;
    }
    
    /* Now work our way through the pattern moving throws into an available
     * landing positions. */
    for (i = 0; i < Len; i++)
    {
        if (Used[i] == -1)
        {
            /* patch up holes in the pattern to zeros */
            Used[i] = 1;
            Weight[i] = 0;
        }
        else
        {
            /* Work out the possible places where a throw can land and pick a 
             * weight at random. */
            int w;
            nOpts = 0;

            for (j = 0 ; j <= MaxWeight; j++)
            {
                if (Used[(i + j) % Len] == -1)
                    Options[nOpts++] = j;
            }
            
            w = Options[random() % nOpts];
            Weight[i] = w;
            
            /* For starting throws make position available for a throw to land.
             * Because Len >= MaxWeight these positions will only be filled when
             * a throw wraps around the end of the site swap and therefore we
             * can guarantee the all the object threads will be tied up. */
            if (Used[i] == -2)
                Used[i] = -1;
            
            Used[(i + w) % Len] = 1;
        }
    }

    free(Options);
    free(Used);
    return Weight;
}


/* Routines to parse the Juggle Saver patterns.  These routines are a bit yucky
 * and make the big assumption that the patterns are well formed.  This is fine
 * as it stands because only 'good' ones are used but if the code is ever
 * extended to read arbitrary patterns (say from a file) then these routines
 * need to be beefed up. */

/* The position text looks something like (x,y,z[,rot[,elev]])
 * where the stuff in square brackets is optional */

static unsigned char ParsePositionText(const char** ppch, POS* pPos)
{
    const char* pch = *ppch;
    unsigned char OK;
    char szTemp[32];
    char* pOut;
    float* Nums[4];
    int i;
    
    Nums[0] = &pPos->x;
    Nums[1] = &pPos->y;
    Nums[2] = &pPos->Rot;
    Nums[3] = &pPos->Elev;


    while (*pch == ' ')
        pch++;
    
    OK = *pch == '(';
    
    if (OK)
        pch++;

    for (i = 0; OK && i < 4; i++)
    {
        pOut = szTemp;
        while (*pch == ' ')
            pch++;
        while (*pch != ',' && *pch != '\0' && *pch != ')' && *pch != ' ')
            *pOut++ = *pch++;
        *pOut = '\0';

        if (szTemp[0] != '\0')
            *Nums[i] = (float) atof(szTemp);

        while (*pch == ' ')
            pch++;

        if (i < 3)
        {
            if (*pch == ',')
                pch++;
            else if (*pch == ')')
                break;
            else
                OK = 0;
        }
    }

    if (OK)
    {
        while (*pch == ' ')
            pch++;        
        if (*pch == ')')
            pch++;
        else
            OK = 0;
    }

    *ppch = pch;

    return OK;
}


static EXT_SITE_INFO* ParsePattern(const char* Site, int* pLen)
{
    const char* pch = Site;
    int Len = 0;
    EXT_SITE_INFO* pInfo = NULL;
    unsigned char OK = 1;

    while (OK && *pch != 0)
    {
        EXT_SITE_INFO Info;
        Info.Flags = 0;

        while (*pch == ' ') pch++;

        OK = *pch != '\0';

        if (OK)
            Info.Weight = *pch >= 'A' ? *pch + 10 - 'A' : *pch - '0';

        /* parse object type */
        if (OK)
        {
            pch++;
            while (*pch == ' ') pch++;

            if (*pch == 'b' || *pch == 'B')
            {
                Info.ObjectType = OBJECT_BALL;
                pch++;
            }
            else if (*pch == 'c' || *pch == 'C')
            {
                Info.ObjectType = OBJECT_CLUB;
                pch++;
            }
            else if (*pch == 'r' || *pch == 'R')
            {
                Info.ObjectType = OBJECT_RING;
                pch++;
            }
            else if (*pch == 'd' || *pch == 'D')
            {
                Info.ObjectType = OBJECT_DEFAULT;
                pch++;
            }
            else
            {
                Info.ObjectType = OBJECT_DEFAULT;
            }
        }

        /* Parse from position */
        if (OK)
        {
            while (*pch == ' ') pch++;
            if (*pch == '@')
            {
                pch++;
                GetDefaultFromPosition(Len % 2, Info.Weight, &Info.FromPos);
                Info.Flags |= HAS_FROM_POS;
                OK = ParsePositionText(&pch, &Info.FromPos);
            }
        }

        /* Parse to position */
        if (OK)
        {
            while (*pch == ' ') pch++;
            if (*pch == '>')
            {
                pch++;
                GetDefaultToPosition(Len % 2, Info.Weight, &Info.ToPos);
                Info.Flags |= HAS_TO_POS;
                OK = ParsePositionText(&pch, &Info.ToPos);
            }
        }

        /* Parse snatch */
        if (OK)
        {
            while (*pch == ' ') pch++;
            if (*pch == '/')
            {
                POS Snatch;
                pch++;
                Info.Flags |= HAS_SNATCH;
                OK = ParsePositionText(&pch, &Snatch);
                Info.SnatchX = Snatch.x;
                Info.SnatchY = Snatch.y;
            }
        }

        /* Parse Spins */
        if (OK)
        {
            while (*pch == ' ') pch++;
            if (*pch == '*')
            {
                pch++;
                OK = 0;
                Info.Spins = 0;
                while (*pch >= '0' && *pch <= '9')
                {
                    OK = 1;
                    Info.Spins = Info.Spins * 10 + *pch - '0';
                    pch++;
                }
            }
            else
                Info.Spins = GetDefaultSpins(Info.Weight);

            Info.Flags |= HAS_SPINS;
        }

        if (OK)
        {
            if (pInfo == NULL)
                pInfo = (EXT_SITE_INFO*) malloc(sizeof(EXT_SITE_INFO));
            else
                pInfo = (EXT_SITE_INFO*) realloc(pInfo, (Len + 1) * sizeof(EXT_SITE_INFO));

            pInfo[Len] = Info;
            Len++;
        }
    }

    if (!OK && pInfo != NULL)
    {
        free(pInfo);
        pInfo = NULL;
    }

    *pLen = Len;

    return pInfo;
}


/*****************************************************************************
 *
 *  Juggle Saver Patterns
 *
 *****************************************************************************
 *
 * This is a selection of some of the more interesting patterns from taken
 * from the Juggle Saver sites.txt file.  I've only used patterns that I
 * originally created.
 */

static const char* PatternText[] =
{
    "9b@(-2.5,0,-70,40)>(2.5,0,70)*2 1b@(1,0,10)>(-1,0,-10)",
    
    "3B@(1,-0.4)>(2,4.2)/(-2,1)3B@(-1.8,4.4)>(-2.1,0)",
    
    "7c@(-2,0,-20)>(1.2,0,-5)7c@(2,0,20)>(-1.2,0,5)",
    
    "3b@(-0.5,0)>(1.5,0) 3b@(0.5,0)>(-1.5,0) 3r@(-2.5,3,-90,80)>(2,1,90,30)"
    "3b@(0.5,0)>(-1.5,0) 3b@(-0.5,0)>(1.5,0) 3r@(2.5,3,90,80)>(-2,1,-90,30)",
    
    "5c@(2,1.9,10)>(-1,1,10)5c@(2,1.8,10)>(-0.5,1.6,10)/(5,-1)"
    "5c@(1.6,0.2,10)>(0,-1,10)/(9,-2)5c@(-2,1.9,-10)>(1,1,-10)"
    "5c@(-2,1.8,-10)>(0.5,1.6,-10)/(-5,-1)5@(-1.6,0.2,-10)>(0,-1,-10)/(-9,-2)",
    
    "3c@(-1.5,0,0)>(-1.5,1,0)3c@(1.5,-0.2,0)>(1.5,-0.1,0)3c@(0,-0.5,0)>(0,1,0)"
    "3@(-1.5,2,0)>(-1.5,-1,0)3@(1.5,0,0)>(1.5,1,0)3@(0,0,0)>(0,-0.5,0)",
    
    "9c@(-2.5,0,-70,40)>(2.5,0,70)*2 1c@(1,0,10)>(-1,0,-10)*0",
    
    "3c@(2,0.5,60,0)>(1.5,4,60,80)/(-6,-12)"
    "3c@(-2,0.5,-60,0)>(-1.5,4,-60,80)/(6,-12)",
    
    "3c@(-0.2,0)>(1,0)3c@(0.2,0)>(-1,0)3c@(-2.5,2,-85,30)>(2.5,2,85,40)*2 "
    "3@(0.2,0)>(-1,0) 3@(-0.2,0)>(1,0) 3@(2.5,2,85,30)>(-2.5,2,-85,40)*2",
    
    "3c@(-0.5,-0.5,20,-30)>(2.6,4.3,60,60)/(0,1)*1 "
    "3c@(1.6,5.6,60,80)>(-2.6,0,-80)*0",
    
    "5c@(-0.3,0,10)>(1.2,0,10) 5c@(0.3,0,-10)>(-1.2,0,-10)"
    "5c@(-0.3,0,10)>(1.2,0,10) 5c@(0.3,0,-10)>(-1.2,0,-10)"
    "5c@(-3,3.5,-65,80)>(3,2.5,65) 5c@(0.3,0,-10)>(-1.2,0,-10)"
    "5@(-0.3,0,10)>(1.2,0,10) 5@(0.3,0,-10)>(-1.2,0,-10)"
    "5@(-0.3,0,10)>(1.2,0,10)5@(3,3.5,65,80)>(-3,2.5,-65)"
};


/*****************************************************************************
 *
 * Rendering
 *
 *****************************************************************************/

static const float FOV = 70.0f;
static const float BodyCol[] = {0.6f, 0.6f, 0.45f, 1.0f};
static const float HandleCol[] = {0.45f, 0.45f, 0.45f, 1.0f};
static const float LightPos[] = {0.0f, 200.0f, 400.0f, 1.0f};
static const float LightDiff[] = {1.0f, 1.0f, 1.0f, 0.0f};
static const float LightAmb[] = {0.02f, 0.02f, 0.02f, 0.0f};
static const float ShoulderPos[3] = {0.95f, 2.1f, 1.7f};
static const float DiffCol[] = {1.0f, 0.0f, 0.0f, 1.0f};
static const float SpecCol[] = {1.0f, 1.0f, 1.0f, 1.0f};

static const float BallRad = 0.34f;
static const float UArmLen = 1.9f;
static const float LArmLen = 2.3f;

#define DL_BALL 0
#define DL_CLUB 1
#define DL_RING 2
#define DL_TORSO 3
#define DL_FOREARM 4
#define DL_UPPERARM 5

static const float AltCols[][4] =
{
    {0.0f, 0.7f, 0.0f, 1.0f},
    {0.0f, 0.0f, 0.9f, 1.0f},
    {0.0f, 0.9f, 0.9f, 1.0f},
    {0.45f, 0.0f, 0.9f, 1.0f},
    {0.9f, 0.45f, 0.0f, 1.0f},
    {0.0f, 0.45f, 0.9f, 1.0f},
    {0.9f, 0.0f, 0.9f, 1.0f},
    {0.9f, 0.9f, 0.0f, 1.0f},
    {0.9f, 0.0f, 0.45f, 1.0f},
    {0.45f, 0.15f, 0.6f, 1.0f}, 
    {0.9f, 0.0f, 0.0f, 1.0f},
    {0.0f, 0.9f, 0.45f, 1.0f},
};

static const float Cols[][4] =
{
    {0.9f, 0.0f, 0.0f, 1.0f},  /*  0 */
    {0.0f, 0.7f, 0.0f, 1.0f},  /*  1 */
    {0.0f, 0.0f, 0.9f, 1.0f},  /*  2 */
    {0.0f, 0.9f, 0.9f, 1.0f},  /*  3 */
    {0.9f, 0.0f, 0.9f, 1.0f},  /*  4 */
    {0.9f, 0.9f, 0.0f, 1.0f},  /*  5 */
    {0.9f, 0.45f, 0.0f, 1.0f}, /*  6 */
    {0.9f, 0.0f, 0.45f, 1.0f}, /*  7 */
    {0.45f, 0.9f, 0.0f, 1.0f}, /*  8 */
    {0.0f, 0.9f, 0.45f, 1.0f}, /*  9 */
    {0.45f, 0.0f, 0.9f, 1.0f}, /* 10 */
    {0.0f, 0.45f, 0.9f, 1.0f}, /* 11 */
};

static int InitGLDisplayLists(void);


static void InitGLSettings(RENDER_STATE* pState, int WireFrame)
{
    memset(pState, 0, sizeof(RENDER_STATE));
    
    pState->trackball = gltrackball_init ();

    if (WireFrame)
        glPolygonMode(GL_FRONT, GL_LINE);
    
    glClearColor(0.0f, 0.0f, 0.0f, 0.0f);

    glLightfv(GL_LIGHT0, GL_POSITION, LightPos);
    glLightfv(GL_LIGHT0, GL_DIFFUSE, LightDiff);
    glLightfv(GL_LIGHT0, GL_AMBIENT, LightAmb);
    
    glEnable(GL_SMOOTH);
    glEnable(GL_LIGHTING);
    glEnable(GL_LIGHT0);

    glDepthFunc(GL_LESS);
    glEnable(GL_DEPTH_TEST);

    glCullFace(GL_BACK);
    glEnable(GL_CULL_FACE);
    
    pState->DLStart = InitGLDisplayLists();
}


static void SetCamera(RENDER_STATE* pState)
{
    /* Try to work out a sensible place to put the camera so that more or less
     * the whole juggling pattern fits into the screen. We assume that the
	 * pattern is height limited (i.e. if we get the height right then the width
	 * will be OK).  This is a pretty good assumption given that the screen
	 * tends to wider than high, and that a juggling pattern is normally much
	 * higher than wide.
     *
     * If I could draw a diagram here then it would be much easier to
     * understand but my ASCII-art skills just aren't up to it.  
     *
     * Basically we estimate a bounding volume for the juggler and objects 
     * throughout the pattern.  We don't fully account for the fact that the
     * juggler moves across the stage in an epicyclic-like motion and instead
     * use the near and far planes in x-y (with z = +/- w).  We also
     * assume that the scene is centred at x=0, this reduces our task to finding
     * a bounding rectangle.  Finally we need to make an estimate of the
     * height - for this we work out the max height of a standard throw or max
     * weight from the pattern; we then do a bit of adjustment to account for
     * a throw occurring at non-zero y values.
     *
     * Next we work out the best way to fit this rectangle into the perspective
     * transform.  Based on the angle of elevation (+ve angle looks down) and
     * the FOV we can work out whether it's the near or far corners that are
     * the extreme points.  And then trace back from them to find the eye
     * point.
     *
     */
     
    float ElevRad = pState->CameraElev * PI / 180.0f;
    float w = 3.0f;
    float cy, cz;
    float ey, ez;
    float d;
    float H = 0.0f;
    int i;
    float a;
    
    float tz, ty, ta;
    float bz, by, ba;
    const PATTERN_INFO* pPattern = pState->pPattern;

    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
        
    for (i = 0; i < pPattern->ThrowLen; i++)
        H = max(H, pPattern->pThrowInfo[i].FromPos.y);
        
    H += pPattern->Height;
    
    ElevRad = pState->CameraElev * PI / 180.0f;
    
    /* ta is the angle from a point on the top of the bounding area to the eye
     * similarly ba is the angle from a point on the bottom. */
    ta = (pState->CameraElev  - (FOV - 10.0f) / 2.0f) * PI / 180.0f;
    ba = (pState->CameraElev  + (FOV - 10.0f) / 2.0f) * PI / 180.0f;

    /* tz and bz hold the z location of the top and bottom extreme points.
     * For the top, if the angle to the eye location is positive then the
     * extreme point is with far z corner (the camera looks in -ve z).
     * The logic is reserved for the bottom. */
    tz = ta >= 0.0f ? -w : w;
    bz = ba >= 0.0f ? w : -w;
    
    ty = H;
    by = -1.0f;
    
    /* Solve of the eye location by using a bit of geometry.
     * We know the eye lies on intersection of two lines.  One comes from the
     * top and other from the bottom. Giving two equations:
     *   ez = tz + a * cos(ta) = bz + b * cos(ba)
     *   ey = ty + a * sin(ta) = by + b * sin(ba)
     * We don't bother to solve for b and use Crammer's rule to get
     *         | bz-tz  -cos(ba) |
     *         | by-ty  -sin(ba) |     
     *   a =  ----------------------
     *        | cos(ta)   -cos(ba) |
     *        | sin(ta)   -sin(ba) |
     */
    d = cosf(ba) * sinf(ta) - cosf(ta) * sinf(ba);
    a = (cosf(ba) * (by - ty) - sinf(ba) * (bz - tz)) / d;
    
    ey = ty + a * sinf(ta);
    ez = tz + a * cosf(ta);
    
    /* now work back from the eye point to get the lookat location */
    cz = 0.0;
    cy = ey - ez * tanf(ElevRad);
    
    /* use the distance from the eye to the scene centre to get a measure
     * of what the far clipping should be.  We then add on a bit more to be 
     * comfortable */
    d = sqrtf(ez * ez + (cy - ey) * (cy - ey));
    
    gluPerspective(FOV, pState->AspectRatio, 0.1f, d + 20.0f);
    gluLookAt(0.0, ey, ez, 0.0, cy, cz, 0.0, 1.0, 0.0);

    glMatrixMode(GL_MODELVIEW);
}


static void ResizeGL(RENDER_STATE* pState, int w, int h)
{
    glViewport(0, 0, w, h);
    pState->AspectRatio = (float) w / h;
    SetCamera(pState);
}


/* Determine the angle at the vertex of a triangle given the length of the
 * three sides. */

static double CosineRule(double a, double b, double c)
{
    double cosang = (a * a + b * b - c * c) / (2 * a * b);
    /* If lengths don't form a proper triangle return something sensible.
     * This typically happens with patterns where the juggler reaches too 
     * far to get hold of an object. */
    if (cosang < -1.0 || cosang > 1.0)
        return 0;
    else
        return 180.0 * acos(cosang) / PI;
}


/* Spheres for the balls are generated by subdividing each triangle face into
 * four smaller triangles.  We start with an octahedron (8 sides) and repeat the
 * process a number of times.  The result is a mesh that can be split into four
 * panels (like beanbags) and is smoother than the normal stacks and slices
 * approach. */

static void InterpolateVertex(
    const float* v1, const float* v2, float t, float* result)
{
    result[0] = v1[0] * (1.0f - t) + v2[0] * t;
    result[1] = v1[1] * (1.0f - t) + v2[1] * t;
    result[2] = v1[2] * (1.0f - t) + v2[2] * t;
}


static void SetGLVertex(const float* v, float rad)
{
    float Len = sqrtf(v[0] * v[0] + v[1] * v[1] + v[2] * v[2]);

    if (Len >= 1.0e-10f)
    {
        glNormal3f(v[0] / Len, v[1] / Len, v[2] / Len);
        glVertex3f(rad * v[0] / Len, rad * v[1] / Len, rad * v[2] / Len);
    }
    else
        glVertex3fv(v);
}


static void SphereSegment(
    const float* v1, const float* v2, const float* v3, float r, int Levels)
{
    int i, j;

    for (i = 0; i < Levels; i++)
    {
        float A[3], B[3], C[3], D[3];
        
        InterpolateVertex(v3, v1, (float) i / Levels, D);
        InterpolateVertex(v3, v1, (float)(i + 1) / Levels, A);
        InterpolateVertex(v3, v2, (float)(i + 1) / Levels, B);
        InterpolateVertex(v3, v2, (float) i / Levels, C);

        glBegin(GL_TRIANGLE_STRIP);

        SetGLVertex(B, r);
        SetGLVertex(C, r);
        
        for (j = 1; j <= i; j++)
        {
            float v[3];

            InterpolateVertex(B, A, (float) j / (i + 1), v);
            SetGLVertex(v, r);

            InterpolateVertex(C, D, (float) j / i, v);
            SetGLVertex(v, r);
        }

        SetGLVertex(A, r);
        
        glEnd();
    }
}


/* OK, this function is a bit of misnomer, it only draws half a sphere.  Indeed
 * it draws two panels and allows us to colour this one way,  then draw the
 * same shape again rotated 90 degrees in a different colour.  Resulting in what
 * looks like a four-panel beanbag in two complementary colours. */
 
static void DrawSphere(float rad)
{
    int Levels = 4;
    float v1[3], v2[3], v3[3];
    
    v1[0] = 1.0f, v1[1] = 0.0f; v1[2] = 0.0f;
    v2[0] = 0.0f, v2[1] = 1.0f; v2[2] = 0.0f;
    v3[0] = 0.0f, v3[1] = 0.0f; v3[2] = 1.0f;
    SphereSegment(v1, v2, v3, rad, Levels);
    
    v2[1] = -1.0f;
    SphereSegment(v2, v1, v3, rad, Levels);
    
    v1[0] = v3[2] = -1.0f;
    SphereSegment(v2, v1, v3, rad, Levels);

    v2[1] = 1.0f;
    SphereSegment(v1, v2, v3, rad, Levels);
}


static void DrawRing(void)
{
    const int Facets = 22;
    const float w = 0.1f;
    GLUquadric* pQuad = gluNewQuadric();
    glRotatef(90.0f, 0.0f, 1.0f, 0.0f);
    glTranslatef(0.0f, 0.0f, -w / 2.0f);

    gluCylinder(pQuad, 1.0f, 1.0f, w, Facets, 1);
    gluQuadricOrientation(pQuad, GLU_INSIDE);

    gluCylinder(pQuad, 0.7f, 0.7f, w, Facets, 1);
    gluQuadricOrientation(pQuad, GLU_OUTSIDE);

    glTranslatef(0.0f, 0.0f, w);
    gluDisk(pQuad, 0.7, 1.0f, Facets, 1);

    glRotatef(180.0f, 0.0f, 1.0f, 0.0f);
    glTranslatef(0.0f, 0.0f, w);
    gluDisk(pQuad, 0.7, 1.0f, Facets, 1);

    gluDeleteQuadric(pQuad);
}


/* The club follows a 'circus club' design i.e. it has stripes running down the
 * body.  The club is draw such that the one stripe uses the current material
 * and the second stripe the standard silver colour. */

static void DrawClub(void)
{
    const float r[4] = {0.06f, 0.1f, 0.34f, 0.34f / 2.0f};
    const float z[4] = {-0.4f, 0.6f, 1.35f, 2.1f};
    float na[4];
    const int n = 18;
    int i, j;
    GLUquadric* pQuad;

    na[0] = (float) atan((r[1] - r[0]) / (z[1] - z[0]));
    na[1] = (float) atan((r[2] - r[1]) / (z[2] - z[1]));
    na[2] = (float) atan((r[3] - r[1]) / (z[3] - z[1]));
    na[3] = (float) atan((r[3] - r[2]) / (z[3] - z[2]));

    for (i = 0; i < n; i += 2)
    {
        float a1 = i * PI * 2.0f / n;
        float a2 = (i + 1) * PI * 2.0f / n;

        glBegin(GL_TRIANGLE_STRIP);
            for (j = 1; j < 4; j++)
            {
                glNormal3f(cosf(na[j]) * cosf(a1),
                    cosf(na[j]) * sinf(a1), sinf(na[j]));

                glVertex3f(r[j] * cosf(a1), r[j] * sinf(a1), z[j]);

                glNormal3f(cosf(na[j]) * cosf(a2),
                    cosf(na[j]) * sinf(a2),    sinf(na[j]));

                glVertex3f(r[j] * cosf(a2), r[j] * sinf(a2), z[j]);
            }
        glEnd();
    }

    glMaterialfv(GL_FRONT_AND_BACK, GL_AMBIENT_AND_DIFFUSE, HandleCol);

    for (i = 1; i < n; i += 2)
    {
        float a1 = i * PI * 2.0f / n;
        float a2 = (i + 1) * PI * 2.0f / n;

        glBegin(GL_TRIANGLE_STRIP);
            for (j = 1; j < 4; j++)
            {
                glNormal3f(cosf(na[j]) * cosf(a1),
                    cosf(na[j]) * sinf(a1),    sinf(na[j]));

                glVertex3f(r[j] * cosf(a1), r[j] * sinf(a1), z[j]);

                glNormal3f(cosf(na[j]) * cosf(a2),
                    cosf(na[j]) * sinf(a2), sinf(na[j]));

                glVertex3f(r[j] * cosf(a2), r[j] * sinf(a2), z[j]);
            }
        glEnd();
    }

    pQuad = gluNewQuadric();
    glTranslatef(0.0f, 0.0f, z[0]);
    gluCylinder(pQuad, r[0], r[1], z[1] - z[0], n, 1);

    glTranslatef(0.0f, 0.0f, z[3] - z[0]);
    gluDisk(pQuad, 0.0, r[3], n, 1);
    glRotatef(180.0f, 0.0f, 1.0f, 0.0f);
    glTranslatef(0.0f, 0.0f, z[3] - z[0]);
    gluDisk(pQuad, 0.0, r[0], n, 1);
    gluDeleteQuadric(pQuad);
}


/* In total 6 display lists are used.  There are created based on the DL_
 * constants defined earlier.  The function returns the index of the first
 * display list, all others can be calculated based on an offset from there. */

static int InitGLDisplayLists(void)
{
    int s = glGenLists(6);
    GLUquadric* pQuad;

    glNewList(s + DL_BALL, GL_COMPILE);
    DrawSphere(BallRad);
    glEndList();

    glNewList(s + DL_CLUB, GL_COMPILE);
    DrawClub();
    glEndList();

    glNewList(s + DL_RING, GL_COMPILE);
    DrawRing();
    glEndList();
    
    pQuad =  gluNewQuadric();
    gluQuadricNormals(pQuad, GLU_SMOOTH);    
    
    glNewList(s + DL_TORSO, GL_COMPILE);
        glPushMatrix();
            glTranslatef(ShoulderPos[0], ShoulderPos[1], -ShoulderPos[2]);
            glRotatef(-90.0f, 0.0f, 1.0f, 0.0f);
            gluCylinder(pQuad, 0.3, 0.3, ShoulderPos[0] * 2, 18, 1);
        glPopMatrix();

        glPushMatrix();
            glTranslatef(0.0f, -1.0f, -ShoulderPos[2]);
            glRotatef(-90.0f, 1.0f, 0.0f, 0.0f);
            gluCylinder(pQuad, 0.3, 0.3, ShoulderPos[1] + 1.0f, 18, 1);
            glRotatef(180.0f, 1.0f, 0.0f, 0.0f);
            gluDisk(pQuad, 0.0, 0.3, 18, 1);
        glPopMatrix();
        
        /* draw the head */
        glPushMatrix();
            glTranslatef(0.0f, ShoulderPos[1] + 1.0f, -ShoulderPos[2]);
            glRotatef(-30.0f, 1.0f, 0.0f, 0.0f);
            gluCylinder(pQuad, 0.5, 0.5, 0.3, 15, 1);
            
            glPushMatrix();
                glRotatef(180.0f, 1.0f, 0.0f, 0.0f);
                glRotatef(180.0f, 0.0f, 0.0f, 1.0f);
                gluDisk(pQuad, 0.0, 0.5, 15, 1);
            glPopMatrix(); 
                
            glTranslatef(0.0f, 0.0f, .3f);
            gluDisk(pQuad, 0.0, 0.5, 15, 1);
        glPopMatrix();        
    glEndList();
    
    glNewList(s + DL_UPPERARM, GL_COMPILE);
        gluQuadricNormals(pQuad, GLU_SMOOTH);
        gluQuadricDrawStyle(pQuad, GLU_FILL);
        gluSphere(pQuad, 0.3, 12, 8);

        gluCylinder(pQuad, 0.3, 0.3, UArmLen, 12, 1); 
        glTranslatef(0.0f, 0.0f, UArmLen);
        gluSphere(pQuad, 0.3, 12, 8);
    glEndList();

    glNewList(s + DL_FOREARM, GL_COMPILE);
        gluCylinder(pQuad, 0.3, 0.3 / 2.0f, LArmLen, 12, 1);
        glTranslatef(0.0f, 0.0f, LArmLen);
        gluDisk(pQuad, 0, 0.3 / 2.0f, 18, 1);
    glEndList();

    gluDeleteQuadric(pQuad);
    return s;
}


/* Drawing the arm requires connecting the upper and fore arm between the
 * shoulder and hand position.  Thinking about things kinematically by treating
 * the shoulder and elbow as ball joints then, provided the arm can stretch far
 * enough, there's a infnite number of ways to position the elbow.  Basically
 * it's possible to fix and hand and shoulder and then rotate the elbow a full
 * 360 degrees.  Clearly human anatomy isn't like this and picking a natural
 * elbow position can be complex.  We chicken out and assume that poking the
 * elbow out by 20 degrees from the lowest position gives a reasonably looking
 * orientation. */

static void DrawArm(RENDER_STATE* pState, float TimePos, int Left)
{
    POS Pos;
    float x, y, len, len2, ang, ang2;
    
    GetHandPosition(pState->pPattern, Left, TimePos, &Pos);

    x = Pos.x + (Left ? -ShoulderPos[0] : ShoulderPos[0]);
    y = Pos.y - ShoulderPos[1];


    len = sqrtf(x * x + y * y + ShoulderPos[2] * ShoulderPos[2]);
    len2 = sqrtf(x * x + ShoulderPos[2] * ShoulderPos[2]);

    ang = (float) CosineRule(UArmLen, len, LArmLen);
    ang2 = (float) CosineRule(UArmLen, LArmLen, len);

    if (ang == 0.0 && ang2 == 0)
        ang2 = 180.0;


    glPushMatrix();
        glTranslatef(Left ? ShoulderPos[0] : -ShoulderPos[0], ShoulderPos[1],
            -ShoulderPos[2]);
        glRotatef((float)(180.0f * asin(x / len2) / 3.14f), 0.0f, 1.0f, 0.0f);
        glRotatef((float)(-180.f * asin(y / len) / 3.14), 1.0f, 0.0f, 0.0f);
        glRotatef(Left ? 20.0f : -20.0f, 0.0f, 0.0f, 1.0f);
        glRotatef((float) ang, 1.0f, 0.0f, 0.0f);
        glCallList(DL_UPPERARM + pState->DLStart);

        glRotatef((float)(ang2 - 180.0), 1.0f, 0.0f, 0.f);
        glCallList(DL_FOREARM + pState->DLStart);
    glPopMatrix();
}


static void DrawGLScene(RENDER_STATE* pState)
{
    float Time = pState->Time;
    int nCols = sizeof(Cols) / sizeof(Cols[0]);
    int i;

    PATTERN_INFO* pPattern = pState->pPattern;

    glClear(GL_DEPTH_BUFFER_BIT | GL_COLOR_BUFFER_BIT);

    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();
    glTranslatef(5.0f * sinf(pState->TranslateAngle), 0.0f, 0.0f);

    gltrackball_rotate (pState->trackball);

    glRotatef(pState->SpinAngle, 0.0f, 1.0f, 0.0f);
    glTranslatef(0.0, 0.0, -1.0f);

    glMaterialfv(GL_FRONT_AND_BACK, GL_AMBIENT_AND_DIFFUSE, DiffCol);
    glMaterialfv(GL_FRONT_AND_BACK, GL_SPECULAR, SpecCol);
    glMaterialf(GL_FRONT_AND_BACK, GL_SHININESS, 60.0f);

    for (i = 0; i < pPattern->Objects; i++)
    {
        POS ObjPos;
        
        glMaterialfv(GL_FRONT_AND_BACK, GL_AMBIENT_AND_DIFFUSE, Cols[i % nCols]);
        glPushMatrix();

        switch (pPattern->pObjectInfo[i].ObjectType)
        {
            case OBJECT_CLUB:
                GetObjectPosition(pPattern, i, Time, 1.0f, &ObjPos);
                glTranslatef(ObjPos.x, ObjPos.y, ObjPos.z);
                glRotatef(ObjPos.Rot, 0.0f, 1.0f, 0.0f);
                glRotatef(ObjPos.Elev, -1.0f, 0.0f, 0.0f);
                glTranslatef(0.0f, 0.0f, -1.0f);
                glCallList(DL_CLUB + pState->DLStart);
                break;

            case OBJECT_RING:
                GetObjectPosition(pPattern, i, Time, 1.0f, &ObjPos);
                glTranslatef(ObjPos.x, ObjPos.y, ObjPos.z);
                glRotatef(ObjPos.Rot, 0.0f, 1.0f, 0.0f);
                glRotatef(ObjPos.Elev, -1.0f, 0.0f, 0.0f);
                glCallList(DL_RING + pState->DLStart);
                break;

            default:
                GetObjectPosition(pPattern, i, Time, 0.0f, &ObjPos);
                glTranslatef(ObjPos.x, ObjPos.y, ObjPos.z);        
                glRotatef(ObjPos.Rot, 0.6963f, 0.6963f, 0.1742f);
                glCallList(DL_BALL + pState->DLStart);
                glRotatef(90.0f, 0.0f, 1.0f, 0.0f);
                glMaterialfv(GL_FRONT_AND_BACK, GL_AMBIENT_AND_DIFFUSE, 
                    AltCols[i % nCols]);
                glCallList(DL_BALL + pState->DLStart);
                break;
        }

        glPopMatrix();
    }

    glMaterialfv(GL_FRONT_AND_BACK, GL_AMBIENT_AND_DIFFUSE, BodyCol);
    glCallList(DL_TORSO + pState->DLStart);
    DrawArm(pState, Time, 1);
    DrawArm(pState, Time, 0);
}


static int RandInRange(int Min, int Max)
{
    return Min + random() % (1 + Max - Min);
}


static void UpdatePattern(
    RENDER_STATE* pState, int MinBalls, int MaxBalls, 
    int MinHeightInc, int MaxHeightInc)
{
    if (pState->pPattern != NULL)
        ReleasePatternInfo(pState->pPattern);
    
    pState->pPattern = (PATTERN_INFO*) malloc(sizeof(PATTERN_INFO));
    
    if ((random() % 3) == 1)
    {    
        int ExtSiteLen;
        int n = random() % (sizeof(PatternText) / sizeof(PatternText[0]));
        EXT_SITE_INFO* pExtInfo = ParsePattern(PatternText[n], &ExtSiteLen);
        InitPatternInfo(pState->pPattern, NULL, pExtInfo, ExtSiteLen);
        free(pExtInfo);
    }
    else
    {
        int* pRand;
        int ballcount, maxweight;
        const int RandPatternLen = 1500;
        
        ballcount = RandInRange(MinBalls, MaxBalls);
        maxweight = ballcount  + RandInRange(MinHeightInc, MaxHeightInc);
        
        pRand = Generate(RandPatternLen, maxweight, ballcount);
        InitPatternInfo(pState->pPattern, pRand, NULL, RandPatternLen);
        free(pRand);
    }
    
    pState->CameraElev = 50.0f - random() % 90;
    pState->TranslateAngle = random() % 360;
    pState->SpinAngle = random() % 360;
    pState->Time = 50.0f;
    SetCamera(pState);
}


/*******************************************************************************
 *
 *  XScreenSaver Configuration
 *
 ******************************************************************************/

typedef struct
{
    GLXContext* glxContext;
    RENDER_STATE RenderState;
    float CurrentFrameRate;
    unsigned FramesSinceSync;
    unsigned LastSyncTime;
} JUGGLER3D_CONFIG;


#define DEF_MAX_OBJS		"8"
#define DEF_MIN_OBJS		"3"
#define DEF_MAX_HINC		"6"
#define DEF_MIN_HINC		"2"
#define DEF_JUGGLE_SPEED	"2.2"
#define DEF_TRANSLATE_SPEED	"0.1"
#define DEF_SPIN_SPEED		"20.0"

static JUGGLER3D_CONFIG* pConfigInfo = NULL;
static int MaxObjects;
static int MinObjects;
static int MaxHeightInc;
static int MinHeightInc;
static float SpinSpeed;
static float TranslateSpeed;
static float JuggleSpeed;

static XrmOptionDescRec opts[] =
{
    {"-spin", ".spinSpeed", XrmoptionSepArg, 0},
    {"-trans", ".translateSpeed", XrmoptionSepArg, 0},
    {"-speed", ".juggleSpeed", XrmoptionSepArg, 0},
    {"-maxobjs", ".maxObjs", XrmoptionSepArg, 0},
    {"-minobjs", ".minObjs", XrmoptionSepArg, 0},
    {"-maxhinc", ".maxHinc", XrmoptionSepArg, 0},
    {"-minhinc", ".minHinc", XrmoptionSepArg, 0},
};


static argtype vars[] = 
{
    {&MaxObjects, "maxObjs", "MaxObjs", DEF_MAX_OBJS, t_Int},
    {&MinObjects, "minObjs", "MinObjs", DEF_MIN_OBJS, t_Int},
    {&MaxHeightInc, "maxHinc", "MaxHinc", DEF_MAX_HINC, t_Int},
    {&MinHeightInc, "minHinc", "MinHinc", DEF_MIN_HINC, t_Int},
    {&JuggleSpeed, "juggleSpeed", "JuggleSpeed", DEF_JUGGLE_SPEED, t_Float},
    {&TranslateSpeed, "translateSpeed", "TranslateSpeed", DEF_TRANSLATE_SPEED, t_Float},
    {&SpinSpeed, "spinSpeed", "SpinSpeed", DEF_SPIN_SPEED, t_Float},
};


ENTRYPOINT ModeSpecOpt juggler3d_opts = {countof(opts), opts, countof(vars), vars};


ENTRYPOINT void reshape_juggler3d(ModeInfo *mi, int width, int height)
{
    JUGGLER3D_CONFIG* pConfig = &pConfigInfo[MI_SCREEN(mi)];
    ResizeGL(&pConfig->RenderState, width, height);
}


ENTRYPOINT void init_juggler3d(ModeInfo* mi)
{
    JUGGLER3D_CONFIG* pConfig;
    
    if (pConfigInfo == NULL)
    {
        /* Apply suitable bounds checks to the input parameters */
        MaxObjects = max(3, min(MaxObjects, 36));
        MinObjects = max(3, min(MinObjects, MaxObjects));

        MaxHeightInc = max(1, min(MaxHeightInc, 32));
        MinHeightInc = max(1, min(MinHeightInc, MaxHeightInc));
            
        pConfigInfo = (JUGGLER3D_CONFIG*) calloc(
            MI_NUM_SCREENS(mi), sizeof(JUGGLER3D_CONFIG));
        if (pConfigInfo == NULL)
        {
            fprintf(stderr, "%s: out of memory\n", progname);
            exit(1);
        }
    }
    
    pConfig = &pConfigInfo[MI_SCREEN(mi)];
    pConfig->glxContext = init_GL(mi);
    pConfig->CurrentFrameRate = 0.0f;
    pConfig->FramesSinceSync = 0;
    pConfig->LastSyncTime = 0;
    InitGLSettings(&pConfig->RenderState, MI_IS_WIREFRAME(mi));

    UpdatePattern(&pConfig->RenderState, MinObjects, MaxObjects, 
        MinHeightInc, MaxHeightInc);
    
    reshape_juggler3d(mi, MI_WIDTH(mi), MI_HEIGHT(mi));
}


ENTRYPOINT void draw_juggler3d(ModeInfo* mi)
{
    JUGGLER3D_CONFIG* pConfig = &pConfigInfo[MI_SCREEN(mi)];
    Display* pDisplay = MI_DISPLAY(mi);
    Window hwnd = MI_WINDOW(mi);

    if (pConfig->glxContext == NULL)
        return;

    glXMakeCurrent(MI_DISPLAY(mi), MI_WINDOW(mi), *(pConfig->glxContext));
    
    /* While drawing, keep track of the rendering speed so we can adjust the
     * animation speed so things appear consistent.  The basis of the this
     * code comes from the frame rate counter (fps.c) but has been modified
     * so that it reports the initial frame rate earlier (after 0.02 secs
     * instead of 1 sec). */
    
    if (pConfig->FramesSinceSync >=  1 * (int) pConfig->CurrentFrameRate)
    {
        struct timeval tvnow;
        unsigned now;
            
        # ifdef GETTIMEOFDAY_TWO_ARGS
            struct timezone tzp;
            gettimeofday(&tvnow, &tzp);
        # else
            gettimeofday(&tvnow);
        # endif
        
        now = (unsigned) (tvnow.tv_sec * 1000000 + tvnow.tv_usec);
        if (pConfig->FramesSinceSync == 0)
        {
            pConfig->LastSyncTime = now;
        }
        else
        {
            unsigned Delta = now - pConfig->LastSyncTime;
            if (Delta > 20000)
            {
                pConfig->LastSyncTime = now;
                pConfig->CurrentFrameRate = 
                    (pConfig->FramesSinceSync * 1.0e6f) / Delta;
                pConfig->FramesSinceSync = 0;
            }
        }
    }
    
    pConfig->FramesSinceSync++;
    
    if (pConfig->RenderState.Time > 150.0f)
    {
        UpdatePattern(&pConfig->RenderState, MinObjects, MaxObjects, 
            MinHeightInc, MaxHeightInc);
    }
    DrawGLScene(&pConfig->RenderState);
    
    if (pConfig->CurrentFrameRate > 1.0e-6f)
    {
        pConfig->RenderState.Time += JuggleSpeed / pConfig->CurrentFrameRate;
        pConfig->RenderState.SpinAngle += SpinSpeed / pConfig->CurrentFrameRate;
        pConfig->RenderState.TranslateAngle += 
            TranslateSpeed / pConfig->CurrentFrameRate;
    }
    
    if (mi->fps_p)
        do_fps(mi);
  
    glFinish();
    glXSwapBuffers(pDisplay, hwnd);
}


ENTRYPOINT Bool juggler3d_handle_event(ModeInfo* mi, XEvent* pEvent)
{
  JUGGLER3D_CONFIG* pConfig = &pConfigInfo[MI_SCREEN(mi)];
  RENDER_STATE* pState = &pConfig->RenderState;

    if (pEvent->xany.type == ButtonPress &&
        pEvent->xbutton.button == Button1)
    {
      pState->button_down_p = True;
      gltrackball_start (pState->trackball,
                         pEvent->xbutton.x, pEvent->xbutton.y,
                         MI_WIDTH (mi), MI_HEIGHT (mi));
      return True;
    }
    else if (pEvent->xany.type == ButtonRelease &&
             pEvent->xbutton.button == Button1)
    {
      pState->button_down_p = False;
      return True;
    }
    else if (pEvent->xany.type == ButtonPress &&
             (pEvent->xbutton.button == Button4 ||
              pEvent->xbutton.button == Button5 ||
              pEvent->xbutton.button == Button6 ||
              pEvent->xbutton.button == Button7))
    {
      gltrackball_mousewheel (pState->trackball, pEvent->xbutton.button, 2,
                              !pEvent->xbutton.state);
      return True;
    }
    else if (pEvent->xany.type == MotionNotify &&
             pState->button_down_p)
    {
      gltrackball_track (pState->trackball,
                         pEvent->xmotion.x, pEvent->xmotion.y,
                         MI_WIDTH (mi), MI_HEIGHT (mi));
      return True;
    }
    else if (pEvent->xany.type == KeyPress)
    {
        char str[20];
        KeySym Key = 0;
        int count = XLookupString(&pEvent->xkey, str, 20, &Key, 0);
        str[count] = '\0';
        if (*str == ' ')
        {
            UpdatePattern(&pConfig->RenderState, MinObjects, MaxObjects, 
                MinHeightInc, MaxHeightInc);
        }
    }
    
    return False;
}

XSCREENSAVER_MODULE ("Juggler3D", juggler3d)

#endif
