CCSOURCE=$(wildcard *.cc)
CSOURCE= $(wildcard *.c)

OBJ=$(CCSOURCE:%.cc=%.o) $(CSOURCE:%.c=%.o)

../bin/ncp : $(OBJ)
	$(LD) $(OBJ) $(LIBS) -o $@

%.o : %.cc
	$(CC) -c $< -o $@

%.o : %.c
	$(CC) -c $< -o $@

clean :
	rm -f ../bin/ncp core $(OBJ) *~