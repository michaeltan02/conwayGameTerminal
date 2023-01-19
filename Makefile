CC=gcc
CFLAGS=-I.

conwayGameBasic: conwayGameBasic.o
	${CC} -o conwayGameBasic conwayGameBasic.o -Wall -Wextra -pedantic


