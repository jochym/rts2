Introduction
============

Status and open issues (2014-04-19)
-----------------------------------
This description is not yet meant to be complete. Comments and corrections are very welcome.

The documentation describes 

1) the rts2saf installation and its integration into ``RTS2``
2) usage and testing with dummy devices
3) offline analysis of previously acquired data

Items which need further attention:

1) target selection for focus run: the focus run is carried out at the current
   telescope position, e.g. goto nearest Landolt target
2) finding the appropriate exposure 
3) further, e.g. faster methods to determine the FWHM minimum: currently about 6...8 images are taken see e.g. Petr's script ``focsing.py``
4) strategies if a focus run fails, e.g. widen the interval in regular mode, fall back to blind mode
5) ``SExtractor``'s filter  option
6) replacemenmt of ``matplotlib`` as plotting engine (it doesn't work well in the background and within threads)
7) many ToDos in the code

.. _sec_introduction-label:

8) RTS2 EXEC does not continue to select targets after an external script, like ``rts2saf_focus.py``, has finished. EXEC continues if a script ``exe /path/script E 1`` contains an additional exposure command at the end.



In case of questions, or if you need support, contact the author and
in case of failures execute

.. code-block:: bash

 ./rts2saf_unittest.py

and send its output together with the contents ``/tmp/rts2saf_log`` (best as ``tar.gz`` archive).


Quick hands on analysis
-----------------------

After  :ref:`installing all helper programs, Python packages <sec_installation-label>`  and the data files:

.. code-block:: bash

 wget http://azug.minpet.unibas.ch/~wildi/rts2saf-test-focus-2013-09-14.tgz
 tar zxvf rts2saf-test-focus-2013-09-14.tgz
 wget http://azug.minpet.unibas.ch/~wildi/rts2saf-test-focus-2013-11-10.tgz
 tar zxvf rts2saf-test-focus-2013-11-10.tgz

 wget http://azug.minpet.unibas.ch/~wildi/20131011054939-621-RA.fits

execute in the main rts2saf directory either

.. code-block:: bash

 ./rts2saf_hands_on_examples.sh

or some of these commands

.. program-output:: grep -vE "(\#|DONE|tail)"  ../rts2saf_hands_on_examples.sh 


Overview
--------
This is the description of the rts2saf auto focuser package.
The latest version is available at the RTS2 svn repo:
http://sourceforge.net/p/rts-2/code/HEAD/tree/trunk/rts-2/scripts/rts2saf/
including this description.


rts2saf is a complete rewrite of rts2af.  The goals were

0) a comprehensive command line user interface including sensible log messages,
1) simpler installation and configuration, 
2) a general solution for all RTS2 driven observatories,
3) the support of multiple filter wheels with an arbitrary number of slots,  
4) a modular software design which eases testing of its components.

rts2saf's main tasks are to determine the focus and set ``FOC_DEF``
during autonomous operations whenever the FWHM of an image exceeds 
a threshold.
Depending on the actual configuration it measures filter focus offsets 
relative to an empty or a predefined slot and writes these values
to the CCD driver.
In addition it provides a tool to analyze previous focus runs offline 
e.g. in order to create a temperature model.

The method to find the focus is straight forward. Acquire a set of images 
at different focuser positions  and  determine FWHM and optionally source's 
maximum flux using ``SExtractor``. In the simplest case the position of 
the minimum FWHM, derived from the fitted function, is the focus.

The output of the fit program is stored as a PNG file and optionally displayed on screen. 
In addition various weighted means are calculated which are currently only logged.


rts2saf makes use of RTS2's HTTP/JSON interface and hence using the scripts  
on the command line is encouraged before setting up autonomous operations. The JSON interface 
eases and speeds up the test phase considerably specially in the early stage
of debugging the configuration. 

Test runs can be carried out during day time either with RTS2
dummy or real devices. If no real images can be acquired,  
"dry fits files" are injected while optionally all involved 
devices operate as if it were night. These files can be images from 
a former focus run or if not available samples are provided by the 
author (see below).

Parameters, like e.g. ``FOC_DEF`` stored in focuser device, are retrieved 
from the running RTS2 instance as far as they are needed. All additional 
device or analysis properties are kept in a single configuration file. 

Operations modes
++++++++++++++++
1) **autonomous operations**:
   ``rts2saf_imgp.py``, ``rts2saf_fwhm.py``, ``rts2saf_focus.py``
2) **command line execution**:
   ``rts2saf_focus.py``
3) **offline analysis**:
   ``rts2saf_analysis.py``

Focus runs come in two flavors:

1) 'regular'
2) 'blind'

Regular runs can be carried out either in autonomous mode or on the
command line while blind runs are typically executed only on the
command line.

Regular runs in autonomous mode are optimized for minimum elapsed time
and typically involve only the wheel's empty slot.


Autonomous operations
~~~~~~~~~~~~~~~~~~~~~
Once an image has been stored on disk RTS2 calls ``rts2saf_imgp.py``
which covers two tasks:

1) measurement of FWHM using ``SExtractor``
2) astrometric calibration using ``astrometry.net``

If the measured FWHM is above a configurable threshold ``rts2saf_fwhm.py``
triggers an on target focus run using selector's focus queue. This 
target is soon executed and ``rts2saf_focus.py`` acquires a configurable set  
of images at different focuser positions. rts2saf then fits these points and 
the extremes are derived  from the fitted functions. If successful it sets 
focuser's ``FOC_DEF`` if variable ``SET_FOC_DEF`` is set to ``True`` in the 
configuration file.

Command line execution
~~~~~~~~~~~~~~~~~~~~~~
In order to simplify the debugging of one's own configuration 
all scripts can be used directly on the command line either
with or without previously acquired images.

Analysis modes
++++++++++++++

``SExtractor`` provides FWHM and maximum flux per analyzed object. Using
defaults only FWHM is fitted.
Optionally an independent fit to the sum of the flux is available. Comparing 
fluxes among images makes only sense in case the sextracted objects are 
identified on all images. The association is carried out by ``SExtractor``.

To increase the chance that the fit converges errors for FWHM and flux are introduced.
In case of FWHM the error is the standard deviation of the FWHM distribution, while for 
flux the average of the square roots of the values is used.

Optionally ``DS9`` displays images and their region files on screen. 
The circle is 
centered to ``SExtractor``'s x,y positions. Red circles indicate objects
which were rejected, green ones which were accepted as star like and in case 
``SExtractor`` associates the objects among images yellow indicates star
like objects which are not on all images and therfore rejected.

If rts2saf is executed interactively on a remote host the X-Window DISPLAY 
variable has to be set otherwise neither the fit nor images are displayed. 
