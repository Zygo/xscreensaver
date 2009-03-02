#!/usr/bin/perl -w
#
# This script is intended to be invoked via the -pipepassCmd option
# of xlock(1), to add private ssh(1) keys to a running ssh-agent(1)
# when the screen is unlocked without the user having to enter the
# passphrase.
# 
# Only keys who's passphrase is the same as the user's password can
# be automatically re-added in this way.
#
use strict;

use Expect;

my $pass = <STDIN>;
my $exp = Expect->spawn('/usr/local/bin/xlockssh-add.sh');
$exp->expect(10, ':');
$exp->send("$pass\r\n");
$exp->expect(10, ':');
$exp->hard_close;
