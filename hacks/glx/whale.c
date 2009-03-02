/* atlantis --- Shows moving 3D sea animals */

#if 0
static const char sccsid[] = "@(#)whale.c	1.3 98/06/18 xlockmore";
#endif

/* Copyright (c) E. Lassauge, 1998. */

/*-
 * Permission to use, copy, modify, and distribute this software and its
 * documentation for any purpose and without fee is hereby granted,
 * provided that the above copyright notice appear in all copies and that
 * both that copyright notice and this permission notice appear in
 * supporting documentation.
 *
 * This file is provided AS IS with no warranties of any kind.  The author
 * shall have no liability with respect to the infringement of copyrights,
 * trade secrets or any patents by this file or any part thereof.  In no
 * event will the author be liable for any lost revenue or profits or
 * other special, indirect and consequential damages.
 *
 * The original code for this mode was written by Mark J. Kilgard
 * as a demo for openGL programming.
 * 
 * Porting it to xlock  was possible by comparing the original Mesa's morph3d 
 * demo with it's ported version to xlock, so thanks for Marcelo F. Vianna 
 * (look at morph3d.c) for his indirect help.
 *
 * Thanks goes also to Brian Paul for making it possible and inexpensive
 * to use OpenGL at home.
 *
 * My e-mail address is lassauge@users.sourceforge.net
 *
 * Eric Lassauge  (May-13-1998)
 *
 */

/*-
 * (c) Copyright 1993, 1994, Silicon Graphics, Inc.
 * ALL RIGHTS RESERVED
 * Permission to use, copy, modify, and distribute this software for
 * any purpose and without fee is hereby granted, provided that the above
 * copyright notice appear in all copies and that both the copyright notice
 * and this permission notice appear in supporting documentation, and that
 * the name of Silicon Graphics, Inc. not be used in advertising
 * or publicity pertaining to distribution of the software without specific,
 * written prior permission.
 *
 * THE MATERIAL EMBODIED ON THIS SOFTWARE IS PROVIDED TO YOU "AS-IS"
 * AND WITHOUT WARRANTY OF ANY KIND, EXPRESS, IMPLIED OR OTHERWISE,
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTY OF MERCHANTABILITY OR
 * FITNESS FOR A PARTICULAR PURPOSE.  IN NO EVENT SHALL SILICON
 * GRAPHICS, INC.  BE LIABLE TO YOU OR ANYONE ELSE FOR ANY DIRECT,
 * SPECIAL, INCIDENTAL, INDIRECT OR CONSEQUENTIAL DAMAGES OF ANY
 * KIND, OR ANY DAMAGES WHATSOEVER, INCLUDING WITHOUT LIMITATION,
 * LOSS OF PROFIT, LOSS OF USE, SAVINGS OR REVENUE, OR THE CLAIMS OF
 * THIRD PARTIES, WHETHER OR NOT SILICON GRAPHICS, INC.  HAS BEEN
 * ADVISED OF THE POSSIBILITY OF SUCH LOSS, HOWEVER CAUSED AND ON
 * ANY THEORY OF LIABILITY, ARISING OUT OF OR IN CONNECTION WITH THE
 * POSSESSION, USE OR PERFORMANCE OF THIS SOFTWARE.
 *
 * US Government Users Restricted Rights
 * Use, duplication, or disclosure by the Government is subject to
 * restrictions set forth in FAR 52.227.19(c)(2) or subparagraph
 * (c)(1)(ii) of the Rights in Technical Data and Computer Software
 * clause at DFARS 252.227-7013 and/or in similar or successor
 * clauses in the FAR or the DOD or NASA FAR Supplement.
 * Unpublished-- rights reserved under the copyright laws of the
 * United States.  Contractor/manufacturer is Silicon Graphics,
 * Inc., 2011 N.  Shoreline Blvd., Mountain View, CA 94039-7311.
 *
 * OpenGL(TM) is a trademark of Silicon Graphics, Inc.
 */

#ifdef USE_GL

#include "atlantis.h"

/* *INDENT-OFF* */
static const float N001[3] = {0.019249, 0.01134, -0.99975};
static const float N002[3] = {-0.132579, 0.954547, 0.266952};
static const float N003[3] = {-0.196061, 0.980392, -0.019778};
static const float N004[3] = {0.695461, 0.604704, 0.388158};
static const float N005[3] = {0.8706, 0.425754, 0.246557};
static const float N006[3] = {-0.881191, 0.392012, 0.264251};
#if 0
static const float N007[3] = {0, 1, 0};
#endif
static const float N008[3] = {-0.341437, 0.887477, 0.309523};
static const float N009[3] = {0.124035, -0.992278, 0};
static const float N010[3] = {0.242536, 0, -0.970143};
static const float N011[3] = {0.588172, 0, 0.808736};
static const float N012[3] = {0.929824, -0.340623, -0.139298};
static const float N013[3] = {0.954183, 0.267108, -0.134865};
static const float N014[3] = {0.495127, 0.855436, -0.151914};
static const float N015[3] = {-0.390199, 0.906569, -0.160867};
static const float N016[3] = {-0.923605, 0.354581, -0.145692};
static const float N017[3] = {-0.955796, -0.260667, -0.136036};
static const float N018[3] = {-0.501283, -0.853462, -0.14254};
static const float N019[3] = {0.4053, -0.901974, -0.148913};
static const float N020[3] = {0.909913, -0.392746, -0.133451};
static const float N021[3] = {0.936494, 0.331147, -0.115414};
static const float N022[3] = {0.600131, 0.793724, -0.099222};
static const float N023[3] = {-0.231556, 0.968361, -0.093053};
static const float N024[3] = {-0.844369, 0.52533, -0.105211};
static const float N025[3] = {-0.982725, -0.136329, -0.125164};
static const float N026[3] = {-0.560844, -0.822654, -0.093241};
static const float N027[3] = {0.263884, -0.959981, -0.093817};
static const float N028[3] = {0.842057, -0.525192, -0.122938};
static const float N029[3] = {0.92162, 0.367565, -0.124546};
static const float N030[3] = {0.613927, 0.784109, -0.090918};
static const float N031[3] = {-0.448754, 0.888261, -0.098037};
static const float N032[3] = {-0.891865, 0.434376, -0.126077};
static const float N033[3] = {-0.881447, -0.448017, -0.149437};
static const float N034[3] = {-0.345647, -0.922057, -0.174183};
static const float N035[3] = {0.307998, -0.941371, -0.137688};
static const float N036[3] = {0.806316, -0.574647, -0.140124};
static const float N037[3] = {0.961346, 0.233646, -0.145681};
static const float N038[3] = {0.488451, 0.865586, -0.110351};
static const float N039[3] = {-0.37429, 0.921953, -0.099553};
static const float N040[3] = {-0.928504, 0.344533, -0.138485};
static const float N041[3] = {-0.918419, -0.371792, -0.135189};
static const float N042[3] = {-0.520666, -0.833704, -0.183968};
static const float N043[3] = {0.339204, -0.920273, -0.195036};
static const float N044[3] = {0.921475, -0.387382, -0.028636};
static const float N045[3] = {0.842465, 0.533335, -0.076204};
static const float N046[3] = {0.38011, 0.924939, 0.002073};
static const float N047[3] = {-0.276128, 0.961073, -0.009579};
static const float N048[3] = {-0.879684, 0.473001, -0.04925};
static const float N049[3] = {-0.947184, -0.317614, -0.044321};
static const float N050[3] = {-0.642059, -0.764933, -0.051363};
static const float N051[3] = {0.466794, -0.880921, -0.07799};
static const float N052[3] = {0.898509, -0.432277, 0.076279};
static const float N053[3] = {0.938985, 0.328141, 0.103109};
static const float N054[3] = {0.44242, 0.895745, 0.043647};
static const float N055[3] = {-0.255163, 0.966723, 0.018407};
static const float N056[3] = {-0.833769, 0.54065, 0.111924};
static const float N057[3] = {-0.953653, -0.289939, 0.080507};
static const float N058[3] = {-0.672357, -0.730524, 0.119461};
static const float N059[3] = {0.522249, -0.846652, 0.102157};
static const float N060[3] = {0.885868, -0.427631, 0.179914};
#if 0
static const float N061[3] = {0, 1, 0};
#endif
static const float N062[3] = {0.648942, 0.743116, 0.163255};
static const float N063[3] = {-0.578967, 0.80773, 0.111219};
#if 0
static const float N064[3] = {0, 1, 0};
#endif
static const float N065[3] = {-0.909864, -0.352202, 0.219321};
static const float N066[3] = {-0.502541, -0.81809, 0.27961};
static const float N067[3] = {0.322919, -0.915358, 0.240504};
static const float N068[3] = {0.242536, 0, -0.970143};
static const float N069[3] = {0, 1, 0};
static const float N070[3] = {0, 1, 0};
static const float N071[3] = {0, 1, 0};
static const float N072[3] = {0, 1, 0};
static const float N073[3] = {0, 1, 0};
static const float N074[3] = {0, 1, 0};
static const float N075[3] = {0.03122, 0.999025, -0.03122};
static const float N076[3] = {0, 1, 0};
static const float N077[3] = {0.446821, 0.893642, 0.041889};
static const float N078[3] = {0.863035, -0.10098, 0.494949};
static const float N079[3] = {0.585597, -0.808215, 0.062174};
static const float N080[3] = {0, 1, 0};
static const float N081[3] = {1, 0, 0};
static const float N082[3] = {0, 1, 0};
static const float N083[3] = {-1, 0, 0};
static const float N084[3] = {-0.478893, 0.837129, -0.264343};
static const float N085[3] = {0, 1, 0};
static const float N086[3] = {0.763909, 0.539455, -0.354163};
static const float N087[3] = {0.446821, 0.893642, 0.041889};
static const float N088[3] = {0.385134, -0.908288, 0.163352};
static const float N089[3] = {-0.605952, 0.779253, -0.159961};
static const float N090[3] = {0, 1, 0};
static const float N091[3] = {0, 1, 0};
static const float N092[3] = {0, 1, 0};
static const float N093[3] = {0, 1, 0};
static const float N094[3] = {1, 0, 0};
static const float N095[3] = {-1, 0, 0};
static const float N096[3] = {0.644444, -0.621516, 0.445433};
static const float N097[3] = {-0.760896, -0.474416, 0.442681};
static const float N098[3] = {0.636888, -0.464314, 0.615456};
static const float N099[3] = {-0.710295, 0.647038, 0.277168};
static const float N100[3] = {0.009604, 0.993655, 0.112063};
#if 0
static const float N101[3] = {0, 1, 0};
static const float N102[3] = {0, 1, 0};
static const float N103[3] = {0, 1, 0};
static const float N104[3] = {0.031837, 0.999285, 0.020415};
static const float N105[3] = {0.031837, 0.999285, 0.020415};
static const float N106[3] = {0.031837, 0.999285, 0.020415};
static const float N107[3] = {0.014647, 0.999648, 0.022115};
static const float N108[3] = {0.014647, 0.999648, 0.022115};
static const float N109[3] = {0.014647, 0.999648, 0.022115};
static const float N110[3] = {-0.985141, 0.039475, 0.167149};
static const float N111[3] = {-0.985141, 0.039475, 0.167149};
static const float N112[3] = {-0.985141, 0.039475, 0.167149};
static const float N113[3] = {0, 1, 0};
static const float N114[3] = {0, 1, 0};
static const float N115[3] = {0, 1, 0};
static const float N116[3] = {0, 1, 0};
static const float N117[3] = {0, 1, 0};
static const float N118[3] = {0, 1, 0};
static const float N119[3] = {0, 1, 0};
static const float N120[3] = {0, 1, 0};
static const float N121[3] = {0, 1, 0};
#endif
static const float iP001[3] = {18.74, 13.19, 3.76};
static       float P001[3] = {18.74, 13.19, 3.76};
static const float P002[3] = {0, 390.42, 10292.57};
static const float P003[3] = {55.8, 622.31, 8254.35};
static const float P004[3] = {20.8, 247.66, 10652.13};
static const float P005[3] = {487.51, 198.05, 9350.78};
static const float P006[3] = {-457.61, 199.04, 9353.01};
#if 0
static const float P007[3] = {0, 259, 10276.27};
#endif
static const float P008[3] = {-34.67, 247.64, 10663.71};
static const float iP009[3] = {97.46, 67.63, 593.82};
static const float iP010[3] = {-84.33, 67.63, 588.18};
static const float iP011[3] = {118.69, 8.98, -66.91};
static       float P009[3] = {97.46, 67.63, 593.82};
static       float P010[3] = {-84.33, 67.63, 588.18};
static       float P011[3] = {118.69, 8.98, -66.91};
static const float iP012[3] = {156.48, -31.95, 924.54};
static const float iP013[3] = {162, 110.22, 924.54};
static const float iP014[3] = {88.16, 221.65, 924.54};
static const float iP015[3] = {-65.21, 231.16, 924.54};
static const float iP016[3] = {-156.48, 121.97, 924.54};
static const float iP017[3] = {-162, -23.93, 924.54};
static const float iP018[3] = {-88.16, -139.1, 924.54};
static const float iP019[3] = {65.21, -148.61, 924.54};
static const float iP020[3] = {246.87, -98.73, 1783.04};
static const float iP021[3] = {253.17, 127.76, 1783.04};
static const float iP022[3] = {132.34, 270.77, 1783.04};
static const float iP023[3] = {-97.88, 285.04, 1783.04};
static const float iP024[3] = {-222.97, 139.8, 1783.04};
static const float iP025[3] = {-225.29, -86.68, 1783.04};
static const float iP026[3] = {-108.44, -224.15, 1783.04};
static const float iP027[3] = {97.88, -221.56, 1783.04};
static const float iP028[3] = {410.55, -200.66, 3213.87};
static const float iP029[3] = {432.19, 148.42, 3213.87};
static const float iP030[3] = {200.66, 410.55, 3213.87};
static const float iP031[3] = {-148.42, 432.19, 3213.87};
static const float iP032[3] = {-407.48, 171.88, 3213.87};
static const float iP033[3] = {-432.19, -148.42, 3213.87};
static const float iP034[3] = {-148.88, -309.74, 3213.87};
static const float iP035[3] = {156.38, -320.17, 3213.87};
static const float iP036[3] = {523.39, -303.81, 4424.57};
static const float iP037[3] = {574.66, 276.84, 4424.57};
static const float iP038[3] = {243.05, 492.5, 4424.57};
static const float iP039[3] = {-191.23, 520.13, 4424.57};
static const float iP040[3] = {-523.39, 304.01, 4424.57};
static const float iP041[3] = {-574.66, -231.83, 4424.57};
static const float iP042[3] = {-266.95, -578.17, 4424.57};
static const float iP043[3] = {211.14, -579.67, 4424.57};
static const float iP044[3] = {680.57, -370.27, 5943.46};
static const float iP045[3] = {834.01, 363.09, 5943.46};
static const float iP046[3] = {371.29, 614.13, 5943.46};
static const float iP047[3] = {-291.43, 621.86, 5943.46};
static const float iP048[3] = {-784.13, 362.6, 5943.46};
static const float iP049[3] = {-743.29, -325.82, 5943.46};
static const float iP050[3] = {-383.24, -804.77, 5943.46};
static const float iP051[3] = {283.47, -846.09, 5943.46};
static       float P012[3] = {156.48, -31.95, 924.54};
static       float P013[3] = {162, 110.22, 924.54};
static       float P014[3] = {88.16, 221.65, 924.54};
static       float P015[3] = {-65.21, 231.16, 924.54};
static       float P016[3] = {-156.48, 121.97, 924.54};
static       float P017[3] = {-162, -23.93, 924.54};
static       float P018[3] = {-88.16, -139.1, 924.54};
static       float P019[3] = {65.21, -148.61, 924.54};
static       float P020[3] = {246.87, -98.73, 1783.04};
static       float P021[3] = {253.17, 127.76, 1783.04};
static       float P022[3] = {132.34, 270.77, 1783.04};
static       float P023[3] = {-97.88, 285.04, 1783.04};
static       float P024[3] = {-222.97, 139.8, 1783.04};
static       float P025[3] = {-225.29, -86.68, 1783.04};
static       float P026[3] = {-108.44, -224.15, 1783.04};
static       float P027[3] = {97.88, -221.56, 1783.04};
static       float P028[3] = {410.55, -200.66, 3213.87};
static       float P029[3] = {432.19, 148.42, 3213.87};
static       float P030[3] = {200.66, 410.55, 3213.87};
static       float P031[3] = {-148.42, 432.19, 3213.87};
static       float P032[3] = {-407.48, 171.88, 3213.87};
static       float P033[3] = {-432.19, -148.42, 3213.87};
static       float P034[3] = {-148.88, -309.74, 3213.87};
static       float P035[3] = {156.38, -320.17, 3213.87};
static       float P036[3] = {523.39, -303.81, 4424.57};
static       float P037[3] = {574.66, 276.84, 4424.57};
static       float P038[3] = {243.05, 492.5, 4424.57};
static       float P039[3] = {-191.23, 520.13, 4424.57};
static       float P040[3] = {-523.39, 304.01, 4424.57};
static       float P041[3] = {-574.66, -231.83, 4424.57};
static       float P042[3] = {-266.95, -578.17, 4424.57};
static       float P043[3] = {211.14, -579.67, 4424.57};
static       float P044[3] = {680.57, -370.27, 5943.46};
static       float P045[3] = {834.01, 363.09, 5943.46};
static       float P046[3] = {371.29, 614.13, 5943.46};
static       float P047[3] = {-291.43, 621.86, 5943.46};
static       float P048[3] = {-784.13, 362.6, 5943.46};
static       float P049[3] = {-743.29, -325.82, 5943.46};
static       float P050[3] = {-383.24, -804.77, 5943.46};
static       float P051[3] = {283.47, -846.09, 5943.46};
static const float P052[3] = {599.09, -332.24, 7902.59};
static const float P053[3] = {735.48, 306.26, 7911.92};
static const float P054[3] = {321.55, 558.53, 7902.59};
static const float P055[3] = {-260.54, 559.84, 7902.59};
static const float P056[3] = {-698.66, 320.83, 7902.59};
static const float P057[3] = {-643.29, -299.16, 7902.59};
static const float P058[3] = {-341.47, -719.3, 7902.59};
static const float P059[3] = {252.57, -756.12, 7902.59};
static const float P060[3] = {458.39, -265.31, 9355.44};
static const float iP061[3] = {353.63, 138.7, 10214.2};
static       float P061[3] = {353.63, 138.7, 10214.2};
static const float P062[3] = {224.04, 438.98, 9364.77};
static const float P063[3] = {-165.71, 441.27, 9355.44};
static const float iP064[3] = {-326.4, 162.04, 10209.54};
static       float P064[3] = {-326.4, 162.04, 10209.54};
static const float P065[3] = {-473.99, -219.71, 9355.44};
static const float P066[3] = {-211.97, -479.87, 9355.44};
static const float P067[3] = {192.86, -504.03, 9355.44};
static const float iP068[3] = {-112.44, 9.25, -64.42};
static const float iP069[3] = {1155.63, 0, -182.46};
static const float iP070[3] = {-1143.13, 0, -181.54};
static const float iP071[3] = {1424.23, 0, -322.09};
static const float iP072[3] = {-1368.01, 0, -310.38};
static const float iP073[3] = {1255.57, 2.31, 114.05};
static const float iP074[3] = {-1149.38, 0, 117.12};
static const float iP075[3] = {718.36, 0, 433.36};
static const float iP076[3] = {-655.9, 0, 433.36};
static       float P068[3] = {-112.44, 9.25, -64.42};
static       float P069[3] = {1155.63, 0, -182.46};
static       float P070[3] = {-1143.13, 0, -181.54};
static       float P071[3] = {1424.23, 0, -322.09};
static       float P072[3] = {-1368.01, 0, -310.38};
static       float P073[3] = {1255.57, 2.31, 114.05};
static       float P074[3] = {-1149.38, 0, 117.12};
static       float P075[3] = {718.36, 0, 433.36};
static       float P076[3] = {-655.9, 0, 433.36};
static const float P077[3] = {1058, -2.66, 7923.51};
static const float P078[3] = {-1016.51, -15.47, 7902.87};
static const float P079[3] = {-1363.99, -484.5, 7593.38};
static const float P080[3] = {1478.09, -861.47, 7098.12};
static const float P081[3] = {1338.06, -284.68, 7024.15};
static const float P082[3] = {-1545.51, -860.64, 7106.6};
static const float P083[3] = {1063.19, -70.46, 7466.6};
static const float P084[3] = {-1369.18, -288.11, 7015.34};
static const float P085[3] = {1348.44, -482.5, 7591.41};
static const float P086[3] = {-1015.45, -96.8, 7474.86};
static const float P087[3] = {731.04, 148.38, 7682.58};
static const float P088[3] = {-697.03, 151.82, 7668.81};
static const float P089[3] = {-686.82, 157.09, 7922.29};
static const float P090[3] = {724.73, 147.75, 7931.39};
static const float iP091[3] = {0, 327.1, 2346.55};
static const float iP092[3] = {0, 552.28, 2311.31};
static const float iP093[3] = {0, 721.16, 2166.41};
static const float iP094[3] = {0, 693.42, 2388.8};
static const float iP095[3] = {0, 389.44, 2859.97};
static       float P091[3] = {0, 327.1, 2346.55};
static       float P092[3] = {0, 552.28, 2311.31};
static       float P093[3] = {0, 721.16, 2166.41};
static       float P094[3] = {0, 693.42, 2388.8};
static       float P095[3] = {0, 389.44, 2859.97};
static const float iP096[3] = {222.02, -183.67, 10266.89};
static const float iP097[3] = {-128.9, -182.7, 10266.89};
static const float iP098[3] = {41.04, 88.31, 10659.36};
static const float iP099[3] = {-48.73, 88.3, 10659.36};
static       float P096[3] = {222.02, -183.67, 10266.89};
static       float P097[3] = {-128.9, -182.7, 10266.89};
static       float P098[3] = {41.04, 88.31, 10659.36};
static       float P099[3] = {-48.73, 88.3, 10659.36};
static const float P100[3] = {0, 603.42, 9340.68};
#if 0
static const float P101[3] = {5.7, 567, 7862.98};
static const float P102[3] = {521.61, 156.61, 9162.34};
static const float P103[3] = {83.68, 566.67, 7861.26};
#endif
static const float P104[3] = {-9.86, 567.62, 7858.65};
static const float P105[3] = {31.96, 565.27, 7908.46};
static const float P106[3] = {22.75, 568.13, 7782.83};
static const float P107[3] = {58.93, 568.42, 7775.94};
static const float P108[3] = {55.91, 565.59, 7905.86};
static const float P109[3] = {99.21, 566, 7858.65};
static const float P110[3] = {-498.83, 148.14, 9135.1};
static const float P111[3] = {-495.46, 133.24, 9158.48};
static const float P112[3] = {-490.82, 146.23, 9182.76};
static const float P113[3] = {-489.55, 174.11, 9183.66};
static const float P114[3] = {-492.92, 189, 9160.28};
static const float P115[3] = {-497.56, 176.02, 9136};
static const float P116[3] = {526.54, 169.68, 9137.7};
static const float P117[3] = {523.49, 184.85, 9161.42};
static const float P118[3] = {518.56, 171.78, 9186.06};
static const float P119[3] = {516.68, 143.53, 9186.98};
static const float P120[3] = {519.73, 128.36, 9163.26};
static const float P121[3] = {524.66, 141.43, 9138.62};
/* *INDENT-ON* */



static void
Whale001(GLenum cap)
{
	glBegin(cap);
	glNormal3fv(N001);
	glVertex3fv(P001);
	glNormal3fv(N068);
	glVertex3fv(P068);
	glNormal3fv(N010);
	glVertex3fv(P010);
	glEnd();
	glBegin(cap);
	glNormal3fv(N068);
	glVertex3fv(P068);
	glNormal3fv(N076);
	glVertex3fv(P076);
	glNormal3fv(N010);
	glVertex3fv(P010);
	glEnd();
	glBegin(cap);
	glNormal3fv(N068);
	glVertex3fv(P068);
	glNormal3fv(N070);
	glVertex3fv(P070);
	glNormal3fv(N076);
	glVertex3fv(P076);
	glEnd();
	glBegin(cap);
	glNormal3fv(N076);
	glVertex3fv(P076);
	glNormal3fv(N070);
	glVertex3fv(P070);
	glNormal3fv(N074);
	glVertex3fv(P074);
	glEnd();
	glBegin(cap);
	glNormal3fv(N070);
	glVertex3fv(P070);
	glNormal3fv(N072);
	glVertex3fv(P072);
	glNormal3fv(N074);
	glVertex3fv(P074);
	glEnd();
	glBegin(cap);
	glNormal3fv(N072);
	glVertex3fv(P072);
	glNormal3fv(N070);
	glVertex3fv(P070);
	glNormal3fv(N074);
	glVertex3fv(P074);
	glEnd();
	glBegin(cap);
	glNormal3fv(N074);
	glVertex3fv(P074);
	glNormal3fv(N070);
	glVertex3fv(P070);
	glNormal3fv(N076);
	glVertex3fv(P076);
	glEnd();
	glBegin(cap);
	glNormal3fv(N070);
	glVertex3fv(P070);
	glNormal3fv(N068);
	glVertex3fv(P068);
	glNormal3fv(N076);
	glVertex3fv(P076);
	glEnd();
	glBegin(cap);
	glNormal3fv(N076);
	glVertex3fv(P076);
	glNormal3fv(N068);
	glVertex3fv(P068);
	glNormal3fv(N010);
	glVertex3fv(P010);
	glEnd();
	glBegin(cap);
	glNormal3fv(N068);
	glVertex3fv(P068);
	glNormal3fv(N001);
	glVertex3fv(P001);
	glNormal3fv(N010);
	glVertex3fv(P010);
	glEnd();
}

static void
Whale002(GLenum cap)
{
	glBegin(cap);
	glNormal3fv(N011);
	glVertex3fv(P011);
	glNormal3fv(N001);
	glVertex3fv(P001);
	glNormal3fv(N009);
	glVertex3fv(P009);
	glEnd();
	glBegin(cap);
	glNormal3fv(N075);
	glVertex3fv(P075);
	glNormal3fv(N011);
	glVertex3fv(P011);
	glNormal3fv(N009);
	glVertex3fv(P009);
	glEnd();
	glBegin(cap);
	glNormal3fv(N069);
	glVertex3fv(P069);
	glNormal3fv(N011);
	glVertex3fv(P011);
	glNormal3fv(N075);
	glVertex3fv(P075);
	glEnd();
	glBegin(cap);
	glNormal3fv(N069);
	glVertex3fv(P069);
	glNormal3fv(N075);
	glVertex3fv(P075);
	glNormal3fv(N073);
	glVertex3fv(P073);
	glEnd();
	glBegin(cap);
	glNormal3fv(N071);
	glVertex3fv(P071);
	glNormal3fv(N069);
	glVertex3fv(P069);
	glNormal3fv(N073);
	glVertex3fv(P073);
	glEnd();
	glBegin(cap);
	glNormal3fv(N001);
	glVertex3fv(P001);
	glNormal3fv(N011);
	glVertex3fv(P011);
	glNormal3fv(N009);
	glVertex3fv(P009);
	glEnd();
	glBegin(cap);
	glNormal3fv(N009);
	glVertex3fv(P009);
	glNormal3fv(N011);
	glVertex3fv(P011);
	glNormal3fv(N075);
	glVertex3fv(P075);
	glEnd();
	glBegin(cap);
	glNormal3fv(N011);
	glVertex3fv(P011);
	glNormal3fv(N069);
	glVertex3fv(P069);
	glNormal3fv(N075);
	glVertex3fv(P075);
	glEnd();
	glBegin(cap);
	glNormal3fv(N069);
	glVertex3fv(P069);
	glNormal3fv(N073);
	glVertex3fv(P073);
	glNormal3fv(N075);
	glVertex3fv(P075);
	glEnd();
	glBegin(cap);
	glNormal3fv(N069);
	glVertex3fv(P069);
	glNormal3fv(N071);
	glVertex3fv(P071);
	glNormal3fv(N073);
	glVertex3fv(P073);
	glEnd();
}

static void
Whale003(GLenum cap)
{
	glBegin(cap);
	glNormal3fv(N018);
	glVertex3fv(P018);
	glNormal3fv(N001);
	glVertex3fv(P001);
	glNormal3fv(N019);
	glVertex3fv(P019);
	glEnd();
	glBegin(cap);
	glNormal3fv(N019);
	glVertex3fv(P019);
	glNormal3fv(N001);
	glVertex3fv(P001);
	glNormal3fv(N012);
	glVertex3fv(P012);
	glEnd();
	glBegin(cap);
	glNormal3fv(N017);
	glVertex3fv(P017);
	glNormal3fv(N001);
	glVertex3fv(P001);
	glNormal3fv(N018);
	glVertex3fv(P018);
	glEnd();
	glBegin(cap);
	glNormal3fv(N001);
	glVertex3fv(P001);
	glNormal3fv(N017);
	glVertex3fv(P017);
	glNormal3fv(N016);
	glVertex3fv(P016);
	glEnd();
	glBegin(cap);
	glNormal3fv(N001);
	glVertex3fv(P001);
	glNormal3fv(N013);
	glVertex3fv(P013);
	glNormal3fv(N012);
	glVertex3fv(P012);
	glEnd();
	glBegin(cap);
	glNormal3fv(N001);
	glVertex3fv(P001);
	glNormal3fv(N016);
	glVertex3fv(P016);
	glNormal3fv(N015);
	glVertex3fv(P015);
	glEnd();
	glBegin(cap);
	glNormal3fv(N001);
	glVertex3fv(P001);
	glNormal3fv(N014);
	glVertex3fv(P014);
	glNormal3fv(N013);
	glVertex3fv(P013);
	glEnd();
	glBegin(cap);
	glNormal3fv(N001);
	glVertex3fv(P001);
	glNormal3fv(N015);
	glVertex3fv(P015);
	glNormal3fv(N014);
	glVertex3fv(P014);
	glEnd();
}

static void
Whale004(GLenum cap)
{
	glBegin(cap);
	glNormal3fv(N014);
	glVertex3fv(P014);
	glNormal3fv(N015);
	glVertex3fv(P015);
	glNormal3fv(N023);
	glVertex3fv(P023);
	glNormal3fv(N022);
	glVertex3fv(P022);
	glEnd();
	glBegin(cap);
	glNormal3fv(N015);
	glVertex3fv(P015);
	glNormal3fv(N016);
	glVertex3fv(P016);
	glNormal3fv(N024);
	glVertex3fv(P024);
	glNormal3fv(N023);
	glVertex3fv(P023);
	glEnd();
	glBegin(cap);
	glNormal3fv(N016);
	glVertex3fv(P016);
	glNormal3fv(N017);
	glVertex3fv(P017);
	glNormal3fv(N025);
	glVertex3fv(P025);
	glNormal3fv(N024);
	glVertex3fv(P024);
	glEnd();
	glBegin(cap);
	glNormal3fv(N017);
	glVertex3fv(P017);
	glNormal3fv(N018);
	glVertex3fv(P018);
	glNormal3fv(N026);
	glVertex3fv(P026);
	glNormal3fv(N025);
	glVertex3fv(P025);
	glEnd();
	glBegin(cap);
	glNormal3fv(N013);
	glVertex3fv(P013);
	glNormal3fv(N014);
	glVertex3fv(P014);
	glNormal3fv(N022);
	glVertex3fv(P022);
	glNormal3fv(N021);
	glVertex3fv(P021);
	glEnd();
	glBegin(cap);
	glNormal3fv(N012);
	glVertex3fv(P012);
	glNormal3fv(N013);
	glVertex3fv(P013);
	glNormal3fv(N021);
	glVertex3fv(P021);
	glNormal3fv(N020);
	glVertex3fv(P020);
	glEnd();
	glBegin(cap);
	glNormal3fv(N018);
	glVertex3fv(P018);
	glNormal3fv(N019);
	glVertex3fv(P019);
	glNormal3fv(N027);
	glVertex3fv(P027);
	glNormal3fv(N026);
	glVertex3fv(P026);
	glEnd();
	glBegin(cap);
	glNormal3fv(N019);
	glVertex3fv(P019);
	glNormal3fv(N012);
	glVertex3fv(P012);
	glNormal3fv(N020);
	glVertex3fv(P020);
	glNormal3fv(N027);
	glVertex3fv(P027);
	glEnd();
}

static void
Whale005(GLenum cap)
{
	glBegin(cap);
	glNormal3fv(N022);
	glVertex3fv(P022);
	glNormal3fv(N023);
	glVertex3fv(P023);
	glNormal3fv(N031);
	glVertex3fv(P031);
	glNormal3fv(N030);
	glVertex3fv(P030);
	glEnd();
	glBegin(cap);
	glNormal3fv(N021);
	glVertex3fv(P021);
	glNormal3fv(N022);
	glVertex3fv(P022);
	glNormal3fv(N030);
	glVertex3fv(P030);
	glEnd();
	glBegin(cap);
	glNormal3fv(N021);
	glVertex3fv(P021);
	glNormal3fv(N030);
	glVertex3fv(P030);
	glNormal3fv(N029);
	glVertex3fv(P029);
	glEnd();
	glBegin(cap);
	glNormal3fv(N023);
	glVertex3fv(P023);
	glNormal3fv(N024);
	glVertex3fv(P024);
	glNormal3fv(N031);
	glVertex3fv(P031);
	glEnd();
	glBegin(cap);
	glNormal3fv(N024);
	glVertex3fv(P024);
	glNormal3fv(N032);
	glVertex3fv(P032);
	glNormal3fv(N031);
	glVertex3fv(P031);
	glEnd();
	glBegin(cap);
	glNormal3fv(N024);
	glVertex3fv(P024);
	glNormal3fv(N025);
	glVertex3fv(P025);
	glNormal3fv(N032);
	glVertex3fv(P032);
	glEnd();
	glBegin(cap);
	glNormal3fv(N025);
	glVertex3fv(P025);
	glNormal3fv(N033);
	glVertex3fv(P033);
	glNormal3fv(N032);
	glVertex3fv(P032);
	glEnd();
	glBegin(cap);
	glNormal3fv(N020);
	glVertex3fv(P020);
	glNormal3fv(N021);
	glVertex3fv(P021);
	glNormal3fv(N029);
	glVertex3fv(P029);
	glEnd();
	glBegin(cap);
	glNormal3fv(N020);
	glVertex3fv(P020);
	glNormal3fv(N029);
	glVertex3fv(P029);
	glNormal3fv(N028);
	glVertex3fv(P028);
	glEnd();
	glBegin(cap);
	glNormal3fv(N027);
	glVertex3fv(P027);
	glNormal3fv(N020);
	glVertex3fv(P020);
	glNormal3fv(N028);
	glVertex3fv(P028);
	glEnd();
	glBegin(cap);
	glNormal3fv(N027);
	glVertex3fv(P027);
	glNormal3fv(N028);
	glVertex3fv(P028);
	glNormal3fv(N035);
	glVertex3fv(P035);
	glEnd();
	glBegin(cap);
	glNormal3fv(N025);
	glVertex3fv(P025);
	glNormal3fv(N026);
	glVertex3fv(P026);
	glNormal3fv(N033);
	glVertex3fv(P033);
	glEnd();
	glBegin(cap);
	glNormal3fv(N033);
	glVertex3fv(P033);
	glNormal3fv(N026);
	glVertex3fv(P026);
	glNormal3fv(N034);
	glVertex3fv(P034);
	glEnd();
	glBegin(cap);
	glNormal3fv(N026);
	glVertex3fv(P026);
	glNormal3fv(N027);
	glVertex3fv(P027);
	glNormal3fv(N035);
	glVertex3fv(P035);
	glNormal3fv(N034);
	glVertex3fv(P034);
	glEnd();
}

static void
Whale006(GLenum cap)
{
	glBegin(cap);
	glNormal3fv(N092);
	glVertex3fv(P092);
	glNormal3fv(N093);
	glVertex3fv(P093);
	glNormal3fv(N094);
	glVertex3fv(P094);
	glEnd();
	glBegin(cap);
	glNormal3fv(N093);
	glVertex3fv(P093);
	glNormal3fv(N092);
	glVertex3fv(P092);
	glNormal3fv(N094);
	glVertex3fv(P094);
	glEnd();
	glBegin(cap);
	glNormal3fv(N092);
	glVertex3fv(P092);
	glNormal3fv(N091);
	glVertex3fv(P091);
	glNormal3fv(N095);
	glVertex3fv(P095);
	glNormal3fv(N094);
	glVertex3fv(P094);
	glEnd();
	glBegin(cap);
	glNormal3fv(N091);
	glVertex3fv(P091);
	glNormal3fv(N092);
	glVertex3fv(P092);
	glNormal3fv(N094);
	glVertex3fv(P094);
	glNormal3fv(N095);
	glVertex3fv(P095);
	glEnd();
}

static void
Whale007(GLenum cap)
{
	glBegin(cap);
	glNormal3fv(N030);
	glVertex3fv(P030);
	glNormal3fv(N031);
	glVertex3fv(P031);
	glNormal3fv(N039);
	glVertex3fv(P039);
	glNormal3fv(N038);
	glVertex3fv(P038);
	glEnd();
	glBegin(cap);
	glNormal3fv(N029);
	glVertex3fv(P029);
	glNormal3fv(N030);
	glVertex3fv(P030);
	glNormal3fv(N038);
	glVertex3fv(P038);
	glEnd();
	glBegin(cap);
	glNormal3fv(N029);
	glVertex3fv(P029);
	glNormal3fv(N038);
	glVertex3fv(P038);
	glNormal3fv(N037);
	glVertex3fv(P037);
	glEnd();
	glBegin(cap);
	glNormal3fv(N028);
	glVertex3fv(P028);
	glNormal3fv(N029);
	glVertex3fv(P029);
	glNormal3fv(N037);
	glVertex3fv(P037);
	glEnd();
	glBegin(cap);
	glNormal3fv(N028);
	glVertex3fv(P028);
	glNormal3fv(N037);
	glVertex3fv(P037);
	glNormal3fv(N036);
	glVertex3fv(P036);
	glEnd();
	glBegin(cap);
	glNormal3fv(N035);
	glVertex3fv(P035);
	glNormal3fv(N028);
	glVertex3fv(P028);
	glNormal3fv(N036);
	glVertex3fv(P036);
	glEnd();
	glBegin(cap);
	glNormal3fv(N035);
	glVertex3fv(P035);
	glNormal3fv(N036);
	glVertex3fv(P036);
	glNormal3fv(N043);
	glVertex3fv(P043);
	glEnd();
	glBegin(cap);
	glNormal3fv(N034);
	glVertex3fv(P034);
	glNormal3fv(N035);
	glVertex3fv(P035);
	glNormal3fv(N043);
	glVertex3fv(P043);
	glNormal3fv(N042);
	glVertex3fv(P042);
	glEnd();
	glBegin(cap);
	glNormal3fv(N033);
	glVertex3fv(P033);
	glNormal3fv(N034);
	glVertex3fv(P034);
	glNormal3fv(N042);
	glVertex3fv(P042);
	glEnd();
	glBegin(cap);
	glNormal3fv(N033);
	glVertex3fv(P033);
	glNormal3fv(N042);
	glVertex3fv(P042);
	glNormal3fv(N041);
	glVertex3fv(P041);
	glEnd();
	glBegin(cap);
	glNormal3fv(N031);
	glVertex3fv(P031);
	glNormal3fv(N032);
	glVertex3fv(P032);
	glNormal3fv(N039);
	glVertex3fv(P039);
	glEnd();
	glBegin(cap);
	glNormal3fv(N039);
	glVertex3fv(P039);
	glNormal3fv(N032);
	glVertex3fv(P032);
	glNormal3fv(N040);
	glVertex3fv(P040);
	glEnd();
	glBegin(cap);
	glNormal3fv(N032);
	glVertex3fv(P032);
	glNormal3fv(N033);
	glVertex3fv(P033);
	glNormal3fv(N040);
	glVertex3fv(P040);
	glEnd();
	glBegin(cap);
	glNormal3fv(N040);
	glVertex3fv(P040);
	glNormal3fv(N033);
	glVertex3fv(P033);
	glNormal3fv(N041);
	glVertex3fv(P041);
	glEnd();
}

static void
Whale008(GLenum cap)
{
	glBegin(cap);
	glNormal3fv(N042);
	glVertex3fv(P042);
	glNormal3fv(N043);
	glVertex3fv(P043);
	glNormal3fv(N051);
	glVertex3fv(P051);
	glNormal3fv(N050);
	glVertex3fv(P050);
	glEnd();
	glBegin(cap);
	glNormal3fv(N043);
	glVertex3fv(P043);
	glNormal3fv(N036);
	glVertex3fv(P036);
	glNormal3fv(N051);
	glVertex3fv(P051);
	glEnd();
	glBegin(cap);
	glNormal3fv(N051);
	glVertex3fv(P051);
	glNormal3fv(N036);
	glVertex3fv(P036);
	glNormal3fv(N044);
	glVertex3fv(P044);
	glEnd();
	glBegin(cap);
	glNormal3fv(N041);
	glVertex3fv(P041);
	glNormal3fv(N042);
	glVertex3fv(P042);
	glNormal3fv(N050);
	glVertex3fv(P050);
	glEnd();
	glBegin(cap);
	glNormal3fv(N041);
	glVertex3fv(P041);
	glNormal3fv(N050);
	glVertex3fv(P050);
	glNormal3fv(N049);
	glVertex3fv(P049);
	glEnd();
	glBegin(cap);
	glNormal3fv(N036);
	glVertex3fv(P036);
	glNormal3fv(N037);
	glVertex3fv(P037);
	glNormal3fv(N044);
	glVertex3fv(P044);
	glEnd();
	glBegin(cap);
	glNormal3fv(N044);
	glVertex3fv(P044);
	glNormal3fv(N037);
	glVertex3fv(P037);
	glNormal3fv(N045);
	glVertex3fv(P045);
	glEnd();
	glBegin(cap);
	glNormal3fv(N040);
	glVertex3fv(P040);
	glNormal3fv(N041);
	glVertex3fv(P041);
	glNormal3fv(N049);
	glVertex3fv(P049);
	glEnd();
	glBegin(cap);
	glNormal3fv(N040);
	glVertex3fv(P040);
	glNormal3fv(N049);
	glVertex3fv(P049);
	glNormal3fv(N048);
	glVertex3fv(P048);
	glEnd();
	glBegin(cap);
	glNormal3fv(N039);
	glVertex3fv(P039);
	glNormal3fv(N040);
	glVertex3fv(P040);
	glNormal3fv(N048);
	glVertex3fv(P048);
	glEnd();
	glBegin(cap);
	glNormal3fv(N039);
	glVertex3fv(P039);
	glNormal3fv(N048);
	glVertex3fv(P048);
	glNormal3fv(N047);
	glVertex3fv(P047);
	glEnd();
	glBegin(cap);
	glNormal3fv(N037);
	glVertex3fv(P037);
	glNormal3fv(N038);
	glVertex3fv(P038);
	glNormal3fv(N045);
	glVertex3fv(P045);
	glEnd();
	glBegin(cap);
	glNormal3fv(N038);
	glVertex3fv(P038);
	glNormal3fv(N046);
	glVertex3fv(P046);
	glNormal3fv(N045);
	glVertex3fv(P045);
	glEnd();
	glBegin(cap);
	glNormal3fv(N038);
	glVertex3fv(P038);
	glNormal3fv(N039);
	glVertex3fv(P039);
	glNormal3fv(N047);
	glVertex3fv(P047);
	glNormal3fv(N046);
	glVertex3fv(P046);
	glEnd();
}

static void
Whale009(GLenum cap)
{
	glBegin(cap);
	glNormal3fv(N050);
	glVertex3fv(P050);
	glNormal3fv(N051);
	glVertex3fv(P051);
	glNormal3fv(N059);
	glVertex3fv(P059);
	glNormal3fv(N058);
	glVertex3fv(P058);
	glEnd();
	glBegin(cap);
	glNormal3fv(N051);
	glVertex3fv(P051);
	glNormal3fv(N044);
	glVertex3fv(P044);
	glNormal3fv(N059);
	glVertex3fv(P059);
	glEnd();
	glBegin(cap);
	glNormal3fv(N059);
	glVertex3fv(P059);
	glNormal3fv(N044);
	glVertex3fv(P044);
	glNormal3fv(N052);
	glVertex3fv(P052);
	glEnd();
	glBegin(cap);
	glNormal3fv(N044);
	glVertex3fv(P044);
	glNormal3fv(N045);
	glVertex3fv(P045);
	glNormal3fv(N053);
	glVertex3fv(P053);
	glEnd();
	glBegin(cap);
	glNormal3fv(N044);
	glVertex3fv(P044);
	glNormal3fv(N053);
	glVertex3fv(P053);
	glNormal3fv(N052);
	glVertex3fv(P052);
	glEnd();
	glBegin(cap);
	glNormal3fv(N049);
	glVertex3fv(P049);
	glNormal3fv(N050);
	glVertex3fv(P050);
	glNormal3fv(N058);
	glVertex3fv(P058);
	glEnd();
	glBegin(cap);
	glNormal3fv(N049);
	glVertex3fv(P049);
	glNormal3fv(N058);
	glVertex3fv(P058);
	glNormal3fv(N057);
	glVertex3fv(P057);
	glEnd();
	glBegin(cap);
	glNormal3fv(N048);
	glVertex3fv(P048);
	glNormal3fv(N049);
	glVertex3fv(P049);
	glNormal3fv(N057);
	glVertex3fv(P057);
	glEnd();
	glBegin(cap);
	glNormal3fv(N048);
	glVertex3fv(P048);
	glNormal3fv(N057);
	glVertex3fv(P057);
	glNormal3fv(N056);
	glVertex3fv(P056);
	glEnd();
	glBegin(cap);
	glNormal3fv(N047);
	glVertex3fv(P047);
	glNormal3fv(N048);
	glVertex3fv(P048);
	glNormal3fv(N056);
	glVertex3fv(P056);
	glEnd();
	glBegin(cap);
	glNormal3fv(N047);
	glVertex3fv(P047);
	glNormal3fv(N056);
	glVertex3fv(P056);
	glNormal3fv(N055);
	glVertex3fv(P055);
	glEnd();
	glBegin(cap);
	glNormal3fv(N045);
	glVertex3fv(P045);
	glNormal3fv(N046);
	glVertex3fv(P046);
	glNormal3fv(N053);
	glVertex3fv(P053);
	glEnd();
	glBegin(cap);
	glNormal3fv(N046);
	glVertex3fv(P046);
	glNormal3fv(N054);
	glVertex3fv(P054);
	glNormal3fv(N053);
	glVertex3fv(P053);
	glEnd();
	glBegin(cap);
	glNormal3fv(N046);
	glVertex3fv(P046);
	glNormal3fv(N047);
	glVertex3fv(P047);
	glNormal3fv(N055);
	glVertex3fv(P055);
	glNormal3fv(N054);
	glVertex3fv(P054);
	glEnd();
}

static void
Whale010(GLenum cap)
{
	glBegin(cap);
	glNormal3fv(N080);
	glVertex3fv(P080);
	glNormal3fv(N081);
	glVertex3fv(P081);
	glNormal3fv(N085);
	glVertex3fv(P085);
	glEnd();
	glBegin(cap);
	glNormal3fv(N081);
	glVertex3fv(P081);
	glNormal3fv(N083);
	glVertex3fv(P083);
	glNormal3fv(N085);
	glVertex3fv(P085);
	glEnd();
	glBegin(cap);
	glNormal3fv(N085);
	glVertex3fv(P085);
	glNormal3fv(N083);
	glVertex3fv(P083);
	glNormal3fv(N077);
	glVertex3fv(P077);
	glEnd();
	glBegin(cap);
	glNormal3fv(N083);
	glVertex3fv(P083);
	glNormal3fv(N087);
	glVertex3fv(P087);
	glNormal3fv(N077);
	glVertex3fv(P077);
	glEnd();
	glBegin(cap);
	glNormal3fv(N077);
	glVertex3fv(P077);
	glNormal3fv(N087);
	glVertex3fv(P087);
	glNormal3fv(N090);
	glVertex3fv(P090);
	glEnd();
	glBegin(cap);
	glNormal3fv(N081);
	glVertex3fv(P081);
	glNormal3fv(N080);
	glVertex3fv(P080);
	glNormal3fv(N085);
	glVertex3fv(P085);
	glEnd();
	glBegin(cap);
	glNormal3fv(N083);
	glVertex3fv(P083);
	glNormal3fv(N081);
	glVertex3fv(P081);
	glNormal3fv(N085);
	glVertex3fv(P085);
	glEnd();
	glBegin(cap);
	glNormal3fv(N083);
	glVertex3fv(P083);
	glNormal3fv(N085);
	glVertex3fv(P085);
	glNormal3fv(N077);
	glVertex3fv(P077);
	glEnd();
	glBegin(cap);
	glNormal3fv(N087);
	glVertex3fv(P087);
	glNormal3fv(N083);
	glVertex3fv(P083);
	glNormal3fv(N077);
	glVertex3fv(P077);
	glEnd();
	glBegin(cap);
	glNormal3fv(N087);
	glVertex3fv(P087);
	glNormal3fv(N077);
	glVertex3fv(P077);
	glNormal3fv(N090);
	glVertex3fv(P090);
	glEnd();
}

static void
Whale011(GLenum cap)
{
	glBegin(cap);
	glNormal3fv(N082);
	glVertex3fv(P082);
	glNormal3fv(N084);
	glVertex3fv(P084);
	glNormal3fv(N079);
	glVertex3fv(P079);
	glEnd();
	glBegin(cap);
	glNormal3fv(N084);
	glVertex3fv(P084);
	glNormal3fv(N086);
	glVertex3fv(P086);
	glNormal3fv(N079);
	glVertex3fv(P079);
	glEnd();
	glBegin(cap);
	glNormal3fv(N079);
	glVertex3fv(P079);
	glNormal3fv(N086);
	glVertex3fv(P086);
	glNormal3fv(N078);
	glVertex3fv(P078);
	glEnd();
	glBegin(cap);
	glNormal3fv(N086);
	glVertex3fv(P086);
	glNormal3fv(N088);
	glVertex3fv(P088);
	glNormal3fv(N078);
	glVertex3fv(P078);
	glEnd();
	glBegin(cap);
	glNormal3fv(N078);
	glVertex3fv(P078);
	glNormal3fv(N088);
	glVertex3fv(P088);
	glNormal3fv(N089);
	glVertex3fv(P089);
	glEnd();
	glBegin(cap);
	glNormal3fv(N088);
	glVertex3fv(P088);
	glNormal3fv(N086);
	glVertex3fv(P086);
	glNormal3fv(N089);
	glVertex3fv(P089);
	glEnd();
	glBegin(cap);
	glNormal3fv(N089);
	glVertex3fv(P089);
	glNormal3fv(N086);
	glVertex3fv(P086);
	glNormal3fv(N078);
	glVertex3fv(P078);
	glEnd();
	glBegin(cap);
	glNormal3fv(N086);
	glVertex3fv(P086);
	glNormal3fv(N084);
	glVertex3fv(P084);
	glNormal3fv(N078);
	glVertex3fv(P078);
	glEnd();
	glBegin(cap);
	glNormal3fv(N078);
	glVertex3fv(P078);
	glNormal3fv(N084);
	glVertex3fv(P084);
	glNormal3fv(N079);
	glVertex3fv(P079);
	glEnd();
	glBegin(cap);
	glNormal3fv(N084);
	glVertex3fv(P084);
	glNormal3fv(N082);
	glVertex3fv(P082);
	glNormal3fv(N079);
	glVertex3fv(P079);
	glEnd();
}

static void
Whale012(GLenum cap)
{
	glBegin(cap);
	glNormal3fv(N058);
	glVertex3fv(P058);
	glNormal3fv(N059);
	glVertex3fv(P059);
	glNormal3fv(N067);
	glVertex3fv(P067);
	glNormal3fv(N066);
	glVertex3fv(P066);
	glEnd();
	glBegin(cap);
	glNormal3fv(N059);
	glVertex3fv(P059);
	glNormal3fv(N052);
	glVertex3fv(P052);
	glNormal3fv(N060);
	glVertex3fv(P060);
	glEnd();
	glBegin(cap);
	glNormal3fv(N059);
	glVertex3fv(P059);
	glNormal3fv(N060);
	glVertex3fv(P060);
	glNormal3fv(N067);
	glVertex3fv(P067);
	glEnd();
	glBegin(cap);
	glNormal3fv(N058);
	glVertex3fv(P058);
	glNormal3fv(N066);
	glVertex3fv(P066);
	glNormal3fv(N065);
	glVertex3fv(P065);
	glEnd();
	glBegin(cap);
	glNormal3fv(N058);
	glVertex3fv(P058);
	glNormal3fv(N065);
	glVertex3fv(P065);
	glNormal3fv(N057);
	glVertex3fv(P057);
	glEnd();
	glBegin(cap);
	glNormal3fv(N056);
	glVertex3fv(P056);
	glNormal3fv(N057);
	glVertex3fv(P057);
	glNormal3fv(N065);
	glVertex3fv(P065);
	glEnd();
	glBegin(cap);
	glNormal3fv(N056);
	glVertex3fv(P056);
	glNormal3fv(N065);
	glVertex3fv(P065);
	glNormal3fv(N006);
	glVertex3fv(P006);
	glEnd();
	glBegin(cap);
	glNormal3fv(N056);
	glVertex3fv(P056);
	glNormal3fv(N006);
	glVertex3fv(P006);
	glNormal3fv(N063);
	glVertex3fv(P063);
	glEnd();
	glBegin(cap);
	glNormal3fv(N056);
	glVertex3fv(P056);
	glNormal3fv(N063);
	glVertex3fv(P063);
	glNormal3fv(N055);
	glVertex3fv(P055);
	glEnd();
	glBegin(cap);
	glNormal3fv(N054);
	glVertex3fv(P054);
	glNormal3fv(N062);
	glVertex3fv(P062);
	glNormal3fv(N005);
	glVertex3fv(P005);
	glEnd();
	glBegin(cap);
	glNormal3fv(N054);
	glVertex3fv(P054);
	glNormal3fv(N005);
	glVertex3fv(P005);
	glNormal3fv(N053);
	glVertex3fv(P053);
	glEnd();
	glBegin(cap);
	glNormal3fv(N053);
	glVertex3fv(P053);
	glNormal3fv(N005);
	glVertex3fv(P005);
	glNormal3fv(N060);
	glVertex3fv(P060);
	glEnd();
	glBegin(cap);
	glNormal3fv(N053);
	glVertex3fv(P053);
	glNormal3fv(N060);
	glVertex3fv(P060);
	glNormal3fv(N052);
	glVertex3fv(P052);
	glEnd();
}

static void
Whale013(GLenum cap)
{
	glBegin(cap);
	glNormal3fv(N066);
	glVertex3fv(P066);
	glNormal3fv(N067);
	glVertex3fv(P067);
	glNormal3fv(N096);
	glVertex3fv(P096);
	glNormal3fv(N097);
	glVertex3fv(P097);
	glEnd();
	glBegin(cap);
	glNormal3fv(N097);
	glVertex3fv(P097);
	glNormal3fv(N096);
	glVertex3fv(P096);
	glNormal3fv(N098);
	glVertex3fv(P098);
	glNormal3fv(N099);
	glVertex3fv(P099);
	glEnd();
	glBegin(cap);
	glNormal3fv(N065);
	glVertex3fv(P065);
	glNormal3fv(N066);
	glVertex3fv(P066);
	glNormal3fv(N097);
	glVertex3fv(P097);
	glEnd();
	glBegin(cap);
	glNormal3fv(N067);
	glVertex3fv(P067);
	glNormal3fv(N060);
	glVertex3fv(P060);
	glNormal3fv(N096);
	glVertex3fv(P096);
	glEnd();
	glBegin(cap);
	glNormal3fv(N060);
	glVertex3fv(P060);
	glNormal3fv(N005);
	glVertex3fv(P005);
	glNormal3fv(N096);
	glVertex3fv(P096);
	glEnd();
	glBegin(cap);
	glNormal3fv(N096);
	glVertex3fv(P096);
	glNormal3fv(N005);
	glVertex3fv(P005);
	glNormal3fv(N098);
	glVertex3fv(P098);
	glEnd();
	glBegin(cap);
	glNormal3fv(N006);
	glVertex3fv(P006);
	glNormal3fv(N065);
	glVertex3fv(P065);
	glNormal3fv(N097);
	glVertex3fv(P097);
	glEnd();
	glBegin(cap);
	glNormal3fv(N006);
	glVertex3fv(P006);
	glNormal3fv(N097);
	glVertex3fv(P097);
	glNormal3fv(N099);
	glVertex3fv(P099);
	glEnd();
	glBegin(cap);
	glVertex3fv(P005);
	glVertex3fv(P006);
	glVertex3fv(P099);
	glVertex3fv(P098);
	glEnd();
}

static void
Whale014(GLenum cap)
{
	glBegin(cap);
	glNormal3fv(N062);
	glVertex3fv(P062);
	glNormal3fv(N004);
	glVertex3fv(P004);
	glNormal3fv(N005);
	glVertex3fv(P005);
	glEnd();
	glBegin(cap);
	glVertex3fv(P006);
	glVertex3fv(P005);
	glVertex3fv(P004);
	glVertex3fv(P008);
	glEnd();
	glBegin(cap);
	glNormal3fv(N063);
	glVertex3fv(P063);
	glNormal3fv(N006);
	glVertex3fv(P006);
	glNormal3fv(N002);
	glVertex3fv(P002);
	glEnd();
	glBegin(cap);
	glNormal3fv(N002);
	glVertex3fv(P002);
	glNormal3fv(N006);
	glVertex3fv(P006);
	glNormal3fv(N008);
	glVertex3fv(P008);
	glEnd();
	glBegin(cap);
	glNormal3fv(N002);
	glVertex3fv(P002);
	glNormal3fv(N008);
	glVertex3fv(P008);
	glNormal3fv(N004);
	glVertex3fv(P004);
	glEnd();
	glBegin(cap);
	glNormal3fv(N062);
	glVertex3fv(P062);
	glNormal3fv(N002);
	glVertex3fv(P002);
	glNormal3fv(N004);
	glVertex3fv(P004);
	glEnd();
}

static void
Whale015(GLenum cap)
{
	glBegin(cap);
	glNormal3fv(N055);
	glVertex3fv(P055);
	glNormal3fv(N003);
	glVertex3fv(P003);
	glNormal3fv(N054);
	glVertex3fv(P054);
	glEnd();
	glBegin(cap);
	glNormal3fv(N003);
	glVertex3fv(P003);
	glNormal3fv(N055);
	glVertex3fv(P055);
	glNormal3fv(N063);
	glVertex3fv(P063);
	glEnd();
	glBegin(cap);
	glNormal3fv(N003);
	glVertex3fv(P003);
	glNormal3fv(N063);
	glVertex3fv(P063);
	glNormal3fv(N100);
	glVertex3fv(P100);
	glEnd();
	glBegin(cap);
	glNormal3fv(N003);
	glVertex3fv(P003);
	glNormal3fv(N100);
	glVertex3fv(P100);
	glNormal3fv(N054);
	glVertex3fv(P054);
	glEnd();
	glBegin(cap);
	glNormal3fv(N054);
	glVertex3fv(P054);
	glNormal3fv(N100);
	glVertex3fv(P100);
	glNormal3fv(N062);
	glVertex3fv(P062);
	glEnd();
	glBegin(cap);
	glNormal3fv(N100);
	glVertex3fv(P100);
	glNormal3fv(N063);
	glVertex3fv(P063);
	glNormal3fv(N002);
	glVertex3fv(P002);
	glEnd();
	glBegin(cap);
	glNormal3fv(N100);
	glVertex3fv(P100);
	glNormal3fv(N002);
	glVertex3fv(P002);
	glNormal3fv(N062);
	glVertex3fv(P062);
	glEnd();
}

static void
Whale016(GLenum cap)
{
	glBegin(cap);
	glVertex3fv(P104);
	glVertex3fv(P105);
	glVertex3fv(P106);
	glEnd();
	glBegin(cap);
	glVertex3fv(P107);
	glVertex3fv(P108);
	glVertex3fv(P109);
	glEnd();
	glBegin(cap);
	glVertex3fv(P110);
	glVertex3fv(P111);
	glVertex3fv(P112);
	glVertex3fv(P113);
	glVertex3fv(P114);
	glVertex3fv(P115);
	glEnd();
	glBegin(cap);
	glVertex3fv(P116);
	glVertex3fv(P117);
	glVertex3fv(P118);
	glVertex3fv(P119);
	glVertex3fv(P120);
	glVertex3fv(P121);
	glEnd();
}

void
DrawWhale(fishRec * fish, int wire)
{
	float       seg0, seg1, seg2, seg3, seg4, seg5, seg6, seg7;
	float       pitch, thrash, chomp;
	GLenum      cap;

	fish->htail = (int) (fish->htail - (int) (5 * fish->v)) % 360;

	thrash = 70 * fish->v;

	seg0 = 1.5 * thrash * sin((fish->htail) * RRAD);
	seg1 = 2.5 * thrash * sin((fish->htail + 10) * RRAD);
	seg2 = 3.7 * thrash * sin((fish->htail + 15) * RRAD);
	seg3 = 4.8 * thrash * sin((fish->htail + 23) * RRAD);
	seg4 = 6 * thrash * sin((fish->htail + 28) * RRAD);
	seg5 = 6.5 * thrash * sin((fish->htail + 35) * RRAD);
	seg6 = 6.5 * thrash * sin((fish->htail + 40) * RRAD);
	seg7 = 6.5 * thrash * sin((fish->htail + 55) * RRAD);

	pitch = fish->v * sin((fish->htail - 160) * RRAD);

	chomp = 0;
	if (fish->v > 2) {
		chomp = -(fish->v - 2) * 200;
	}
	P012[1] = iP012[1] + seg5;
	P013[1] = iP013[1] + seg5;
	P014[1] = iP014[1] + seg5;
	P015[1] = iP015[1] + seg5;
	P016[1] = iP016[1] + seg5;
	P017[1] = iP017[1] + seg5;
	P018[1] = iP018[1] + seg5;
	P019[1] = iP019[1] + seg5;

	P020[1] = iP020[1] + seg4;
	P021[1] = iP021[1] + seg4;
	P022[1] = iP022[1] + seg4;
	P023[1] = iP023[1] + seg4;
	P024[1] = iP024[1] + seg4;
	P025[1] = iP025[1] + seg4;
	P026[1] = iP026[1] + seg4;
	P027[1] = iP027[1] + seg4;

	P028[1] = iP028[1] + seg2;
	P029[1] = iP029[1] + seg2;
	P030[1] = iP030[1] + seg2;
	P031[1] = iP031[1] + seg2;
	P032[1] = iP032[1] + seg2;
	P033[1] = iP033[1] + seg2;
	P034[1] = iP034[1] + seg2;
	P035[1] = iP035[1] + seg2;

	P036[1] = iP036[1] + seg1;
	P037[1] = iP037[1] + seg1;
	P038[1] = iP038[1] + seg1;
	P039[1] = iP039[1] + seg1;
	P040[1] = iP040[1] + seg1;
	P041[1] = iP041[1] + seg1;
	P042[1] = iP042[1] + seg1;
	P043[1] = iP043[1] + seg1;

	P044[1] = iP044[1] + seg0;
	P045[1] = iP045[1] + seg0;
	P046[1] = iP046[1] + seg0;
	P047[1] = iP047[1] + seg0;
	P048[1] = iP048[1] + seg0;
	P049[1] = iP049[1] + seg0;
	P050[1] = iP050[1] + seg0;
	P051[1] = iP051[1] + seg0;

	P009[1] = iP009[1] + seg6;
	P010[1] = iP010[1] + seg6;
	P075[1] = iP075[1] + seg6;
	P076[1] = iP076[1] + seg6;

	P001[1] = iP001[1] + seg7;
	P011[1] = iP011[1] + seg7;
	P068[1] = iP068[1] + seg7;
	P069[1] = iP069[1] + seg7;
	P070[1] = iP070[1] + seg7;
	P071[1] = iP071[1] + seg7;
	P072[1] = iP072[1] + seg7;
	P073[1] = iP073[1] + seg7;
	P074[1] = iP074[1] + seg7;

	P091[1] = iP091[1] + seg3 * 1.1;
	P092[1] = iP092[1] + seg3;
	P093[1] = iP093[1] + seg3;
	P094[1] = iP094[1] + seg3;
	P095[1] = iP095[1] + seg3 * 0.9;

	P099[1] = iP099[1] + chomp;
	P098[1] = iP098[1] + chomp;
	P064[1] = iP064[1] + chomp;
	P061[1] = iP061[1] + chomp;
	P097[1] = iP097[1] + chomp;
	P096[1] = iP096[1] + chomp;

	glPushMatrix();

	glRotatef(pitch, 1, 0, 0);

	glTranslatef(0, 0, 8000);

	glRotatef(180, 0, 1, 0);

	glScalef(3, 3, 3);

	glEnable(GL_CULL_FACE);

	cap = wire ? GL_LINE_LOOP : GL_POLYGON;
	Whale001(cap);
	Whale002(cap);
	Whale003(cap);
	Whale004(cap);
	Whale005(cap);
	Whale006(cap);
	Whale007(cap);
	Whale008(cap);
	Whale009(cap);
	Whale010(cap);
	Whale011(cap);
	Whale012(cap);
	Whale013(cap);
	Whale014(cap);
	Whale015(cap);
	Whale016(cap);

	glDisable(GL_CULL_FACE);

	glPopMatrix();
}
#endif
