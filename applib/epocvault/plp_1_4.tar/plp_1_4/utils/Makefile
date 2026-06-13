CCSOURCE=$(wildcard *.cc)
CSOURCE= $(wildcard *.c)

OBJ=$(CCSOURCE:%.cc=%.o) $(CSOURCE:%.c=%.o)

libutils.a : $(OBJ)
	$(AR) $@ $(OBJ)

%.o : %.cc
	$(CC) -c $< -o $@

%.o : %.c
	$(CC) -c $< -o $@

clean :
	rm -f *.a core $(OBJ) *~