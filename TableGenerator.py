#!/usr/bin/python

import os
import sys

## ARGUMENTS
# number of lines
# type first signal
# period in samples
# amplitude
# type second signal
# period in samples
# amplitude 
# ...

# for arg in sys.argv: line += ' ' + arg

print 'Table generator running ...'

if len(sys.argv)>1:
    origin=sys.argv[1]
    print 'Filename is ' + origin
else:
    print 'No file'
    sys.exit(0)

file = open(origin,'w')
if not file:
    print 'impossible to open file'
    sys.exit(0)
    
samples = 4096
print 'Preparing ',samples,' samples'
line = " "
for i in range(samples):
    line = str(i)
    
    amplitude = 2.5
    period = 4096
    ramp = (i%period)*2*amplitude/period-amplitude
    ramp = ramp +5
    #ramp = (i>2047 and i<2048+512) and 3 or ramp
    #ramp = (i>2048+512 and i<2048+1024) and -3 or ramp
    #ramp = i>=3072 and 1 or ramp
    line = line + '\t' + str(ramp)
    
    amplitude = 2.5
    ramp = i*2*amplitude/samples-amplitude-5
    line = line + '\t' + str(ramp)

    amplitude = 8.0
    ramp = i*2*amplitude/samples-amplitude
    line = line + '\t' + str(ramp)

    amplitude = 7.0
    ramp = i*2*amplitude/samples-amplitude
    line = line + '\t' + str(ramp)
    
    amplitude = 6.0
    ramp = i*2*amplitude/samples-amplitude
    line = line + '\t' + str(ramp)

    amplitude = 5.0
    ramp = i*2*amplitude/samples-amplitude
    line = line + '\t' + str(ramp)

    amplitude = 4.0
    ramp = i*2*amplitude/samples-amplitude
    line = line + '\t' + str(ramp)

    amplitude = 3.0
    ramp = i*2*amplitude/samples-amplitude
    line = line + '\t' + str(ramp)
    
    line = line + '\n'
    file.write(line)

file.close()
print 'Done'
sys.exit(0)

#for dir, sdirs, filenames in os.walk('.'):
#    print dir, ': ',len(filenames),' files'


    #filename=filename[:filename.lower().find('.bat')]
    #batch = file.readlines()
            # batch = orig[copystart:copyend+1]+batch
        # batch.append(':SCRIPT_END\n')
