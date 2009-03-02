#
# gettext like message catalogue system
#
# By: YOKOTA Hiroshi <yokota@netlab.is.tsukuba.ac.jp>
#
# $Id: gettext.tcl,v 1.3 2000/07/30 04:44:24 elca Exp $

#
# How to use
#
# 1) Create catalogue file named "language.<$LANG>.tcl".
#    If your $LANG environment valuable is "ja_JP.eucJP", you make
#    "language.ja_JP.eucJP.tcl" or "language.ja_JP.tcl" or "language.ja.tcl".
#    Here is some example for English-Japanese translation.
#
#    -------
#    settext "hello" "こんにちは"
#    settext "OK"    "了解"
#    ...
#    ------
#
# 2) Setup auto-load path. You must make file named "tclIndex" before
#    use this system. See "auto_mkindex" Tcl command help.
#
# 3) Add these lines at top of your tcl script file.
#
#    -----
#    set auto_path [linsert $auto_path end .]  ;# All script files in "."
#    set_catalogue [set_language]
#    -----
#
# 4) Use "gettext" command for message transration.
#    You can also use "_" command for easy use.
#
#    -----
#    gettext "hello"   ;# -> "こんにちは"
#    _ "OK"            ;# -> "了解"
#    -----
#

#####################################################################
# Internal valueable. Don't use it.
set g_language "C"
set g_resource("__dummy__") ""

######################################################################
# Global functions
######################################################################

#
# Select language catarogue file
#
# If catalogue file is found, return language string.
# If not found catalogue file, return "C".
#
proc set_language {{lang ""}} {
    global g_language env

    # If some argument is given, force to set language.
    if {$lang != ""} then {
	set g_language $lang
	return $g_language
    }

    if {![info exists env(LANG)]} {
	set g_language "C"
	return "C"
    }

    foreach f "$env(LANG) [lang_substr1 $env(LANG)] [lang_substr2 $env(LANG)]" {
	if {$f == ""} {continue}
	if {[file exist language.$f.tcl]} {
	    set g_language $f
	}
    }

    return $g_language
}

#
# Read catalogue file named "language.$lang.tcl".
#
proc set_catalogue {lang} {
    if {$lang == "C"} {return}

    if {[file exist language.$lang.tcl]} {
	source language.$lang.tcl
    }
}

#
# Get translated string
#
proc gettext {string} {
    global g_resource

    if {[info exist g_resource($string)]} {
	return $g_resource($string)
    } else {
	return $string
    }
}

#
# Set translation catalogue string.
#
proc settext {from to} {
    global g_resource

    set g_resource($from) $to
}

#
# For easy use
#
proc _ {string} {
    return [gettext $string]
}


########################################################################
# Internal functions. Don't use it.
########################################################################

#
# "ja_JP.eucJP" -> "ja_JP"
# "ja_JP"       -> "ja_JP"
#
proc lang_substr1 {str} {
    if {[regexp -nocase {^([a-z]+_[a-z]+)\.[a-z]+$} $str a b]} {
	return $b
    } elseif {[regexp -nocase {^([a-z]+_[a-z]+)$} $str a b]} {
	return $b
    } else {
	return ""
    }
}

#
# "ja_JP.eucJP" -> "ja"
# "ja_JP"       -> "ja"
# "ja"          -> "ja"
#
proc lang_substr2 {str} {
    if {[regexp -nocase {^([a-z]+)_[a-z]+\.[a-z]+$} $str a b]} {
	return $b
    } elseif {[regexp -nocase {^([a-z]+)_[a-z]+$} $str a b]} {
	return $b
    } elseif {[regexp -nocase {^([a-z]+)$} $str a b]} {
	return $b
    } else {
	return ""
    }
}

# end
