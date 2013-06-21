
all: AdlinkIODS.pdf

clean: dia-clean
	rm -f *.pdf *.log *.backup *.dvi *~ *.aux *.toc

AdlinkIODS.pdf: AdlinkIODS.latex dia-make
	pdflatex AdlinkIODS.latex

html: AdlinkIODS.latex dia-png
	latex2html -split 0 -info 0 -no-navigation AdlinkIODS.latex 

dia-png:
	cd dia && make png

dia-make:
	cd dia && make

dia-clean:
	cd dia && make clean
