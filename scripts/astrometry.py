#!/usr/bin/python

import pyfits

import sys
from optparse import OptionParser

def run_on_image(fn,verbose=False,blind=False):
	import rts2.astrometry
	import rts2.libnova

	a = rts2.astrometry.AstrometryScript(fn)

	ff=pyfits.fitsopen(fn,'readonly')

	ra = dec = scale = None

	ra=ff[0].header['CRVAL1']
	dec=ff[0].header['CRVAL2']

	for k in ['CDELT1','CD1_1']:
		try:
			scale=abs(float(ff[0].header[k]) * 3600.0)
			break
		except KeyError,ke:
			pass

	object = None
	num = None

	try:
		object=ff[0].header['OBJECT']
		num=ff[0].header['IMGID']
	except KeyError,ke:
		pass
			
	ff.close()

	ret=a.run(replace=True,verbose=verbose) if blind else a.run(scale=scale,ra=ra,dec=dec,replace=True,verbose=verbose)

	if ret:
		# script needs to reopen file 
		
		ff=pyfits.fitsopen(fn,'readonly')
		fh=ff[0].header
		ff.close()

		raorig=ra
		decorig=dec

		rastrxy = rts2.astrometry.xy2wcs(fh['NAXIS1']/2.0,fh['NAXIS2']/2.0,fh)

		#rastrxy=[float(ret[0])*15.0,float(ret[1])]

		err = rts2.libnova.angularSeparation(raorig,decorig,rastrxy[0],rastrxy[1])

		print "corrwerr 1 {0:.10f} {1:.10f} {2:.10f} {3:.10f} {4:.10f}".format(rastrxy[0], rastrxy[1], raorig-rastrxy[0], decorig-rastrxy[1], err)

		import rts2.scriptcomm
		c = rts2.scriptcomm.Rts2Comm()
		c.doubleValue('real_ra','[hours] image ra as calculated from astrometry',rastrxy[0])
		c.doubleValue('real_dec','[deg] image dec as calculated from astrometry',rastrxy[1])

		c.doubleValue('tra','[hours] telescope ra',raorig)
		c.doubleValue('tdec','[deg] telescope dec',decorig)

		c.doubleValue('ora','[arcdeg] offsets ra ac calculated from astrometry',raorig-rastrxy[0])
		c.doubleValue('odec','[arcdeg] offsets dec as calculated from astrometry',decorig-rastrxy[1])

		c.stringValue('object','astrometry object',object)
		c.integerValue('img_num','last astrometry number',num)


parser = OptionParser()
parser.add_option('--verbose',help='Be verbose, print what the script is doing',action='store_true',dest='verbose',default=False)
parser.add_option('--blind',help='Blind solve',action='store_true',dest='blind',default=False)

(options,args) = parser.parse_args()

if not(len(args) == 1):
	print 'Usage: {0} <fits filenames>'.format(sys.argv[0])
	sys.exit(1)

for x in args:
	run_on_image(x,verbose=options.verbose,blind=options.blind)
