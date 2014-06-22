ycbcr_lutloader: ycbcr_lutloader.c
	gcc -std=c99 -Wall -g -o $@ $< -lm -llcms2
