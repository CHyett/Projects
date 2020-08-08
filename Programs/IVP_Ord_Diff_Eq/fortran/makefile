
.PHONY: clean

main.exe: 
	gfortran -fdefault-real-8 -fdefault-double-8 -c euler.F90
	gfortran -fdefault-real-8 -fdefault-double-8 -c main.F90
	gfortran -fdefault-real-8 -fdefault-double-8 euler.o main.o -o main.exe
	./main.exe

%.o : %.F90
	gfortran -c $< 
	
	
clean:
	rm -f euler.mod euler.o output_*.dat main.o main.exe