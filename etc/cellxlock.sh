# converts a file from xl4d format to xlock life3d format
sed '
s/10/-6,/g
s/11/-5,/g
s/12/-4,/g
s/13/-3,/g
s/14/-2,/g
s/15/-1,/g
s/16/0,/g
s/17/1,/g
s/18/2,/g
s/19/3,/g
s/^/		/' $1 > $1$$
#rm -f $1$$
mv $1$$ $1
