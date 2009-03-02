/* Japanese messages are separate to another file.             */
/* Because Japanese EUC-JP encoding is conflict with ISO-8859. */
/* By: YOKOTA Hiroshi <yokota@netlab.is.tsukuba.ac.jp>         */

#ifndef __DCLOCK_MSG_JP_H__
#define __DCLOCK_MSG_JP_H__

#define POPEX_STRING      "現在の世界人口"
#define PEOPLE_STRING     " 人"
#define FOREST_STRING     "現在の熱帯雨林の面積"
#define TROPICAL_STRING   " "
#define HIV_STRING        "現在の HIV 感染数"
#define CASES_STRING      " 件"
#define LAB_STRING        "今年、動物実験により"
#define VEG_STRING        "今年、人間の食糧として"
#define YEAR_STRING       " 体の動物が犠牲になりました"
#define Y2K_STRING        "Y2K (2000年1月1日, 0時00分00秒) まで あと"
#define POST_Y2K_STRING   "Y2K (2000年1月1日, 0時00分00秒) から"
#define Y2001_STRING      "二千年紀の終了 (2001年1月1日, 0時00分00秒) まで あと"
#define POST_Y2001_STRING "三千年紀の開始 (2001年1月1日, 0時00分00秒) から"
#define DAY               "日"
#define DAYS              "日"
#define HOUR              "時間"
#define HOURS             "時間"
#define MINUTE            "分"
#define MINUTES           "分"
#define SECOND            "秒"
#define SECONDS           "秒"

#ifndef METRIC
#define METRIC 1
#endif

#if METRIC
#define AREA_STRING "ヘクタール"
#else
#define AREA_STRING "エーカー"
#endif

#endif
