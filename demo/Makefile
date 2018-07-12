OBJS = header.ips main.o65 system.o65 music.o65

all: demo.nes

demo.nes: $(OBJS)
	# Just for fun, create a 4-page ROM to demonstrate page swapping.
	neslink -s65536 -fnes -o $@ $^

# Generic rule to create .o65 out from .a65
%.o65: %.a65
	nescom -J -Wall -o $@ $<

# Generic rule to create .ips out from .a65
%.ips: %.a65
	nescom -J -I -Wall -o $@ $<

clean:
	rm -f *.ips *.o65 *~ demo.nes
