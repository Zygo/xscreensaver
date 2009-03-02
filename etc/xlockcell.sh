# converts a file from xlock life3d format to xl4d
sed '
s/-8,/ 8/g
s/-7,/ 9/g
s/-6,/10/g
s/-5,/11/g
s/-4,/12/g
s/-3,/13/g
s/-2,/14/g
s/-1,/15/g
s/-0,/16/g
s/0,/16/g
s/1,/17/g
s/2,/18/g
s/3,/19/g
s/4,/20/g
s/5,/21/g
s/6,/22/g
s/        //g
/^$/d
s/	//g' $1 > $1$$
fold -w 9 $1$$ > $1
sed "1,\$s/ $//g" $1 > $1$$
#rm -f $1$$
mv $1$$ $1
