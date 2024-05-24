CC=g++ 
COPTS= -Wall -Werror -ggdb 
LIBS=  -lSoundTouch  --static 
#LIBS=   

all: lib

#	$(CC) -DMAIN -o test arc_prompt_control.cc  $(LIBS)

object:
	$(CC) $(COPTS) -c arc_prompt_control.cc \
			-I./soundtouch/include 


lib: object
	ar -cru libarcpromptcontrol.a *.o 

clean:
	rm *.o ; rm libarcpromptcontrol.a

test:
	$(CC) $(COPTS) -D MAIN -o test arc_prompt_control.cc \
			-I./soundtouch/include  ./soundtouch/lib/libSoundTouch.a  

