
###############################################################################
####  Actual Ramp Generation:
## Returns an array of ramps, all with same number of samples. Ramp X
## is based in applying channels[X](sample): channels is a list
## of functions that define each channel.
###############################################################################
def build(channels, nsamples):
	nchannels = len(channels)
	r = []
	
	for fn in channels:
		r.append([fn(float(x)) for x in xrange(nsamples)])
	return r


###############################################################################
####  Save Ramp in file:
## Files format:
##   ch0S0 \t ch1S0 \t ... \t chNS0 \n
##                ... \n
##   ch0SM \t ch1SM \t ... \t chNSM \n
##   N=ncnannels, M=nsamples
###############################################################################
def save(destfile, ramp):
	lines = []
	lineX = []
	nchannels = len(ramp)
	nsamples = len(ramp[0])
	
	for n in xrange(nsamples):
		lineX = []
		for ch in xrange(nchannels):
			lineX.append(str(ramp[ch][n]))
		lines.append('\t'.join(lineX))
	s = '\n'.join(lines)
	
	f = file(destfile, 'w')
	f.write(s)
	f.close()

def build_file(destfile, channels, nsamples):
	save(destfile, build(channels, nsamples))

###############################################################################
####  Example usage:
###############################################################################
if __name__ == "__main__":
	from math import *
	destfile = 'caca.ramp'
	nsamples = 1024
	channels = [
		lambda x: 4*sin(2*pi*x/nsamples),
		lambda x: 4*cos(2*pi*x/nsamples),
		lambda x: 4*sin(2*pi*x/nsamples) - 2*cos(2*pi*x/nsamples),
		lambda x: 5*x/nsamples,
	]
	build_file(destfile, channels, nsamples)
