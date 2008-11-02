/*
 * Scheduling body.
 * Copyright (C) 2008 Petr Kubanek <petr@kubanek.net>
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

#include "../utilsdb/rts2appdb.h"
#include "../utils/rts2config.h"

#include "rts2schedbag.h"

#define OPT_START_DATE		OPT_LOCAL + 210
#define OPT_END_DATE		OPT_LOCAL + 211

/**
 * Class of the scheduler application.  Prepares schedule, and run
 * optimalization few times to get them out..
 *
 * @author Petr Kubanek <petr@kubanek.net>
 */
class Rts2ScheduleApp: public Rts2AppDb
{
	private:
		Rts2SchedBag *schedBag;

		// verbosity level
		int verbose;

		// number of populations
		int generations;

		// population size
		int popSize;

		// used algorithm
		enum {SGA, NSGAII} algorithm;

		// start and end JD
		double startDate;
		double endDate;

		/**
		 * Print merit of given type.
		 *
		 * @param _type Merit type.
		 * @param _name Name of the merit function (for printing).
		 *
		 * @see objFunc
		 */
		void printMerits (objFunc _type, const char *name);

		/**
		 * Print objective functions for SGA.
		 */
		void printSGAMerits ();

		/**
		 * Prints objective functions of NSGAII.
		 */
		void printNSGAMerits ();

		/**
		 * Prints all merits statistics of schedule set.
		 */
		void printMerits ();

	protected:
		virtual void usage ();
		virtual void help ();
		virtual int processOption (int _opt);
		virtual int init ();

	public:
		Rts2ScheduleApp (int argc, char ** argv);
		virtual ~Rts2ScheduleApp (void);

		virtual int doProcessing ();
};


void
Rts2ScheduleApp::printMerits (objFunc _type, const char *name)
{
	Rts2SchedBag::iterator iter;
	std::cout << name << ": ";

	for (iter = schedBag->begin (); iter != schedBag->end (); iter++)
	{
		std::cout << std::left << std::setw (8) << (*iter)->getObjectiveFunction (_type) << " ";
	}

	double min, avg, max;
	schedBag->getStatistics (min, avg, max, _type);

	std::cout << std::endl << name << " statistics: "
		<< min << " "
		<< avg << " "
		<< max << std::endl;
}


void
Rts2ScheduleApp::printSGAMerits ()
{
	Rts2SchedBag::iterator iter;
	schedBag->calculateNSGARanks ();
	// print header
	std::cout << std::left
		<< std::setw (11) << "ALTITUDE"
		<< std::setw (11) << "ACCOUNT"
		<< std::setw (11) << "DISTANCE"
		<< std::setw (11) << "VISIBILITY"
		<< std::setw (4) << "SCH"
		<< std::setw (11) << "SINGLE"
		<< std::endl;
	for (iter = schedBag->begin (); iter < schedBag->end (); iter++)
	{
		std::cout << std::left 
			<< std::setw (11) << (*iter)->getObjectiveFunction (ALTITUDE)
			<< std::setw (11) << (*iter)->getObjectiveFunction (ACCOUNT)
			<< std::setw (11) << (*iter)->getObjectiveFunction (DISTANCE)
			<< std::setw (11) << (*iter)->getObjectiveFunction (VISIBILITY) 
			<< std::setw (4) << (*iter)->getConstraintFunction (CONSTR_SCHEDULE_TIME)
			<< std::setw (11) << (*iter)->getObjectiveFunction (SINGLE) 
			<< std::endl; 
	}
}

void
Rts2ScheduleApp::printNSGAMerits ()
{
	Rts2SchedBag::iterator iter;
	schedBag->calculateNSGARanks ();
	// print header
	std::cout << std::left << "RNK "
		<< std::setw (11) << "ALTITUDE"
		<< std::setw (11) << "ACCOUNT"
		<< std::setw (11) << "DISTANCE"
		<< std::setw (11) << "VISIBILITY"
		<< std::setw (4) << "SCH"
		<< std::endl;
	for (iter = schedBag->begin (); iter < schedBag->end (); iter++)
	{
		std::cout << std::left << std::setw (4) << (*iter)->getNSGARank ()
			<< std::setw (11) << (*iter)->getObjectiveFunction (ALTITUDE)
			<< std::setw (11) << (*iter)->getObjectiveFunction (ACCOUNT)
			<< std::setw (11) << (*iter)->getObjectiveFunction (DISTANCE)
			<< std::setw (11) << (*iter)->getObjectiveFunction (VISIBILITY)
			<< std::setw (4) << (*iter)->getConstraintFunction (CONSTR_SCHEDULE_TIME)
			<< std::endl; 
	}
}


void
Rts2ScheduleApp::printMerits ()
{
	switch (algorithm)
	{
		case SGA:
			printSGAMerits ();
			break;
		case NSGAII:
			printNSGAMerits ();
			break;
	}
}


void
Rts2ScheduleApp::usage ()
{
	std::cout << "\t" << getAppName () << std::endl;
}


void
Rts2ScheduleApp::help ()
{
	std::cout << "Create schedule for given night. The programme uses genetic algorithm scheduling, described at \
http://rts2.org/scheduling.pdf." << std::endl
		<< "You are free to experiment with various settings to create optimal observing schedule" << std::endl;
	Rts2AppDb::help ();
}


int
Rts2ScheduleApp::processOption (int _opt)
{
	switch (_opt)
	{
		case 'v':
			verbose++;
			break;
		case 'g':
			generations = atoi (optarg);
			break;
		case 'p':
			popSize = atoi (optarg);
			if (popSize <= 0)
			{
				logStream (MESSAGE_ERROR) << "Population size must be positive number " << optarg << sendLog;
				return -1;
			}
			break;		
		case 'a':
			if (!strcasecmp (optarg, "SGA"))
			{
				algorithm = SGA;
			}
			else if (!strcasecmp (optarg, "NSGAII"))
			{
			  	algorithm = NSGAII;
			}
			else
			{
				logStream (MESSAGE_ERROR) << "Unknow algorithm: " << optarg << sendLog;
			  	return -1;
			}
			break;
		case OPT_START_DATE:
			return parseDate (optarg, startDate);
		case OPT_END_DATE:
			return parseDate (optarg, endDate);
		default:
			return Rts2AppDb::processOption (_opt);
	}
	return 0;
}


int
Rts2ScheduleApp::init ()
{
	int ret;
	ret = Rts2AppDb::init ();
	if (ret)
		return ret;

	srandom (time (NULL));

	// initialize schedules..
	if (isnan (startDate))
		startDate = ln_get_julian_from_sys ();
	if (isnan (endDate))
	  	endDate = startDate + 0.5;
	if (startDate >= endDate)
	{
		  std::cerr << "Scheduling interval end date is behind scheduling start date, start is "
		  	<< LibnovaDate (startDate) << ", end is "
			<< LibnovaDate (endDate)
			<< std::cerr;
	}

	std::cout << "Generating schedule from " << LibnovaDate (startDate) << " to " << LibnovaDate (endDate) << std::endl;

	// create list of schedules..
	schedBag = new Rts2SchedBag (startDate, endDate);

	ret = schedBag->constructSchedules (popSize);
	if (ret)
		return ret;

	return 0;
}


Rts2ScheduleApp::Rts2ScheduleApp (int argc, char ** argv): Rts2AppDb (argc, argv)
{
	schedBag = NULL;
	verbose = 0;
	generations = 1500;
	popSize = 100;
	algorithm = SGA;

	startDate = nan ("f");
	endDate = nan ("f");

	addOption ('v', NULL, 0, "verbosity level");
	addOption ('g', NULL, 1, "number of generations");
	addOption ('p', NULL, 1, "population size");
	addOption ('a', NULL, 1, "algorithm (SGA or NSGAII are currently supported)");

	addOption (OPT_START_DATE, "start", 1, "produce schedule from this date");
	addOption (OPT_END_DATE, "end", 1, "produce schedule till this date");
}


Rts2ScheduleApp::~Rts2ScheduleApp (void)
{
	delete schedBag;
}


int
Rts2ScheduleApp::doProcessing ()
{
	if (verbose)
	  	printMerits ();

	for (int i = 1; i <= generations; i++)
	{
		switch (algorithm)
		{
			case SGA:
				schedBag->doGAStep ();
				break;
			case NSGAII:
				schedBag->doNSGAIIStep ();
				break;
		}


		if (verbose > 1)
		{
			printMerits ();
		}
		else
		{
			// collect and print statistics..
			double _min, _avg, _max;
			schedBag->getStatistics (_min, _avg, _max);

			std::cout << std::right << std::setw (5) << i << " "
				<< std::setw (4) << schedBag->size () << " "
				<< std::setw (10) << _min << " "
				<< std::setw (10) << _avg << " "
				<< std::setw (10) << _max << " "
				<< std::setw (4) << schedBag->constraintViolation (CONSTR_VISIBILITY) << " "
				<< std::setw (4) << schedBag->constraintViolation (CONSTR_SCHEDULE_TIME);
			int rankSize = 0;
			int rank = 0;
			// print addtional algoritm specific info
			switch (algorithm)
			{
				case SGA:
					break;
				case NSGAII:
					// print populations size by ranks..
					while (true)
				  	{
						rankSize = schedBag->getNSGARankSize (rank);
						if (rankSize <= 0)
							break;
						std::cout << " " << std::right << std::setw (3) << rankSize;
						rank++;
					}

					break;
			}
				
			std::cout << std::endl;
		}
	}

	if (verbose)	
		printMerits ();

	return 0;
}


int
main (int argc, char ** argv)
{
	Rts2ScheduleApp app = Rts2ScheduleApp (argc, argv);
	return app.run ();
}
