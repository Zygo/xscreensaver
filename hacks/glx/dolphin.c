/* atlantis --- Shows moving 3D sea animals */

#if 0
static const char sccsid[] = "@(#)dolphin.c	1.2 98/06/16 xlockmore";
#endif

/* Copyright (c) E. Lassauge, 1998. */

/*
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
 * My e-mail address is lassauge@sagem.fr
 *
 * Eric Lassauge  (May-13-1998)
 *
 */

/**
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
static const float N001[3] = {-0.005937, -0.101998, -0.994767};
static const float N002[3] = {0.93678, -0.200803, 0.286569};
static const float N003[3] = {-0.233062, 0.972058, 0.028007};
#if 0
static const float N004[3] = {0, 1, 0};
#endif
static const float N005[3] = {0.898117, 0.360171, 0.252315};
static const float N006[3] = {-0.915437, 0.348456, 0.201378};
static const float N007[3] = {0.602263, -0.777527, 0.18092};
static const float N008[3] = {-0.906912, -0.412015, 0.088061};
#if 0
static const float N009[3] = {-0.015623, 0.999878, 0};
static const float N010[3] = {0, -0.992278, 0.124035};
static const float N011[3] = {0, -0.936329, -0.351123};
#endif
static const float N012[3] = {0.884408, -0.429417, -0.182821};
static const float N013[3] = {0.921121, 0.311084, -0.234016};
static const float N014[3] = {0.382635, 0.877882, -0.287948};
static const float N015[3] = {-0.380046, 0.888166, -0.258316};
static const float N016[3] = {-0.891515, 0.392238, -0.226607};
static const float N017[3] = {-0.901419, -0.382002, -0.203763};
static const float N018[3] = {-0.367225, -0.911091, -0.187243};
static const float N019[3] = {0.339539, -0.924846, -0.171388};
static const float N020[3] = {0.914706, -0.378617, -0.14129};
static const float N021[3] = {0.950662, 0.262713, -0.164994};
static const float N022[3] = {0.546359, 0.80146, -0.243218};
static const float N023[3] = {-0.315796, 0.917068, -0.243431};
static const float N024[3] = {-0.825687, 0.532277, -0.186875};
static const float N025[3] = {-0.974763, -0.155232, -0.160435};
static const float N026[3] = {-0.560596, -0.816658, -0.137119};
static const float N027[3] = {0.38021, -0.910817, -0.160786};
static const float N028[3] = {0.923772, -0.358322, -0.135093};
static const float N029[3] = {0.951202, 0.275053, -0.139859};
static const float N030[3] = {0.686099, 0.702548, -0.188932};
static const float N031[3] = {-0.521865, 0.826719, -0.21022};
static const float N032[3] = {-0.92382, 0.346739, -0.162258};
static const float N033[3] = {-0.902095, -0.409995, -0.134646};
static const float N034[3] = {-0.509115, -0.848498, -0.144404};
static const float N035[3] = {0.456469, -0.880293, -0.129305};
static const float N036[3] = {0.873401, -0.475489, -0.105266};
static const float N037[3] = {0.970825, 0.179861, -0.158584};
static const float N038[3] = {0.675609, 0.714187, -0.183004};
static const float N039[3] = {-0.523574, 0.830212, -0.19136};
static const float N040[3] = {-0.958895, 0.230808, -0.165071};
static const float N041[3] = {-0.918285, -0.376803, -0.121542};
static const float N042[3] = {-0.622467, -0.774167, -0.114888};
static const float N043[3] = {0.404497, -0.908807, -0.102231};
static const float N044[3] = {0.930538, -0.365155, -0.027588};
static const float N045[3] = {0.92192, 0.374157, -0.100345};
static const float N046[3] = {0.507346, 0.860739, 0.041562};
static const float N047[3] = {-0.394646, 0.918815, -0.00573};
static const float N048[3] = {-0.925411, 0.373024, -0.066837};
static const float N049[3] = {-0.945337, -0.322309, -0.049551};
static const float N050[3] = {-0.660437, -0.750557, -0.022072};
static const float N051[3] = {0.488835, -0.87195, -0.027261};
static const float N052[3] = {0.902599, -0.421397, 0.087969};
static const float N053[3] = {0.938636, 0.322606, 0.12202};
static const float N054[3] = {0.484605, 0.871078, 0.079878};
static const float N055[3] = {-0.353607, 0.931559, 0.084619};
static const float N056[3] = {-0.867759, 0.478564, 0.134054};
static const float N057[3] = {-0.951583, -0.29603, 0.082794};
static const float N058[3] = {-0.672355, -0.730209, 0.121384};
static const float N059[3] = {0.528336, -0.842452, 0.105525};
static const float N060[3] = {0.786913, -0.56476, 0.248627};
#if 0
static const float N061[3] = {0, 1, 0};
#endif
static const float N062[3] = {0.622098, 0.76523, 0.165584};
static const float N063[3] = {-0.631711, 0.767816, 0.106773};
static const float N064[3] = {-0.687886, 0.606351, 0.398938};
static const float N065[3] = {-0.946327, -0.281623, 0.158598};
static const float N066[3] = {-0.509549, -0.860437, 0.002776};
static const float N067[3] = {0.462594, -0.876692, 0.131977};
#if 0
static const float N068[3] = {0, -0.992278, 0.124035};
static const float N069[3] = {0, -0.970143, -0.242536};
static const float N070[3] = {0.015502, 0.992159, -0.12402};
#endif
static const float N071[3] = {0, 1, 0};
#if 0
static const float N072[3] = {0, 1, 0};
static const float N073[3] = {0, 1, 0};
static const float N074[3] = {0, -1, 0};
static const float N075[3] = {-0.242536, 0, -0.970143};
static const float N076[3] = {-0.010336, -0.992225, -0.124028};
#endif
static const float N077[3] = {-0.88077, 0.461448, 0.106351};
static const float N078[3] = {-0.88077, 0.461448, 0.106351};
static const float N079[3] = {-0.88077, 0.461448, 0.106351};
static const float N080[3] = {-0.88077, 0.461448, 0.106351};
static const float N081[3] = {-0.571197, 0.816173, 0.087152};
static const float N082[3] = {-0.88077, 0.461448, 0.106351};
static const float N083[3] = {-0.571197, 0.816173, 0.087152};
static const float N084[3] = {-0.571197, 0.816173, 0.087152};
static const float N085[3] = {-0.88077, 0.461448, 0.106351};
static const float N086[3] = {-0.571197, 0.816173, 0.087152};
static const float N087[3] = {-0.88077, 0.461448, 0.106351};
static const float N088[3] = {-0.88077, 0.461448, 0.106351};
static const float N089[3] = {-0.88077, 0.461448, 0.106351};
static const float N090[3] = {-0.88077, 0.461448, 0.106351};
static const float N091[3] = {0, 1, 0};
static const float N092[3] = {0, 1, 0};
static const float N093[3] = {0, 1, 0};
static const float N094[3] = {1, 0, 0};
static const float N095[3] = {-1, 0, 0};
#if 0
static const float N096[3] = {0, 1, 0};
#endif
static const float N097[3] = {-0.697296, 0.702881, 0.140491};
static const float N098[3] = {0.918864, 0.340821, 0.198819};
static const float N099[3] = {-0.932737, 0.201195, 0.299202};
static const float N100[3] = {0.029517, 0.981679, 0.188244};
#if 0
static const float N101[3] = {0, 1, 0};
#endif
static const float N102[3] = {0.813521, -0.204936, 0.544229};
#if 0
static const float N103[3] = {0, 1, 0};
static const float N104[3] = {0, 1, 0};
static const float N105[3] = {0, 1, 0};
static const float N106[3] = {0, 1, 0};
static const float N107[3] = {0, 1, 0};
static const float N108[3] = {0, 1, 0};
static const float N109[3] = {0, 1, 0};
#endif
static const float N110[3] = {-0.78148, -0.384779, 0.491155};
static const float N111[3] = {-0.722243, 0.384927, 0.574627};
static const float N112[3] = {-0.752278, 0.502679, 0.425901};
static const float N113[3] = {0.547257, 0.36791, 0.751766};
static const float N114[3] = {0.725949, -0.232568, 0.647233};
static const float N115[3] = {-0.747182, -0.660786, 0.07128};
static const float N116[3] = {0.931519, 0.200748, 0.30327};
static const float N117[3] = {-0.828928, 0.313757, 0.463071};
static const float N118[3] = {0.902554, -0.370967, 0.218587};
static const float N119[3] = {-0.879257, -0.441851, 0.177973};
static const float N120[3] = {0.642327, 0.611901, 0.461512};
static const float N121[3] = {0.964817, -0.202322, 0.16791};
static const float N122[3] = {0, 1, 0};
#if 0
static const float N123[3] = {-0.980734, 0.041447, 0.1909};
static const float N124[3] = {-0.980734, 0.041447, 0.1909};
static const float N125[3] = {-0.980734, 0.041447, 0.1909};
static const float N126[3] = {0, 1, 0};
static const float N127[3] = {0, 1, 0};
static const float N128[3] = {0, 1, 0};
static const float N129[3] = {0.96325, 0.004839, 0.268565};
static const float N130[3] = {0.96325, 0.004839, 0.268565};
static const float N131[3] = {0.96325, 0.004839, 0.268565};
static const float N132[3] = {0, 1, 0};
static const float N133[3] = {0, 1, 0};
static const float N134[3] = {0, 1, 0};
#endif
static       float P001[3] = {5.68, -300.95, 1324.7};
static const float P002[3] = {338.69, -219.63, 9677.03};
static const float P003[3] = {12.18, 474.59, 9138.14};
#if 0
static const float P004[3] = {-7.49, -388.91, 10896.74};
#endif
static const float P005[3] = {487.51, 198.05, 9350.78};
static const float P006[3] = {-457.61, 68.74, 9427.85};
static const float P007[3] = {156.52, -266.72, 10311.68};
static const float P008[3] = {-185.56, -266.51, 10310.47};
static       float P009[3] = {124.39, -261.46, 1942.34};
static       float P010[3] = {-130.05, -261.46, 1946.03};
static       float P011[3] = {141.07, -320.11, 1239.38};
static       float P012[3] = {156.48, -360.12, 2073.41};
static       float P013[3] = {162, -175.88, 2064.44};
static       float P014[3] = {88.16, -87.72, 2064.02};
static       float P015[3] = {-65.21, -96.13, 2064.02};
static       float P016[3] = {-156.48, -180.96, 2064.44};
static       float P017[3] = {-162, -368.93, 2082.39};
static       float P018[3] = {-88.16, -439.22, 2082.39};
static       float P019[3] = {65.21, -440.32, 2083.39};
static       float P020[3] = {246.87, -356.02, 2576.95};
static       float P021[3] = {253.17, -111.15, 2567.15};
static       float P022[3] = {132.34, 51.41, 2559.84};
static       float P023[3] = {-97.88, 40.44, 2567.15};
static       float P024[3] = {-222.97, -117.49, 2567.15};
static       float P025[3] = {-252.22, -371.53, 2569.92};
static       float P026[3] = {-108.44, -518.19, 2586.75};
static       float P027[3] = {97.88, -524.79, 2586.75};
static       float P028[3] = {370.03, -421.19, 3419.7};
static       float P029[3] = {351.15, -16.98, 3423.17};
static       float P030[3] = {200.66, 248.46, 3430.37};
static       float P031[3] = {-148.42, 235.02, 3417.91};
static       float P032[3] = {-360.21, -30.27, 3416.84};
static       float P033[3] = {-357.9, -414.89, 3407.04};
static       float P034[3] = {-148.88, -631.35, 3409.9};
static       float P035[3] = {156.38, -632.59, 3419.7};
static       float P036[3] = {462.61, -469.21, 4431.51};
static       float P037[3] = {466.6, 102.25, 4434.98};
static       float P038[3] = {243.05, 474.34, 4562.02};
static       float P039[3] = {-191.23, 474.4, 4554.42};
static       float P040[3] = {-476.12, 111.05, 4451.11};
static       float P041[3] = {-473.36, -470.74, 4444.78};
static       float P042[3] = {-266.95, -748.41, 4447.78};
static       float P043[3] = {211.14, -749.91, 4429.73};
static       float P044[3] = {680.57, -370.27, 5943.46};
static       float P045[3] = {834.01, 363.09, 6360.63};
static       float P046[3] = {371.29, 804.51, 6486.26};
static       float P047[3] = {-291.43, 797.22, 6494.28};
static       float P048[3] = {-784.13, 370.75, 6378.01};
static       float P049[3] = {-743.29, -325.82, 5943.46};
static       float P050[3] = {-383.24, -804.77, 5943.46};
static       float P051[3] = {283.47, -846.09, 5943.46};
static const float iP001[3] = {5.68, -300.95, 1324.7};
#if 0
static const float iP002[3] = {338.69, -219.63, 9677.03};
static const float iP003[3] = {12.18, 624.93, 8956.39};
static const float iP004[3] = {-7.49, -388.91, 10896.74};
static const float iP005[3] = {487.51, 198.05, 9350.78};
static const float iP006[3] = {-457.61, 199.04, 9353.01};
static const float iP007[3] = {156.52, -266.72, 10311.68};
static const float iP008[3] = {-185.56, -266.51, 10310.47};
#endif
static const float iP009[3] = {124.39, -261.46, 1942.34};
static const float iP010[3] = {-130.05, -261.46, 1946.03};
static const float iP011[3] = {141.07, -320.11, 1239.38};
static const float iP012[3] = {156.48, -360.12, 2073.41};
static const float iP013[3] = {162, -175.88, 2064.44};
static const float iP014[3] = {88.16, -87.72, 2064.02};
static const float iP015[3] = {-65.21, -96.13, 2064.02};
static const float iP016[3] = {-156.48, -180.96, 2064.44};
static const float iP017[3] = {-162, -368.93, 2082.39};
static const float iP018[3] = {-88.16, -439.22, 2082.39};
static const float iP019[3] = {65.21, -440.32, 2083.39};
static const float iP020[3] = {246.87, -356.02, 2576.95};
static const float iP021[3] = {253.17, -111.15, 2567.15};
static const float iP022[3] = {132.34, 51.41, 2559.84};
static const float iP023[3] = {-97.88, 40.44, 2567.15};
static const float iP024[3] = {-222.97, -117.49, 2567.15};
static const float iP025[3] = {-252.22, -371.53, 2569.92};
static const float iP026[3] = {-108.44, -518.19, 2586.75};
static const float iP027[3] = {97.88, -524.79, 2586.75};
static const float iP028[3] = {370.03, -421.19, 3419.7};
static const float iP029[3] = {351.15, -16.98, 3423.17};
static const float iP030[3] = {200.66, 248.46, 3430.37};
static const float iP031[3] = {-148.42, 235.02, 3417.91};
static const float iP032[3] = {-360.21, -30.27, 3416.84};
static const float iP033[3] = {-357.9, -414.89, 3407.04};
static const float iP034[3] = {-148.88, -631.35, 3409.9};
static const float iP035[3] = {156.38, -632.59, 3419.7};
static const float iP036[3] = {462.61, -469.21, 4431.51};
static const float iP037[3] = {466.6, 102.25, 4434.98};
static const float iP038[3] = {243.05, 474.34, 4562.02};
static const float iP039[3] = {-191.23, 474.4, 4554.42};
static const float iP040[3] = {-476.12, 111.05, 4451.11};
static const float iP041[3] = {-473.36, -470.74, 4444.78};
static const float iP042[3] = {-266.95, -748.41, 4447.78};
static const float iP043[3] = {211.14, -749.91, 4429.73};
static const float iP044[3] = {680.57, -370.27, 5943.46};
static const float iP045[3] = {834.01, 363.09, 6360.63};
static const float iP046[3] = {371.29, 804.51, 6486.26};
static const float iP047[3] = {-291.43, 797.22, 6494.28};
static const float iP048[3] = {-784.13, 370.75, 6378.01};
static const float iP049[3] = {-743.29, -325.82, 5943.46};
static const float iP050[3] = {-383.24, -804.77, 5943.46};
static const float iP051[3] = {283.47, -846.09, 5943.46};
static const float P052[3] = {599.09, -300.15, 7894.03};
static const float P053[3] = {735.48, 306.26, 7911.92};
static const float P054[3] = {246.22, 558.53, 8460.5};
static const float P055[3] = {-230.41, 559.84, 8473.23};
static const float P056[3] = {-698.66, 320.83, 7902.59};
static const float P057[3] = {-643.29, -299.16, 7902.59};
static const float P058[3] = {-341.47, -719.3, 7902.59};
static const float P059[3] = {252.57, -756.12, 7902.59};
static const float P060[3] = {458.39, -265.31, 9355.44};
#if 0
static const float P061[3] = {433.38, -161.9, 9503.03};
#endif
static const float P062[3] = {224.04, 338.75, 9450.3};
static const float P063[3] = {-165.71, 341.04, 9462.35};
static const float P064[3] = {-298.11, 110.13, 10180.37};
static const float P065[3] = {-473.99, -219.71, 9355.44};
static const float P066[3] = {-211.97, -479.87, 9355.44};
static const float P067[3] = {192.86, -491.45, 9348.73};
static       float P068[3] = {-136.29, -319.84, 1228.73};
static       float P069[3] = {1111.17, -314.14, 1314.19};
static       float P070[3] = {-1167.34, -321.61, 1319.45};
static       float P071[3] = {1404.86, -306.66, 1235.45};
static       float P072[3] = {-1409.73, -314.14, 1247.66};
static       float P073[3] = {1254.01, -296.87, 1544.58};
static       float P074[3] = {-1262.09, -291.7, 1504.26};
static       float P075[3] = {965.71, -269.26, 1742.65};
static       float P076[3] = {-900.97, -276.74, 1726.07};
static const float iP068[3] = {-136.29, -319.84, 1228.73};
static const float iP069[3] = {1111.17, -314.14, 1314.19};
static const float iP070[3] = {-1167.34, -321.61, 1319.45};
static const float iP071[3] = {1404.86, -306.66, 1235.45};
static const float iP072[3] = {-1409.73, -314.14, 1247.66};
static const float iP073[3] = {1254.01, -296.87, 1544.58};
static const float iP074[3] = {-1262.09, -291.7, 1504.26};
static const float iP075[3] = {965.71, -269.26, 1742.65};
static const float iP076[3] = {-900.97, -276.74, 1726.07};
static const float P077[3] = {1058, -448.81, 8194.66};
static const float P078[3] = {-1016.51, -456.43, 8190.62};
static const float P079[3] = {-1515.96, -676.45, 7754.93};
static const float P080[3] = {1856.75, -830.34, 7296.56};
static const float P081[3] = {1472.16, -497.38, 7399.68};
static const float P082[3] = {-1775.26, -829.51, 7298.46};
static const float P083[3] = {911.09, -252.51, 7510.99};
static const float P084[3] = {-1451.94, -495.62, 7384.3};
static const float P085[3] = {1598.75, -669.26, 7769.9};
static const float P086[3] = {-836.53, -250.08, 7463.25};
static const float P087[3] = {722.87, -158.18, 8006.41};
static const float P088[3] = {-688.86, -162.28, 7993.89};
static const float P089[3] = {-626.92, -185.3, 8364.98};
static const float P090[3] = {647.72, -189.46, 8354.99};
static       float P091[3] = {0, 835.01, 5555.62};
static       float P092[3] = {0, 1350.18, 5220.86};
static       float P093[3] = {0, 1422.94, 5285.27};
static       float P094[3] = {0, 1296.75, 5650.19};
static       float P095[3] = {0, 795.63, 6493.88};
static const float iP091[3] = {0, 835.01, 5555.62};
static const float iP092[3] = {0, 1350.18, 5220.86};
static const float iP093[3] = {0, 1422.94, 5285.27};
static const float iP094[3] = {0, 1296.75, 5650.19};
static const float iP095[3] = {0, 795.63, 6493.88};
#if 0
static const float P096[3] = {-447.38, -165.99, 9499.6};
#endif
static       float P097[3] = {-194.91, -357.14, 10313.32};
static       float P098[3] = {135.35, -357.66, 10307.94};
static const float iP097[3] = {-194.91, -357.14, 10313.32};
static const float iP098[3] = {135.35, -357.66, 10307.94};
static const float P099[3] = {-380.53, -221.14, 9677.98};
static const float P100[3] = {0, 412.99, 9629.33};
#if 0
static const float P101[3] = {5.7, 567, 7862.98};
#endif
static       float P102[3] = {59.51, -412.55, 10677.58};
static const float iP102[3] = {59.51, -412.55, 10677.58};
static const float P103[3] = {6.5, 484.74, 9009.94};
#if 0
static const float P104[3] = {-9.86, 567.62, 7858.65};
#endif
static const float P105[3] = {-41.86, 476.51, 9078.17};
#if 0
static const float P106[3] = {22.75, 568.13, 7782.83};
static const float P107[3] = {58.93, 568.42, 7775.94};
#endif
static const float P108[3] = {49.2, 476.83, 9078.24};
#if 0
static const float P109[3] = {99.21, 566, 7858.65};
#endif
static       float P110[3] = {-187.62, -410.04, 10674.12};
static const float iP110[3] = {-187.62, -410.04, 10674.12};
static       float P111[3] = {-184.25, -318.7, 10723.88};
static const float iP111[3] = {-184.25, -318.7, 10723.88};
static const float P112[3] = {-179.61, -142.81, 10670.26};
static const float P113[3] = {57.43, -147.94, 10675.26};
static const float P114[3] = {54.06, -218.9, 10712.44};
static const float P115[3] = {-186.35, -212.09, 10713.76};
static const float P116[3] = {205.9, -84.61, 10275.97};
static const float P117[3] = {-230.96, -83.26, 10280.09};
static const float iP118[3] = {216.78, -509.17, 10098.94};
static const float iP119[3] = {-313.21, -510.79, 10102.62};
static       float P118[3] = {216.78, -509.17, 10098.94};
static       float P119[3] = {-313.21, -510.79, 10102.62};
static const float P120[3] = {217.95, 96.34, 10161.62};
static       float P121[3] = {71.99, -319.74, 10717.7};
static const float iP121[3] = {71.99, -319.74, 10717.7};
static       float P122[3] = {0, 602.74, 5375.84};
static const float iP122[3] = {0, 602.74, 5375.84};
static const float P123[3] = {-448.94, -203.14, 9499.6};
static const float P124[3] = {-442.64, -185.2, 9528.07};
static const float P125[3] = {-441.07, -148.05, 9528.07};
static const float P126[3] = {-443.43, -128.84, 9499.6};
static const float P127[3] = {-456.87, -146.78, 9466.67};
static const float P128[3] = {-453.68, -183.93, 9466.67};
static const float P129[3] = {428.43, -124.08, 9503.03};
static const float P130[3] = {419.73, -142.14, 9534.56};
static const float P131[3] = {419.92, -179.96, 9534.56};
static const float P132[3] = {431.2, -199.73, 9505.26};
static const float P133[3] = {442.28, -181.67, 9475.96};
static const float P134[3] = {442.08, -143.84, 9475.96};
/* *INDENT-ON* */



static void
Dolphin001(GLenum cap)
{
	glNormal3fv(N071);
	glBegin(cap);
	glVertex3fv(P001);
	glVertex3fv(P068);
	glVertex3fv(P010);
	glEnd();
	glBegin(cap);
	glVertex3fv(P068);
	glVertex3fv(P076);
	glVertex3fv(P010);
	glEnd();
	glBegin(cap);
	glVertex3fv(P068);
	glVertex3fv(P070);
	glVertex3fv(P076);
	glEnd();
	glBegin(cap);
	glVertex3fv(P076);
	glVertex3fv(P070);
	glVertex3fv(P074);
	glEnd();
	glBegin(cap);
	glVertex3fv(P070);
	glVertex3fv(P072);
	glVertex3fv(P074);
	glEnd();
	glNormal3fv(N119);
	glBegin(cap);
	glVertex3fv(P072);
	glVertex3fv(P070);
	glVertex3fv(P074);
	glEnd();
	glBegin(cap);
	glVertex3fv(P074);
	glVertex3fv(P070);
	glVertex3fv(P076);
	glEnd();
	glBegin(cap);
	glVertex3fv(P070);
	glVertex3fv(P068);
	glVertex3fv(P076);
	glEnd();
	glBegin(cap);
	glVertex3fv(P076);
	glVertex3fv(P068);
	glVertex3fv(P010);
	glEnd();
	glBegin(cap);
	glVertex3fv(P068);
	glVertex3fv(P001);
	glVertex3fv(P010);
	glEnd();
}

static void
Dolphin002(GLenum cap)
{
	glNormal3fv(N071);
	glBegin(cap);
	glVertex3fv(P011);
	glVertex3fv(P001);
	glVertex3fv(P009);
	glEnd();
	glBegin(cap);
	glVertex3fv(P075);
	glVertex3fv(P011);
	glVertex3fv(P009);
	glEnd();
	glBegin(cap);
	glVertex3fv(P069);
	glVertex3fv(P011);
	glVertex3fv(P075);
	glEnd();
	glBegin(cap);
	glVertex3fv(P069);
	glVertex3fv(P075);
	glVertex3fv(P073);
	glEnd();
	glBegin(cap);
	glVertex3fv(P071);
	glVertex3fv(P069);
	glVertex3fv(P073);
	glEnd();
	glNormal3fv(N119);
	glBegin(cap);
	glVertex3fv(P001);
	glVertex3fv(P011);
	glVertex3fv(P009);
	glEnd();
	glBegin(cap);
	glVertex3fv(P009);
	glVertex3fv(P011);
	glVertex3fv(P075);
	glEnd();
	glBegin(cap);
	glVertex3fv(P011);
	glVertex3fv(P069);
	glVertex3fv(P075);
	glEnd();
	glBegin(cap);
	glVertex3fv(P069);
	glVertex3fv(P073);
	glVertex3fv(P075);
	glEnd();
	glBegin(cap);
	glVertex3fv(P069);
	glVertex3fv(P071);
	glVertex3fv(P073);
	glEnd();
}

static void
Dolphin003(GLenum cap)
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
Dolphin004(GLenum cap)
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
Dolphin005(GLenum cap)
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
Dolphin006(GLenum cap)
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
	glBegin(cap);
	glNormal3fv(N122);
	glVertex3fv(P122);
	glNormal3fv(N095);
	glVertex3fv(P095);
	glNormal3fv(N091);
	glVertex3fv(P091);
	glEnd();
	glBegin(cap);
	glNormal3fv(N122);
	glVertex3fv(P122);
	glNormal3fv(N091);
	glVertex3fv(P091);
	glNormal3fv(N095);
	glVertex3fv(P095);
	glEnd();
}

static void
Dolphin007(GLenum cap)
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
Dolphin008(GLenum cap)
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
Dolphin009(GLenum cap)
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
Dolphin010(GLenum cap)
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
Dolphin011(GLenum cap)
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
Dolphin012(GLenum cap)
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
	glNormal3fv(N052);
	glVertex3fv(P052);
	glNormal3fv(N053);
	glVertex3fv(P053);
	glNormal3fv(N005);
	glVertex3fv(P005);
	glNormal3fv(N060);
	glVertex3fv(P060);
	glEnd();
}

static void
Dolphin013(GLenum cap)
{
	glBegin(cap);
	glNormal3fv(N116);
	glVertex3fv(P116);
	glNormal3fv(N117);
	glVertex3fv(P117);
	glNormal3fv(N112);
	glVertex3fv(P112);
	glNormal3fv(N113);
	glVertex3fv(P113);
	glEnd();
	glBegin(cap);
	glNormal3fv(N114);
	glVertex3fv(P114);
	glNormal3fv(N113);
	glVertex3fv(P113);
	glNormal3fv(N112);
	glVertex3fv(P112);
	glNormal3fv(N115);
	glVertex3fv(P115);
	glEnd();
	glBegin(cap);
	glNormal3fv(N114);
	glVertex3fv(P114);
	glNormal3fv(N116);
	glVertex3fv(P116);
	glNormal3fv(N113);
	glVertex3fv(P113);
	glEnd();
	glBegin(cap);
	glNormal3fv(N114);
	glVertex3fv(P114);
	glNormal3fv(N007);
	glVertex3fv(P007);
	glNormal3fv(N116);
	glVertex3fv(P116);
	glEnd();
	glBegin(cap);
	glNormal3fv(N007);
	glVertex3fv(P007);
	glNormal3fv(N002);
	glVertex3fv(P002);
	glNormal3fv(N116);
	glVertex3fv(P116);
	glEnd();
	glBegin(cap);
	glVertex3fv(P002);
	glVertex3fv(P007);
	glVertex3fv(P008);
	glVertex3fv(P099);
	glEnd();
	glBegin(cap);
	glVertex3fv(P007);
	glVertex3fv(P114);
	glVertex3fv(P115);
	glVertex3fv(P008);
	glEnd();
	glBegin(cap);
	glNormal3fv(N117);
	glVertex3fv(P117);
	glNormal3fv(N099);
	glVertex3fv(P099);
	glNormal3fv(N008);
	glVertex3fv(P008);
	glEnd();
	glBegin(cap);
	glNormal3fv(N117);
	glVertex3fv(P117);
	glNormal3fv(N008);
	glVertex3fv(P008);
	glNormal3fv(N112);
	glVertex3fv(P112);
	glEnd();
	glBegin(cap);
	glNormal3fv(N112);
	glVertex3fv(P112);
	glNormal3fv(N008);
	glVertex3fv(P008);
	glNormal3fv(N115);
	glVertex3fv(P115);
	glEnd();
}

static void
Dolphin014(GLenum cap)
{
	glBegin(cap);
	glNormal3fv(N111);
	glVertex3fv(P111);
	glNormal3fv(N110);
	glVertex3fv(P110);
	glNormal3fv(N102);
	glVertex3fv(P102);
	glNormal3fv(N121);
	glVertex3fv(P121);
	glEnd();
	glBegin(cap);
	glNormal3fv(N111);
	glVertex3fv(P111);
	glNormal3fv(N097);
	glVertex3fv(P097);
	glNormal3fv(N110);
	glVertex3fv(P110);
	glEnd();
	glBegin(cap);
	glNormal3fv(N097);
	glVertex3fv(P097);
	glNormal3fv(N119);
	glVertex3fv(P119);
	glNormal3fv(N110);
	glVertex3fv(P110);
	glEnd();
	glBegin(cap);
	glNormal3fv(N097);
	glVertex3fv(P097);
	glNormal3fv(N099);
	glVertex3fv(P099);
	glNormal3fv(N119);
	glVertex3fv(P119);
	glEnd();
	glBegin(cap);
	glNormal3fv(N099);
	glVertex3fv(P099);
	glNormal3fv(N065);
	glVertex3fv(P065);
	glNormal3fv(N119);
	glVertex3fv(P119);
	glEnd();
	glBegin(cap);
	glNormal3fv(N065);
	glVertex3fv(P065);
	glNormal3fv(N066);
	glVertex3fv(P066);
	glNormal3fv(N119);
	glVertex3fv(P119);
	glEnd();
	glBegin(cap);
	glVertex3fv(P098);
	glVertex3fv(P097);
	glVertex3fv(P111);
	glVertex3fv(P121);
	glEnd();
	glBegin(cap);
	glVertex3fv(P002);
	glVertex3fv(P099);
	glVertex3fv(P097);
	glVertex3fv(P098);
	glEnd();
	glBegin(cap);
	glNormal3fv(N110);
	glVertex3fv(P110);
	glNormal3fv(N119);
	glVertex3fv(P119);
	glNormal3fv(N118);
	glVertex3fv(P118);
	glNormal3fv(N102);
	glVertex3fv(P102);
	glEnd();
	glBegin(cap);
	glNormal3fv(N119);
	glVertex3fv(P119);
	glNormal3fv(N066);
	glVertex3fv(P066);
	glNormal3fv(N067);
	glVertex3fv(P067);
	glNormal3fv(N118);
	glVertex3fv(P118);
	glEnd();
	glBegin(cap);
	glNormal3fv(N067);
	glVertex3fv(P067);
	glNormal3fv(N060);
	glVertex3fv(P060);
	glNormal3fv(N002);
	glVertex3fv(P002);
	glEnd();
	glBegin(cap);
	glNormal3fv(N067);
	glVertex3fv(P067);
	glNormal3fv(N002);
	glVertex3fv(P002);
	glNormal3fv(N118);
	glVertex3fv(P118);
	glEnd();
	glBegin(cap);
	glNormal3fv(N118);
	glVertex3fv(P118);
	glNormal3fv(N002);
	glVertex3fv(P002);
	glNormal3fv(N098);
	glVertex3fv(P098);
	glEnd();
	glBegin(cap);
	glNormal3fv(N118);
	glVertex3fv(P118);
	glNormal3fv(N098);
	glVertex3fv(P098);
	glNormal3fv(N102);
	glVertex3fv(P102);
	glEnd();
	glBegin(cap);
	glNormal3fv(N102);
	glVertex3fv(P102);
	glNormal3fv(N098);
	glVertex3fv(P098);
	glNormal3fv(N121);
	glVertex3fv(P121);
	glEnd();
}

static void
Dolphin015(GLenum cap)
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
	glNormal3fv(N064);
	glVertex3fv(P064);
	glNormal3fv(N120);
	glVertex3fv(P120);
	glEnd();
	glBegin(cap);
	glNormal3fv(N100);
	glVertex3fv(P100);
	glNormal3fv(N063);
	glVertex3fv(P063);
	glNormal3fv(N064);
	glVertex3fv(P064);
	glEnd();
	glBegin(cap);
	glNormal3fv(N063);
	glVertex3fv(P063);
	glNormal3fv(N006);
	glVertex3fv(P006);
	glNormal3fv(N064);
	glVertex3fv(P064);
	glEnd();
	glBegin(cap);
	glNormal3fv(N064);
	glVertex3fv(P064);
	glNormal3fv(N006);
	glVertex3fv(P006);
	glNormal3fv(N099);
	glVertex3fv(P099);
	glEnd();
	glBegin(cap);
	glNormal3fv(N064);
	glVertex3fv(P064);
	glNormal3fv(N099);
	glVertex3fv(P099);
	glNormal3fv(N117);
	glVertex3fv(P117);
	glEnd();
	glBegin(cap);
	glNormal3fv(N120);
	glVertex3fv(P120);
	glNormal3fv(N064);
	glVertex3fv(P064);
	glNormal3fv(N117);
	glVertex3fv(P117);
	glNormal3fv(N116);
	glVertex3fv(P116);
	glEnd();
	glBegin(cap);
	glNormal3fv(N006);
	glVertex3fv(P006);
	glNormal3fv(N065);
	glVertex3fv(P065);
	glNormal3fv(N099);
	glVertex3fv(P099);
	glEnd();
	glBegin(cap);
	glNormal3fv(N062);
	glVertex3fv(P062);
	glNormal3fv(N100);
	glVertex3fv(P100);
	glNormal3fv(N120);
	glVertex3fv(P120);
	glEnd();
	glBegin(cap);
	glNormal3fv(N005);
	glVertex3fv(P005);
	glNormal3fv(N062);
	glVertex3fv(P062);
	glNormal3fv(N120);
	glVertex3fv(P120);
	glEnd();
	glBegin(cap);
	glNormal3fv(N005);
	glVertex3fv(P005);
	glNormal3fv(N120);
	glVertex3fv(P120);
	glNormal3fv(N002);
	glVertex3fv(P002);
	glEnd();
	glBegin(cap);
	glNormal3fv(N002);
	glVertex3fv(P002);
	glNormal3fv(N120);
	glVertex3fv(P120);
	glNormal3fv(N116);
	glVertex3fv(P116);
	glEnd();
	glBegin(cap);
	glNormal3fv(N060);
	glVertex3fv(P060);
	glNormal3fv(N005);
	glVertex3fv(P005);
	glNormal3fv(N002);
	glVertex3fv(P002);
	glEnd();
}

static void
Dolphin016(GLenum cap)
{

	glDisable(GL_DEPTH_TEST);
	glBegin(cap);
	glVertex3fv(P123);
	glVertex3fv(P124);
	glVertex3fv(P125);
	glVertex3fv(P126);
	glVertex3fv(P127);
	glVertex3fv(P128);
	glEnd();
	glBegin(cap);
	glVertex3fv(P129);
	glVertex3fv(P130);
	glVertex3fv(P131);
	glVertex3fv(P132);
	glVertex3fv(P133);
	glVertex3fv(P134);
	glEnd();
	glBegin(cap);
	glVertex3fv(P103);
	glVertex3fv(P105);
	glVertex3fv(P108);
	glEnd();
	glEnable(GL_DEPTH_TEST);
}

void
DrawDolphin(fishRec * fish, int wire)
{
	float       seg0, seg1, seg2, seg3, seg4, seg5, seg6, seg7;
	float       pitch, thrash, chomp;
	GLenum      cap;

	fish->htail = (int) (fish->htail - (int) (10 * fish->v)) % 360;

	thrash = 70 * fish->v;

	seg0 = 1 * thrash * sin((fish->htail) * RRAD);
	seg3 = 1 * thrash * sin((fish->htail) * RRAD);
	seg1 = 2 * thrash * sin((fish->htail + 4) * RRAD);
	seg2 = 3 * thrash * sin((fish->htail + 6) * RRAD);
	seg4 = 4 * thrash * sin((fish->htail + 10) * RRAD);
	seg5 = 4.5 * thrash * sin((fish->htail + 15) * RRAD);
	seg6 = 5 * thrash * sin((fish->htail + 20) * RRAD);
	seg7 = 6 * thrash * sin((fish->htail + 30) * RRAD);

	pitch = fish->v * sin((fish->htail + 180) * RRAD);

/*	if (fish->v > 2) {
		chomp = -(fish->v - 2) * 200;
	}*/
	chomp = 100;

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

	P091[1] = iP091[1] + seg3;
	P092[1] = iP092[1] + seg3;
	P093[1] = iP093[1] + seg3;
	P094[1] = iP094[1] + seg3;
	P095[1] = iP095[1] + seg3;
	P122[1] = iP122[1] + seg3 * 1.5;

	P097[1] = iP097[1] + chomp;
	P098[1] = iP098[1] + chomp;
	P102[1] = iP102[1] + chomp;
	P110[1] = iP110[1] + chomp;
	P111[1] = iP111[1] + chomp;
	P121[1] = iP121[1] + chomp;
	P118[1] = iP118[1] + chomp;
	P119[1] = iP119[1] + chomp;

	glPushMatrix();

	glRotatef(pitch, 1, 0, 0);

	glTranslatef(0, 0, 7000);

	glRotatef(180, 0, 1, 0);

	glEnable(GL_CULL_FACE);
	cap = wire ? GL_LINE_LOOP : GL_POLYGON;
	Dolphin014(cap);
	Dolphin010(cap);
	Dolphin009(cap);
	Dolphin012(cap);
	Dolphin013(cap);
	Dolphin006(cap);
	Dolphin002(cap);
	Dolphin001(cap);
	Dolphin003(cap);
	Dolphin015(cap);
	Dolphin004(cap);
	Dolphin005(cap);
	Dolphin007(cap);
	Dolphin008(cap);
	Dolphin011(cap);
	Dolphin016(cap);
	glDisable(GL_CULL_FACE);

	glPopMatrix();
}
#endif
