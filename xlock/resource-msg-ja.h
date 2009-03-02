/*
 * Resource messages for Japanese
 *   By: YOKOTA Hiroshi <yokota@netlab.is.tsukuba.ac.jp>
 *   Modified by: YAMAGUCHI Shingo <shingo-y@spacelan.ne.jp>
 */

#ifndef __RESOURCE_MSG_JA__
#define __RESOURCE_MSG_JA__

#define DEF_NAME		"ログイン名: "
#define DEF_PASS		"パスワード: "
#define DEF_VALID		"パスワード検査中..."
#define DEF_INVALID		"パスワードが違います。"
#define DEF_INVALIDCAPSLOCK     "パスワードが違います。CapsLock が ON です。"
#define DEF_INFO		"パスワードを入力して下さい。アイコンをクリックすると再ロックします。"

#ifdef HAVE_KRB5
#define DEF_KRBINFO		"Kerberos パスワードを入力して下さい。アイコンをクリックすると再ロックします。"
#endif /* HAVE_KRB5 */

#define DEF_COUNT_FAILED	" 回失敗"
#define DEF_COUNTS_FAILED	" 回失敗"

#define DEF_BTN_LABEL		"ログアウト"
#define DEF_BTN_HELP		"ここをクリックしてログアウト"
#define DEF_FAIL		"自動ログアウト失敗"

#endif /* __RESOURCE_MSG_JA__ */
