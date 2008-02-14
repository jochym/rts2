/* 
 * Driver for OpemTPL mounts.
 * Copyright (C) 2005-2007 Petr Kubanek <petr@kubanek.net>
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

#include "ir.h"

#define BLIND_SIZE            1.0
#define OPT_ROTATOR_OFFSET    OPT_LOCAL + 1

class Rts2DevTelescopeIr:public Rts2TelescopeIr
{
	private:
		struct ln_equ_posn target;
		int irTracking;

		int startMoveReal (double ra, double dec);
	protected:
		virtual int processOption (int in_opt);
	public:
		Rts2DevTelescopeIr (int in_arcg, char **in_argv);
		virtual int startMove ();
		virtual int isMoving ();
		virtual int stopMove ();

		virtual int stopWorm ();
		virtual int startWorm ();

		virtual int startPark ();
		int moveCheck (bool park);
		virtual int isParking ();
		virtual int endPark ();

		virtual int changeMasterState (int new_state);
};

int
Rts2DevTelescopeIr::processOption (int in_opt)
{
	switch (in_opt)
	{
		case 't':
			irTracking = atoi (optarg);
			break;
		case OPT_ROTATOR_OFFSET:
			derotatorOffset->setValueDouble (atof (optarg));
			break;
		default:
			return Rts2TelescopeIr::processOption (in_opt);
	}
	return 0;
}


int
Rts2DevTelescopeIr::startMoveReal (double ra, double dec)
{
	int status;
	//  status = setTrack (0);
	status = tpl_set ("POINTING.TARGET.RA", ra / 15.0, &status);
	status = tpl_set ("POINTING.TARGET.DEC", dec, &status);
	if (!getDerotatorPower ())
	{
		status = tpl_set ("DEROTATOR[3].POWER", 1, &status);
		status = tpl_set ("CABINET.POWER", 1, &status);
	}

	#ifdef DEBUG_EXTRA
	logStream (MESSAGE_DEBUG) << "IR startMove TRACK status " << status <<
		sendLog;
	#endif
	status = tpl_set ("DEROTATOR[3].OFFSET", -1 * derotatorOffset->getValueDouble () , &status);

	if (status != TPL_OK)
		return status;

	//  usleep (USEC_SEC);
	status = setTrack (irTracking, domeAutotrack->getValueBool ());
	usleep (USEC_SEC);
	return status;
}


int
Rts2DevTelescopeIr::startMove ()
{
	int status = 0;
	double sep;

	getTarget (&target);

	// move to zenit - move to different dec instead
	if (fabs (target.dec - telLatitude->getValueDouble ()) <= BLIND_SIZE)
	{
		if (fabs (target.ra / 15.0 - getLocSidTime ()) <= BLIND_SIZE / 15.0)
		{
			target.dec = telLatitude->getValueDouble () - BLIND_SIZE;
		}
	}

	double az_off = 0;
	double alt_off = 0;
	status = tpl_set ("AZ.OFFSET", az_off, &status);
	status = tpl_set ("ZD.OFFSET", alt_off, &status);
	if (status)
	{
		logStream (MESSAGE_ERROR) << "IR startMove cannot zero offset" << sendLog;
		return -1;
	}

	status = startMoveReal (target.ra, target.dec);
	if (status)
		return -1;

	// wait till we get it processed
	status = tpl_get ("POINTING.TARGETDISTANCE", sep, &status);
	if (status)
	{
		logStream (MESSAGE_ERROR) << "cannot get target separation" << sendLog;
		return -1;
	}
	if (sep > 2)
		sleep (3);
	else if (sep > 2 / 60.0)
		usleep (USEC_SEC / 10);
	else
		usleep (USEC_SEC / 10);

	time (&timeout);
	timeout += 300;
	return 0;
}


int
Rts2DevTelescopeIr::isMoving ()
{
	return moveCheck (false);
}


int
Rts2DevTelescopeIr::stopMove ()
{
	int status = 0;
	double zd;
	Rts2DevTelescope::stopMove ();
	info ();
	// ZD check..
	status = tpl_get ("ZD.CURRPOS", zd, &status);
	if (status)
	{
		logStream (MESSAGE_DEBUG) << "IR stopMove cannot get ZD! (" << status <<
			")" << sendLog;
		return -1;
	}
	if (fabs (zd) < 1)
	{
		logStream (MESSAGE_DEBUG) << "IR stopMove suspicious ZD.. " << zd <<
			sendLog;
		status = setTrack (0);
		if (status)
		{
			logStream (MESSAGE_DEBUG) << "IR stopMove cannot set track: " <<
				status << sendLog;
			return -1;
		}
		return 0;
	}
	return 0;
}


int
Rts2DevTelescopeIr::startPark ()
{
	int status = TPL_OK;
	// Park to south+zenith
	status = setTrack (0);
	#ifdef DEBUG_EXTRA
	logStream (MESSAGE_DEBUG) << "IR startPark tracking status " << status <<
		sendLog;
	#endif
	sleep (1);
	status = TPL_OK;
	status = tpl_set ("AZ.TARGETPOS", 0, &status);
	status = tpl_set ("ZD.TARGETPOS", 0, &status);
	status = tpl_set ("DEROTATOR[3].POWER", 0, &status);
	if (status)
	{
		logStream (MESSAGE_ERROR) << "IR startPark ZD.TARGETPOS status " <<
			status << sendLog;
		return -1;
	}
	time (&timeout);
	timeout += 300;
	return 0;
}


int
Rts2DevTelescopeIr::moveCheck (bool park)
{
	int status = TPL_OK;
	int track;
	double poin_dist;
	time_t now;
	struct ln_equ_posn tPos;
	struct ln_equ_posn cPos;
	//  status = tpl_get ("POINTING.TARGETDISTANCE", poin_dist, &status);
	status = tpl_get ("POINTING.TARGET.RA", tPos.ra, &status);
	status = tpl_get ("POINTING.TARGET.DEC", tPos.dec, &status);
	status = tpl_get ("POINTING.CURRENT.RA", cPos.ra, &status);
	status = tpl_get ("POINTING.CURRENT.DEC", cPos.dec, &status);
	if (status != TPL_OK)
		return -1;
	poin_dist = ln_get_angular_separation (&cPos, &tPos);
	time (&now);
	// get track..
	status = tpl_get ("POINTING.TRACK", track, &status);
	if (track == 0 && !park)
	{
		logStream (MESSAGE_WARNING) <<
			"Tracking sudently stopped, reenable tracking" << sendLog;
		setTrack (irTracking, domeAutotrack->getValueBool ());
		sleep (1);
		return USEC_SEC / 100;
	}
	// 0.01 = 36 arcsec
	if (fabs (poin_dist) <= 0.01)
	{
		#ifdef DEBUG_EXTRA
		logStream (MESSAGE_DEBUG) << "IR isMoving target distance " << poin_dist
			<< sendLog;
		#endif
		return -2;
	}
	// finish due to timeout
	if (timeout < now)
	{
		logStream (MESSAGE_ERROR) << "IR isMoving target distance in timeout "
			<< poin_dist << "(" << status << ")" << sendLog;
		return -1;
	}
	return USEC_SEC / 100;
}


int
Rts2DevTelescopeIr::isParking ()
{
	return moveCheck (true);
}


int
Rts2DevTelescopeIr::endPark ()
{
	#ifdef DEBUG_EXTRA
	logStream (MESSAGE_DEBUG) << "IR endPark" << sendLog;
	#endif
	return 0;
}


Rts2DevTelescopeIr::Rts2DevTelescopeIr (int in_argc, char **in_argv):Rts2TelescopeIr (in_argc,
in_argv)
{
	irTracking = 4;

	addOption (OPT_ROTATOR_OFFSET, "rotator_offset", 1, "rotator offset, default to 0");
	addOption ('t', "ir_tracking", 1,
		"IR tracking (1, 2, 3 or 4 - read OpenTCI doc; default 4");
}


int
Rts2DevTelescopeIr::startWorm ()
{
	int status = TPL_OK;
	status = setTrack (irTracking, domeAutotrack->getValueBool ());
	if (status != TPL_OK)
		return -1;
	return 0;
}


int
Rts2DevTelescopeIr::stopWorm ()
{
	int status = TPL_OK;
	status = setTrack (0);
	if (status)
		return -1;
	return 0;
}


int
Rts2DevTelescopeIr::changeMasterState (int new_state)
{
	switch (new_state & (SERVERD_STATUS_MASK | SERVERD_STANDBY_MASK))
	{
		case SERVERD_DUSK:
		case SERVERD_NIGHT:
		case SERVERD_DAWN:
			coverOpen ();
			domeOpen ();
			break;
		default:
			coverClose ();
			domeClose ();
			break;
	}
	return Rts2DevTelescope::changeMasterState (new_state);
}


int
main (int argc, char **argv)
{
	Rts2DevTelescopeIr device = Rts2DevTelescopeIr (argc, argv);
	return device.run ();
}
