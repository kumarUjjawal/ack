1i\
 /*  ops is converted into a test program for longs \
*/
/LSTART/d
s/FN/main/
/ISTART/c\
	unsigned x=40234 , y= 301 ;
/[XYZS]/s/.*/& ; printf("%s#","&"@) ;/
/Z1/s/#/ %d&/
/Z1/s/@/, Z1&/
/Z2/s/#/ %d&/
/Z2/s/@/, Z2&/
/Z1/s/^/Z1 = 10 ; /
/Z2/s/^/Z2 = 0100 ; /
/[XYZS]/s/^/	/
s/X/40000/g
s/Y/8012/g
s/S/9/g
s/Z1/x/g
s/Z2/y/g
s/#/\\n/
s/@//
