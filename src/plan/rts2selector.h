/*
 * Selector body.
 * Copyright (C) 2003-2008 Petr Kubanek <petr@kubanek.net>
 * Copyright (C) 2011 Petr Kubanek, Institute of Physics <kubanek@fzu.cz>
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

#ifndef __RTS2_SELECTOR__
#define __RTS2_SELECTOR__

#include <algorithm>

#include "../utilsdb/rts2appdb.h"
#include "../utilsdb/target.h"

namespace rts2plan
{

/**
 * Holds target together with its bonus. Used to order targets by bonus for
 * selection.
 *
 * @author Petr Kubanek <petr@kubanek.net>
 */
class TargetEntry
{
	public:
		TargetEntry (rts2db::Target *_target) { target = _target; bonus = rts2_nan ("f"); }
		~TargetEntry () { delete target; }
		rts2db::Target * target;
		double bonus;
		void updateBonus () { bonus = target->getBonus (); }
};

/**
 * For sorting TargetEntry by bonus.
 *
 * @author Petr Kubanek <petr@kubanek.net>
 */
struct bonusSort: public std::binary_function <TargetEntry *, TargetEntry *, bool>
{
	bool operator () (TargetEntry * t1, TargetEntry * t2) { return t1->bonus > t2->bonus; }
};

/**
 * Select next target. Traverse list of targets which are enabled and select
 * target with biggest priority.
 *
 * @author Petr Kubanek <petr@kubanek.net>
 */
class Selector
{
	public:
		Selector ();
		virtual ~ Selector (void);

		void setObserver (struct ln_lnlat_posn *in_observer) { observer = in_observer; }
		void init ();

		// return next observation..
		int selectNext (int masterState, double az1 = rts2_nan ("f"), double az2 = rts2_nan ("f"));
		int selectNextNight (int in_bonusLimit = 0, bool verbose = false, double az1 = rts2_nan ("f"), double az2 = rts2_nan ("f"));

		double getFlatSunMin () { return flat_sun_min; }
		double getFlatSunMax () { return flat_sun_max; }

		void setFlatSunMin (double in_m) { flat_sun_min = in_m; }
		void setFlatSunMax (double in_m) { flat_sun_max = in_m; }

		/**
		 * Returns night disabled types.
		 *
		 * @return String with night disabled types.
		 */
		std::string getNightDisabledTypes ();

		/**
		 * Set types of targets which are not permited for selection
		 * during night.
		 *
		 * @param disabledTypes  List of types (separated by space) which are disabled.
		 *
		 * @return -1 if list is incorrect, otherwise 0.
		 */
		int setNightDisabledTypes (const char *types);

		/**
		 * Add filters avaikabke for device.
		 */
		void addFilters (const char *cam, std::vector <std::string> filters) { availableFilters[std::string(cam)] = filters; }

		/**
		 * Add filter aliases.
		 */
		void addFilterAlias (std::string filter, std::string alias) { filterAliases[filter] = alias; }

		void readFilters (std::string camera, std::string fn);
		void readAliasFile (const char *aliasFile);

		void parseFilterOption (const char *in_optarg);
		void parseFilterFileOption (const char *in_optarg);

	private:
		std::vector < TargetEntry* > possibleTargets;
		void considerTarget (int consider_tar_id, double JD);
		std::vector <char> nightDisabledTypes;
		void checkTargetObservability ();
		void checkTargetBonus ();
		void findNewTargets ();
		int selectFlats ();
		int selectDarks ();
		struct ln_lnlat_posn *observer;
		double flat_sun_min;
		double flat_sun_max;

		// available filters for filter command on cameras
		std::map <std::string, std::vector < std::string > > availableFilters;
		std::map <std::string, std::string> filterAliases;

		/**
		 * Checks if type is among types disabled for night selection.
		 *
		 * @param type_id   Type id which will be checked for incursion in nightDisabledTypes.
		 *
		 * @return True if type is among disabled types.
		 */
		bool isInNightDisabledTypes (char target_type)
		{
			return (std::find (nightDisabledTypes.begin (), nightDisabledTypes.end (), target_type) != nightDisabledTypes.end ());
		}
};

}
#endif							 /* !__RTS2_SELECTOR__ */
