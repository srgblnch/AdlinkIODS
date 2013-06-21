
DIAS=acquisition.pdf classes2.pdf classes.pdf classes3.pdf classes4.pdf
PNGS=acquisition.png classes2.png classes.png classes3.png classes4.png

all : pdf 

png: $(PNGS)

pdf: $(DIAS)

#
# We generate both pdf and eps versions
# pdflatex does not understand eps and latex does not understand pdf
# This way we build both versions, so kile can make fast dvi preview (using eps)
# and pdf version is built OK too.
#
# In order for this to work, do not put extension in \includegraphics
#
%.pdf: %.dia 
	dia $? --export=$*.eps
	epstopdf $*.eps

%.png: %.dia
	dia $? --export=$*.png

clean:
	rm -f $(DIAS) *~ *.eps $(PNGS)


