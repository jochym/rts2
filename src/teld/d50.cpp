/* 
 * Driver for Ondrejov, Astrolab D50 scope.
 * Copyright (C) 2007 Petr Kubanek <petr@kubanek.net>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 */

#define DEBUG_EXTRA

#include "telescope.h"
#include "model/telmodel.h"

#include "../utils/rts2config.h"
#include "../utils/libnova_cpp.h"

#include <errno.h>
#include <fcntl.h>
#include <termios.h>
#include <unistd.h>

class Rts2DevTelD50:public Rts2DevTelescope
{
	private:
		char *device_name;
		int tel_desc;

		// utility comm I/O
		int tel_write (const char command);
		int tel_write (const char *command);
		// write to both units
		int write_both (const char *command);
		int tel_write (const char command, long value);

		int tel_write_unit (int unit, const char *command);
		int tel_write_unit (int unit, const char command, long value);

		int tel_read (const char command, Rts2ValueLong * value,
			Rts2ValueInteger * proc);
		int tel_read_unit (int unit, const char command, Rts2ValueLong * value,
			Rts2ValueInteger * proc);

		Rts2ValueBool *motorRa;
		Rts2ValueBool *motorDec;

		Rts2ValueBool *wormRa;
		Rts2ValueBool *wormDec;

		Rts2ValueLong *unitRa;
		Rts2ValueLong *unitDec;

		Rts2ValueInteger *procRa;
		Rts2ValueInteger *procDec;

	protected:
		virtual int processOption (int in_opt);
		virtual int isMoving ();
		virtual int isParking ();

		virtual void updateTrack ();

		virtual int setValue (Rts2Value * old_value, Rts2Value * new_value);
	public:
		Rts2DevTelD50 (int in_argc, char **in_argv);
		virtual ~ Rts2DevTelD50 (void);

		virtual int init ();

		virtual int info ();

		virtual int startMove (double tar_ra, double tar_dec);
		virtual int endMove ();
		virtual int stopMove ();

		virtual int startPark ();
		virtual int endPark ();

		virtual int correct (double cor_ra, double cor_dec, double real_ra,
			double real_dec);
};

int
Rts2DevTelD50::tel_write (const char command)
{
	int ret;
	#ifdef DEBUG_EXTRA
	logStream (MESSAGE_DEBUG) << "D50 will write " << command << sendLog;
	#endif
	ret = write (tel_desc, &command, 1);
	if (ret != 1)
	{
		logStream (MESSAGE_ERROR) << "Error during write " << errno << " " <<
			strerror (errno) << " ret " << ret << sendLog;
	}
	return 0;
}


int
Rts2DevTelD50::tel_write (const char *command)
{
	size_t ret;
	#ifdef DEBUG_EXTRA
	logStream (MESSAGE_DEBUG) << "D50 will write " << command << sendLog;
	#endif
	ret = write (tel_desc, command, strlen (command));
	if (ret != strlen (command))
	{
		logStream (MESSAGE_ERROR) << "Error during write " << errno << " " <<
			strerror (errno) << " ret " << ret << sendLog;
	}
	return 0;
}


int
Rts2DevTelD50::write_both (const char *command)
{
	int ret;
	ret = tel_write_unit (1, command);
	if (ret)
		return ret;
	ret = tel_write_unit (2, command);
	return ret;
}


int
Rts2DevTelD50::tel_write (const char command, long value)
{
	static char buf[50];
	sprintf (buf, "%c%li\x0d", command, value);
	return tel_write (buf);
}


int
Rts2DevTelD50::tel_write_unit (int unit, const char *command)
{
	int ret;
	// switch unit
	ret = tel_write ('x', unit);
	if (ret)
		return ret;
	return tel_write (command);
}


int
Rts2DevTelD50::tel_write_unit (int unit, const char command, long value)
{
	int ret;
	// switch unit
	ret = tel_write ('x', unit);
	if (ret)
		return ret;
	return tel_write (command, value);
}


int
Rts2DevTelD50::tel_read (const char command, Rts2ValueLong * value,
Rts2ValueInteger * proc)
{
	int ret;
	static char buf[15];
	ret = tel_write (command);
	if (ret < 0)
		return ret;
	ret = read (tel_desc, buf, 14);
	if (ret <= 0)
	{
		logStream (MESSAGE_ERROR) << "Error during read " << errno <<
			strerror (errno) << sendLog;
		return -1;
	}
	buf[ret] = '\0';
	if (ret < 2)
	{
		logStream (MESSAGE_ERROR) << "Error during read - too small " << ret <<
			"'" << buf << "'" << sendLog;
		return -1;
	}
	#ifdef DEBUG_EXTRA
	logStream (MESSAGE_DEBUG) << "Readed from D50 " << buf << sendLog;
	#endif
	long int vall;
	int ppro;
	ret = sscanf (buf, "#%li&%i\x0d\x0a", &vall, &ppro);
	if (ret != 2)
	{
		logStream (MESSAGE_ERROR) << "Wrong buffer " << buf << sendLog;
		return -1;
	}
	value->setValueLong (vall);
	proc->setValueInteger (ppro);
	return 0;
}


int
Rts2DevTelD50::tel_read_unit (int unit, const char command,
Rts2ValueLong * value, Rts2ValueInteger * proc)
{
	int ret;
	ret = tel_write ('x', unit);
	if (ret)
		return ret;
	return tel_read (command, value, proc);
}


Rts2DevTelD50::Rts2DevTelD50 (int in_argc, char **in_argv):Rts2DevTelescope (in_argc,
in_argv)
{
	tel_desc = -1;

	device_name = "/dev/ttyS0";
	addOption ('f', "device_name", 1, "device file (default /dev/ttyS0");

	createValue (motorRa, "ra_motor", "power of RA motor", false);
	motorRa->setValueBool (false);
	createValue (motorDec, "dec_motor", "power of DEC motor", false);
	motorDec->setValueBool (false);

	createValue (wormRa, "ra_worm", "RA worm drive", false);
	wormRa->setValueBool (true);
	createValue (wormDec, "dec_worm", "DEC worm drive", false);
	wormDec->setValueBool (false);

	createValue (unitRa, "axis_ra", "RA axis raw counts", false);
	createValue (unitDec, "axis_dec", "DEC axis raw counts", false);

	createValue (procRa, "proc_ra", "state for RA processor", false);
	createValue (procDec, "proc_dec", "state for DEC processor", false);

	// apply all correction for paramount
	corrections->
		setValueInteger (COR_ABERATION | COR_PRECESSION | COR_REFRACTION);
}


Rts2DevTelD50::~Rts2DevTelD50 (void)
{
	close (tel_desc);
}


int
Rts2DevTelD50::processOption (int in_opt)
{
	switch (in_opt)
	{
		case 'f':
			device_name = optarg;
			break;
		default:
			return Rts2DevTelescope::processOption (in_opt);
	}
	return 0;
}


int
Rts2DevTelD50::init ()
{
	struct termios tel_termios;
	int ret;

	ret = Rts2DevTelescope::init ();
	if (ret)
		return ret;

	Rts2Config *config = Rts2Config::instance ();
	ret = config->loadFile ();
	if (ret)
		return -1;

	telLongtitude->setValueDouble (config->getObserver ()->lng);
	telLatitude->setValueDouble (config->getObserver ()->lat);

								 // south hemispehere
	if (telLatitude->getValueDouble () < 0)
	{
		// swap values which are opposite for south hemispehere
	}

	tel_desc = open (device_name, O_RDWR);
	if (tel_desc < 0)
		return -1;

	if (tcgetattr (tel_desc, &tel_termios) < 0)
		return -1;

	if (cfsetospeed (&tel_termios, B9600) < 0 ||
		cfsetispeed (&tel_termios, B9600) < 0)
		return -1;

	tel_termios.c_iflag = IGNBRK & ~(IXON | IXOFF | IXANY);
	tel_termios.c_oflag = 0;
	tel_termios.c_cflag =
		((tel_termios.c_cflag & ~(CSIZE)) | CS8) & ~(PARENB | PARODD);
	tel_termios.c_lflag = 0;
	tel_termios.c_cc[VMIN] = 0;
	tel_termios.c_cc[VTIME] = 40;

	if (tcsetattr (tel_desc, TCSANOW, &tel_termios) < 0)
		return -1;

	tcflush (tel_desc, TCIOFLUSH);

	snprintf (telType, 64, "D50");

	// switch both motors off
	ret = write_both ("D\x0d");

	return ret;
}


void
Rts2DevTelD50::updateTrack ()
{
}


int
Rts2DevTelD50::setValue (Rts2Value * old_value, Rts2Value * new_value)
{
	if (old_value == motorRa)
	{
		return tel_write_unit (1,
			((Rts2ValueBool *) new_value)->
			getValueBool ()? "E\x0d" : "D\x0d") ==
			0 ? 0 : -2;
	}
	if (old_value == motorDec)
	{
		return tel_write_unit (2,
			((Rts2ValueBool *) new_value)->
			getValueBool ()? "E\x0d" : "D\x0d") ==
			0 ? 0 : -2;
	}
	if (old_value == wormRa)
	{
		return tel_write_unit (1,
			((Rts2ValueBool *) new_value)->
			getValueBool ()? "o0" : "c0") == 0 ? 0 : -2;

	}
	if (old_value == wormDec)
	{
		return tel_write_unit (2,
			((Rts2ValueBool *) new_value)->
			getValueBool ()? "o0" : "c0") == 0 ? 0 : -2;

	}
	if (old_value == unitRa)
	{
		return tel_write_unit (1, 't',
			new_value->getValueLong ()) == 0 ? 0 : -2;
	}
	if (old_value == unitDec)
	{
		return tel_write_unit (2, 't',
			new_value->getValueLong ()) == 0 ? 0 : -2;
	}
	return Rts2DevTelescope::setValue (old_value, new_value);
}


int
Rts2DevTelD50::info ()
{
	int ret;
	ret = tel_read_unit (1, 'u', unitRa, procRa);
	if (ret)
		return ret;
	ret = tel_read_unit (2, 'u', unitDec, procDec);
	if (ret)
		return ret;

	return Rts2DevTelescope::info ();
}


int
Rts2DevTelD50::startMove (double tar_ra, double tar_dec)
{
	return 0;
}


int
Rts2DevTelD50::isMoving ()
{
	// we reached destination
	return -2;
}


int
Rts2DevTelD50::endMove ()
{
	return Rts2DevTelescope::endMove ();
}


int
Rts2DevTelD50::stopMove ()
{
	return Rts2DevTelescope::stopMove ();
}


int
Rts2DevTelD50::startPark ()
{
	return Rts2DevTelescope::startPark ();
}


int
Rts2DevTelD50::isParking ()
{
	return -2;
}


int
Rts2DevTelD50::endPark ()
{
	return Rts2DevTelescope::endPark ();
}


int
Rts2DevTelD50::correct (double cor_ra, double cor_dec, double real_ra,
double real_dec)
{
	return correctOffsets (cor_ra, cor_dec, real_ra, real_dec);
}


int
main (int argc, char **argv)
{
	Rts2DevTelD50 device = Rts2DevTelD50 (argc, argv);
	return device.run ();
}
