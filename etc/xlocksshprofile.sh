#
# This is an example of what an ssh-agent(1) user might add to their
# .profile to get a single ssh-agent process 
SSH_AUTH_SOCK=$HOME/.ssh/agentsocket
export SSH_AUTH_SOCK
if [ \! -r "$SSH_AUTH_SOCK" ]
then
	ssh-agent -a $SSH_AUTH_SOCK
	/usr/local/bin/xlockssh-add.sh
fi
