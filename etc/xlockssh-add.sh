#!/bin/sh
#
# This is an example of a script to prompt for a passphrase and add
# several ssh(1) keys to a running ssh-agent(1).
#
ssh-add $HOME/.ssh/identity $HOME/.ssh/other_identity
