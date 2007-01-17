#ifndef __RTS_TARGET__
#define __RTS_TARGET__

#include <errno.h>
#include <libnova/libnova.h>
#include <ostream>
#include <stdio.h>
#include <time.h>

#include "status.h"

#include "../utils/objectcheck.h"
#include "../utils/rts2device.h"

#include "rts2taruser.h"
#include "rts2targetset.h"

#define MAX_READOUT_TIME		120
#define PHOT_TIMEOUT			10
#define EXPOSURE_TIMEOUT		50

#define MAX_COMMAND_LENGTH              2000

#define TARGET_NAME_LEN		150

#define TYPE_UNKNOW		'u'

#define TYPE_OPORTUNITY         'O'
#define TYPE_GRB                'G'
#define TYPE_GRB_TEST		'g'
#define TYPE_SKY_SURVEY         'S'
#define TYPE_GPS                'P'
#define TYPE_ELLIPTICAL         'E'
#define TYPE_HETE               'H'
#define TYPE_PHOTOMETRIC	'M'
#define TYPE_TECHNICAL          't'
#define TYPE_TERESTIAL		'T'
#define TYPE_CALIBRATION	'c'
#define TYPE_MODEL		'm'

#define TYPE_SWIFT_FOV		'W'
#define TYPE_INTEGRAL_FOV	'I'

#define TYPE_AUGER		'A'

#define TYPE_DARK		'd'
#define TYPE_FLAT		'f'
#define TYPE_FOCUSING		'o'

#define TYPE_LANDOLT		'l'

// master plan target
#define TYPE_PLAN		'p'

#define COMMAND_EXPOSURE	"E"
#define COMMAND_DARK		"D"
#define COMMAND_FILTER		"F"
#define COMMAND_FOCUSING	'O'
#define COMMAND_CHANGE		"C"
#define COMMAND_BINNING		"BIN"
#define COMMAND_BOX		"BOX"
#define COMMAND_CENTER		"CENTER"
// must be paired with COMMAND_CHANGE
#define COMMAND_WAIT		"W"
#define COMMAND_ACQUIRE		"A"
#define COMMAND_WAIT_ACQUIRE	"Aw"
#define COMMAND_MIRROR_MOVE	"M"
#define COMMAND_PHOTOMETER	"P"
#define COMMAND_STAR_SEARCH	"star"
#define COMMAND_PHOT_SEARCH	"PS"
#define COMMAND_BLOCK_WAITSIG   "block_waitsig"
#define COMMAND_GUIDING		"guiding"
#define COMMAND_BLOCK_ACQ	"ifacq"
#define COMMAND_BLOCK_ELSE	"else"
#define COMMAND_GAIN		"gain"

// HAM acqusition - only on FRAM telescope
#define COMMAND_HAM		"HAM"

// signal handling..
#define COMMAND_SEND_SIGNAL	"SS"
#define COMMAND_WAIT_SIGNAL	"SW"

#define TARGET_DARK		1
#define TARGET_FLAT		2
#define TARGET_FOCUSING		3
// orimary model
#define TARGET_MODEL		4
// primary terestial - based on HAM/FRAM number
#define TARGET_TERRESTIAL	5
// master calibration target
#define TARGET_CALIBRATION	6
// master plan target
#define TARGET_PLAN		7

#define TARGET_SWIFT_FOV	10
#define TARGET_INTEGRAL_FOV	11
#define TARGET_SHOWER		12

typedef enum
{
  OBS_MOVE_FAILED = -1,
  OBS_MOVE = 0,
  OBS_ALREADY_STARTED,
  OBS_DONT_MOVE,
  OBS_MOVE_FIXED,
  OBS_MOVE_UNMODELLED
} moveType;

// move was executed
#define OBS_BIT_MOVED		0x01
// observation started - expect some nice images in db
#define OBS_BIT_STARTED		0x02
// set while in acquisition
#define OBS_BIT_ACQUSITION	0x04
// when observation was interupted
#define OBS_BIT_INTERUPED	0x10
// when acqusition failed
#define OBS_BIT_ACQUSITION_FAI	0x20
// observation was processed
#define OBS_BIT_PROCESSED	0x40

// send message when observation stars
#define SEND_START_OBS		0x01
// send message when first image from given observation get astrometry
#define SEND_ASTRO_OK		0x02
// send message at the end of observation
#define SEND_END_OBS		0x04
// send message at end of processing
#define SEND_END_PROC		0x08
// send message at end of night, with all observations and number of images obtained/processed
#define SEND_END_NIGHT		0x10

class Rts2Obs;
class Rts2TargetSet;
class Rts2Image;

/**
 * Class for one observation.
 *
 * It is created when it's possible to observer such target, and when such
 * observation have some scientific value.
 * 
 * It's put on the observation que inside planc. It's then executed.
 *
 * During execution it can do centering (acquiring images to better know scope position),
 * it can compute importance of future observations.
 *
 * After execution planc select new target for execution, run image processing
 * on first observation target in the que.
 *
 * After all images are processed, new priority can be computed, some results
 * (e.g. light curves) can be put to the database.
 *
 * Target is backed by observation entry in the DB. 
 */
class Target
{
private:
  int epochId;
  int obs_id;
  Rts2Obs *observation;
  int img_id;			// count for images

  int obs_state;		// 0 - not started 0x01 - slew started 0x02 - images taken 0x04 - acquistion started
  // mask with 0xf0 - 0x00 - nominal end 0x10 - interupted 0x20 - acqusition don't converge
  int type;			// light, dark, flat, flat_dark
  int selected;			// how many times startObservation was called

  int startCalledNum;		// how many times startObservation was called - good to know for targets
  double airmassScale;

  time_t observationStart;

  Rts2TarUser *targetUsers;

  int acquired;
  // which changes behaviour based on how many times we called them before

  void sendTargetMail (int eventMask, const char *subject_text, Rts2Block * master);	// send mail to users..

  double minAlt;

  int moveCount;

  float tar_priority;
  float tar_bonus;
  time_t tar_bonus_time;
  time_t tar_next_observable;
  bool tar_enabled;

protected:
  int target_id;
  int obs_target_id;
  char target_type;
  char *target_name;
  char *target_comment;
  struct ln_lnlat_posn *observer;

  virtual int getDBScript (const char *camera_name, char *script);

  // print nice formated log strings
  void logMsg (const char *message, int num);
  void logMsg (const char *message, long num);
  void logMsg (const char *message, double num);
  void logMsg (const char *message, const char *val);
  virtual int selectedAsGood ();	// get called when target was selected to update bonuses etc..
public:
    Target (int in_tar_id, struct ln_lnlat_posn *in_obs);
  // create new target. Save will save it to the database..
    Target ();
    virtual ~ Target (void);
  void logMsg (const char *message);
  void logMsgDb (const char *message);
  void getTargetSubject (std::string & subj);

  // that method is GUARANTIE to be called after target creating to load data from DB
  virtual int load ();
  // load target data from give target id
  int loadTarget (int in_tar_id);
  int save ();
  virtual int save (int tar_id);
  virtual int getScript (const char *device_name, char *buf);
  int setScript (const char *device_name, const char *buf);
  struct ln_lnlat_posn *getObserver ()
  {
    return observer;
  }
  int getPosition (struct ln_equ_posn *pos)
  {
    return getPosition (pos, ln_get_julian_from_sys ());
  }
  // return target position at given julian date
  virtual int getPosition (struct ln_equ_posn *pos, double JD)
  {
    return -1;
  }

  // some libnova trivials to get more comfortable access to
  // coordinates
  int getAltAz (struct ln_hrz_posn *hrz)
  {
    return getAltAz (hrz, ln_get_julian_from_sys ());
  }

  virtual double getMinAlt ()
  {
    return minAlt;
  }

  virtual int getAltAz (struct ln_hrz_posn *hrz, double JD);

  int getGalLng (struct ln_gal_posn *gal)
  {
    return getGalLng (gal, ln_get_julian_from_sys ());
  }
  virtual int getGalLng (struct ln_gal_posn *gal, double JD);

  double getGalCenterDist ()
  {
    return getGalCenterDist (ln_get_julian_from_sys ());
  }
  double getGalCenterDist (double JD);

  double getAirmass ()
  {
    return getAirmass (ln_get_julian_from_sys ());
  }
  virtual double getAirmass (double JD);

  double getZenitDistance ()
  {
    return getZenitDistance (ln_get_julian_from_sys ());
  }

  double getZenitDistance (double JD);

  /**
   * Returns target HA in arcdeg, e.g. in same value as target RA is given.
   */
  double getHourAngle ()
  {
    return getHourAngle (ln_get_julian_from_sys ());
  }

  double getHourAngle (double JD);

  double getDistance (struct ln_equ_posn *in_pos)
  {
    return getDistance (in_pos, ln_get_julian_from_sys ());
  }

  double getDistance (struct ln_equ_posn *in_pos, double JD);

  double getRaDistance (struct ln_equ_posn *in_pos)
  {
    return getRaDistance (in_pos, ln_get_julian_from_sys ());
  }

  double getRaDistance (struct ln_equ_posn *in_pos, double JD);

  double getSolarDistance ()
  {
    return getSolarDistance (ln_get_julian_from_sys ());
  }

  double getSolarDistance (double JD);

  double getSolarRaDistance ()
  {
    return getSolarRaDistance (ln_get_julian_from_sys ());
  }

  double getSolarRaDistance (double JD);

  double getLunarDistance ()
  {
    return getLunarDistance (ln_get_julian_from_sys ());
  }

  double getLunarDistance (double JD);

  double getLunarRaDistance ()
  {
    return getLunarRaDistance (ln_get_julian_from_sys ());
  }

  double getLunarRaDistance (double JD);

  double getMeridianDistance ()
  {
    return fabs (getHourAngle ());
  }

  double getMeridianDistance (double JD)
  {
    return fabs (getHourAngle (JD));
  }

  // time in seconds to object set/rise/meridian pass (if it's visible, otherwise -1 (for
  // circumpolar & not visible objects)
  int secToObjectSet ()
  {
    return secToObjectSet (ln_get_julian_from_sys ());
  }

  int secToObjectSet (double JD);

  int secToObjectRise ()
  {
    return secToObjectRise (ln_get_julian_from_sys ());
  }

  int secToObjectRise (double JD);

  int secToObjectMeridianPass ()
  {
    return secToObjectMeridianPass (ln_get_julian_from_sys ());
  }

  int secToObjectMeridianPass (double JD);

  int getEpoch ()
  {
    return epochId;
  }

  int getTargetID ()
  {
    return target_id;
  }
  virtual int getObsTargetID ()
  {
    if (obs_target_id > 0)
      return obs_target_id;
    return getTargetID ();
  }
  char getTargetType ()
  {
    return target_type;
  }
  void setTargetType (char in_target_type)
  {
    target_type = in_target_type;
  }
  const char *getTargetName ()
  {
    return target_name;
  }
  void setTargetName (const char *in_target_name)
  {
    delete[]target_name;
    target_name = new char[strlen (in_target_name) + 1];
    strcpy (target_name, in_target_name);
  }
  const char *getTargetComment ()
  {
    return target_comment;
  }
  void setTargetComment (const char *in_target_comment)
  {
    delete target_comment;
    target_comment = new char[strlen (in_target_comment) + 1];
    strcpy (target_comment, in_target_comment);
  }
  bool getTargetEnabled ()
  {
    return tar_enabled;
  }
  void setTargetEnabled (bool new_en = true)
  {
    tar_enabled = new_en;
  }

  float getTargetPriority ()
  {
    return tar_priority;
  }

  void setTargetPriority (float new_priority)
  {
    tar_priority = new_priority;
  }

  float getTargetBonus ()
  {
    return tar_bonus;
  }

  const time_t *getTargetBonusTime ()
  {
    return &tar_bonus_time;
  }

  void setTargetBonus (float new_bonus, time_t * new_time = NULL)
  {
    tar_bonus = new_bonus;
    if (new_time)
      setTargetBonusTime (new_time);
  }

  void setTargetBonusTime (time_t * new_time)
  {
    tar_bonus_time = *new_time;
  }

  int getObsId ()
  {
    return obs_id;
  }

  /**
   * Set observation ID and start observation
   */
  void setObsId (int new_obs_id)
  {
    obs_id = new_obs_id;
    selected++;
    obs_state |= OBS_BIT_MOVED;
  }

  int getRST (struct ln_rst_time *rst)
  {
    return getRST (rst, ln_get_julian_from_sys (), LN_STAR_STANDART_HORIZON);
  }
  virtual int getRST (struct ln_rst_time *rst, double jd, double horizon)
  {
    return -1;
  }

  // returns 1 when we are almost the same target, so 
  // interruption of this target is not necessary
  // otherwise (when interruption is necessary) returns 0
  virtual int compareWithTarget (Target * in_target, double in_sep_limit);
  virtual moveType startSlew (struct ln_equ_posn *position);
  virtual moveType afterSlewProcessed ();
  void moveStarted ();
  void moveEnded ();
  void moveFailed ();
  bool moveWasStarted ();
  bool wasMoved ();
  // return 1 if observation is already in progress, 0 if observation started, -1 on error
  // 2 if we don't need to move
  int startObservation (Rts2Block * master);
  void acqusitionStart ();
  void acqusitionEnd ();
  void acqusitionFailed ();
  // called when waiting for acqusition..
  int isAcquired ()
  {
    return (acquired == 1);
  }
  // return 0 when acquistion isn't running, non 0 when we are currently
  // acquiring target (searching for correct field)
  int isAcquiring ()
  {
    return (obs_state & OBS_BIT_ACQUSITION);
  }
  int getAcquired ()
  {
    return acquired;
  }
  void nullAcquired ()
  {
    acquired = 0;
  }
  void interupted ();

  // similar to startSlew - return 0 if observation ends, 1 if
  // it doesn't ends (ussually in case when in_next_id == target_id),
  // -1 on errror

  int endObservation (int in_next_id, Rts2Block * master);

  virtual int endObservation (int in_next_id);

  // returns 1 if target is continuus - in case next target is same | next
  // targer doesn't exists, we keep exposing and we will not move mount between
  // exposures. Good for darks observation, partial good for GRB (when we solve
  // problem with moving mount in exposures - position updates)
  // returns 2 when target need change - usefull for plan target and other targets,
  // which set obs_target_id
  virtual int isContinues ();

  int observationStarted ();

  virtual int beforeMove ();	// called when we can move to next observation - good to generate next target in mosaic observation etc..
  int postprocess ();
  virtual int isGood (double lst, double JD, struct ln_equ_posn *pos);
  virtual int isGood (double JD);
  // scheduler functions
  virtual int considerForObserving (double JD);	// return 0, when target can be observed, otherwise modify tar_bonus..
  virtual int dropBonus ();
  float getBonus ()
  {
    return getBonus (ln_get_julian_from_sys ());
  }
  virtual float getBonus (double JD);
  virtual int changePriority (int pri_change, time_t * time_ch);
  int changePriority (int pri_change, double validJD);

  virtual int setNextObservable (time_t * time_ch);
  int setNextObservable (double validJD);

  int getNextImgId ()
  {
    return ++img_id;
  }

  int getSelected ()
  {
    return selected;
  }

  int getNumObs (time_t * start_time, time_t * end_time);
  double getLastObsTime ();	// return time in seconds to last observation of same target

  int getCalledNum ()
  {
    return startCalledNum;
  }

  double getAirmassScale ()
  {
    return airmassScale;
  }

  /**
   * Returns time of first observation, or nan if no observation.
   */
  double getFirstObs ();

  /**
   * Returns time of last observation, or nan if no observation.
   */
  double getLastObs ();

  /**
   * Prints extra informations about target, which can be interested to user.
   *
   * @param _os stream to print that
   */
  virtual void printExtra (std::ostream & _os, double JD);

  /**
   * print short target info
   */
  void printShortInfo (std::ostream & _os)
  {
    printShortInfo (_os, ln_get_julian_from_sys ());
  }

  virtual void printShortInfo (std::ostream & _os, double JD);

  /**
   * Prints one-line info with bonus inforamtions
   */

  void printShortBonusInfo (std::ostream & _os)
  {
    printShortBonusInfo (_os, ln_get_julian_from_sys ());
  }

  virtual void printShortBonusInfo (std::ostream & _os, double JD);

  /**
   * Prints position info for given JD.
   *
   * @param _os stream to print that
   * @param JD date for which to print info
   */
  virtual void sendPositionInfo (std::ostream & _os, double JD);
  void sendInfo (std::ostream & _os)
  {
    sendInfo (_os, ln_get_julian_from_sys ());
  }
  virtual void sendInfo (std::ostream & _os, double JD);

  std::string getUsersEmail (int in_event_mask, int &count);


  /**
   * Print observations at current target postion around given position.
   *
   */
  int printObservations (double radius, std::ostream & _os)
  {
    return printObservations (radius, ln_get_julian_from_sys (), _os);
  }

  int printObservations (double radius, double JD, std::ostream & _os);

  int printTargets (double radius, std::ostream & _os)
  {
    return printTargets (radius, ln_get_julian_from_sys (), _os);
  }

  int printTargets (double radius, double JD, std::ostream & _os);

  int printImages (std::ostream & _os)
  {
    return printImages (ln_get_julian_from_sys (), _os);
  }

  int printImages (double JD, std::ostream & _os);

  /**
   * Return calibration targets for given target
   */
  Rts2TargetSet *getCalTargets ()
  {
    return getCalTargets (ln_get_julian_from_sys ());
  }

  virtual Rts2TargetSet *getCalTargets (double JD);

  virtual void writeToImage (Rts2Image * image);
};

class ConstTarget:public Target
{
private:
  struct ln_equ_posn position;
protected:

    virtual int selectedAsGood ();	// get called when target was selected to update bonuses, target position etc..
public:
    ConstTarget ();
    ConstTarget (int in_tar_id, struct ln_lnlat_posn *in_obs);
    ConstTarget (int in_tar_id, struct ln_lnlat_posn *in_obs,
		 struct ln_equ_posn *pos);
  virtual int load ();
  virtual int save (int tar_id);
  virtual int getPosition (struct ln_equ_posn *pos, double JD);
  virtual int getRST (struct ln_rst_time *rst, double jd, double horizon);
  virtual int compareWithTarget (Target * in_target, double grb_sep_limit);
  virtual void printExtra (std::ostream & _os, double JD);

  void setPosition (double ra, double dec)
  {
    position.ra = ra;
    position.dec = dec;
  }
};

class EllTarget:public Target
{
private:
  struct ln_ell_orbit orbit;
public:
    EllTarget (int in_tar_id, struct ln_lnlat_posn *in_obs);
    EllTarget ():Target ()
  {
  };
  virtual int load ();
  virtual int getPosition (struct ln_equ_posn *pos, double JD);
  virtual int getRST (struct ln_rst_time *rst, double jd, double horizon);

  virtual void printExtra (std::ostream & _os, double JD);
};

class DarkTarget;

class PossibleDarks
{
private:
  char *deviceName;
  // 
  void addDarkExposure (float exp);
  int defaultDark ();
  int dbDark ();
  // we will need temperature as well
    std::list < float >dark_exposures;
  DarkTarget *target;
public:
    PossibleDarks (DarkTarget * in_target, const char *in_deviceName);
    virtual ~ PossibleDarks (void);
  int getScript (char *buf);
  int isName (const char *in_deviceName);
};

class DarkTarget:public Target
{
private:
  struct ln_equ_posn currPos;
    std::list < PossibleDarks * >darkList;
public:
    DarkTarget (int in_tar_id, struct ln_lnlat_posn *in_obs);
    virtual ~ DarkTarget (void);
  virtual int getScript (const char *deviceName, char *buf);
  virtual int getPosition (struct ln_equ_posn *pos, double JD);
  virtual moveType startSlew (struct ln_equ_posn *position);
  virtual int isContinues ()
  {
    return 1;
  }
};

class FlatTarget:public ConstTarget
{
private:
  void getAntiSolarPos (struct ln_equ_posn *pos, double JD);
public:
    FlatTarget (int in_tar_id, struct ln_lnlat_posn *in_obs);
  virtual int getScript (const char *deviceName, char *buf);
  virtual int load ();
  virtual int getPosition (struct ln_equ_posn *pos, double JD);
  virtual int considerForObserving (double JD);
  virtual int isContinues ()
  {
    return 1;
  }
};

// possible calibration target
class PosCalibration:public Target
{
private:
  double currAirmass;
  char type_id;
  struct ln_equ_posn object;
public:
    PosCalibration (int in_tar_id, double ra, double dec, char in_type_id,
		    char *in_tar_name, struct ln_lnlat_posn *in_observer,
		    double JD):Target (in_tar_id, in_observer)
  {
    struct ln_hrz_posn hrz;

      object.ra = ra;
      object.dec = dec;
      ln_get_hrz_from_equ (&object, observer, JD, &hrz);
      currAirmass = ln_get_airmass (hrz.alt, getAirmassScale ());

      setTargetType (in_type_id);
      setTargetName (in_tar_name);
  }
  double getCurrAirmass ()
  {
    return currAirmass;
  }
  virtual int getPosition (struct ln_equ_posn *in_pos, double JD)
  {
    in_pos->ra = object.ra;
    in_pos->dec = object.dec;
    return 0;
  }
};

class CalibrationTarget:public ConstTarget
{
private:
  struct ln_equ_posn airmassPosition;
  time_t lastImage;
  int needUpdate;
public:
    CalibrationTarget (int in_tar_id, struct ln_lnlat_posn *in_obs);
  virtual int load ();
  virtual int beforeMove ();
  virtual int endObservation (int in_next_id);
  virtual int getPosition (struct ln_equ_posn *pos, double JD);
  virtual int considerForObserving (double JD);
  virtual int changePriority (int pri_change, time_t * time_ch)
  {
    return 0;
  }
  virtual float getBonus (double JD);
};

class FocusingTarget:public ConstTarget
{
public:
  FocusingTarget (int in_tar_id,
		  struct ln_lnlat_posn *in_obs):ConstTarget (in_tar_id,
							     in_obs)
  {
  };
  virtual int getScript (const char *deviceName, char *buf);
};

class ModelTarget:public ConstTarget
{
private:
  struct ln_hrz_posn hrz_poz;
  struct ln_equ_posn equ_poz;
  double ra_noise;
  double dec_noise;
  float alt_start;
  float alt_stop;
  float alt_step;
  float az_start;
  float az_stop;
  float az_step;
  float noise;
  int step;

  int alt_size;

  int modelStepType;

  int writeStep ();
  int getNextPosition ();
  int calPosition ();
public:
    ModelTarget (int in_tar_id, struct ln_lnlat_posn *in_obs);
    virtual ~ ModelTarget (void);
  virtual int load ();
  virtual int beforeMove ();
  virtual moveType afterSlewProcessed ();
  virtual int endObservation (int in_next_id);
  virtual int getPosition (struct ln_equ_posn *pos, double JD);
};

class OportunityTarget:public ConstTarget
{
public:
  OportunityTarget (int in_tar_id, struct ln_lnlat_posn *in_obs);
  virtual float getBonus (double JD);
  virtual int isContinues ()
  {
    return 1;
  }
};

class LunarTarget:public Target
{
public:
  LunarTarget (int in_tar_id, struct ln_lnlat_posn * in_obs);
  virtual int getScript (const char *deviceName, char *buf);
  virtual int getPosition (struct ln_equ_posn *pos, double JD);
  virtual int getRST (struct ln_rst_time *rst, double jd, double horizon);
};

class TargetGRB:public ConstTarget
{
private:
  double grbDate;
  double lastUpdate;
  int gcnPacketType;		// gcn packet from last update
  int gcnGrbId;
  bool grb_is_grb;
  int shouldUpdate;
  int gcnPacketMin;		// usefull for searching for packet class
  int gcnPacketMax;

  int maxBonusTimeout;
  int dayBonusTimeout;
  int fiveBonusTimeout;

  struct ln_equ_posn grb;
  float errorbox;

  const char *getSatelite ();
protected:
    virtual int getDBScript (const char *camera_name, char *script);
public:
    TargetGRB (int in_tar_id, struct ln_lnlat_posn *in_obs,
	       int in_maxBonusTimeout, int in_dayBonusTimeout,
	       int in_fiveBonusTimeout);
  virtual int load ();
  virtual int getPosition (struct ln_equ_posn *pos, double JD);
  virtual int compareWithTarget (Target * in_target, double grb_sep_limit);
  virtual int getScript (const char *deviceName, char *buf);
  virtual int beforeMove ();
  virtual float getBonus (double JD);
  virtual double getMinAlt ()
  {
    return 0;
  }
  // some logic needed to distinguish states when GRB position change
  // from last observation. there was update etc..
  virtual int isContinues ();

  double getFirstPacket ();
  virtual void printExtra (std::ostream & _os, double JD);
  void printGrbList (std::ostream & _os);

  virtual void writeToImage (Rts2Image * image);
};

/**
 * Swift Field of View - generate observations on Swift Field of View, based on
 * data received from GCN
 *
 */
class TargetSwiftFOV:public Target
{
private:
  int oldSwiftId;
  int swiftId;
  struct ln_equ_posn swiftFovCenter;
  time_t swiftTimeStart;
  time_t swiftTimeEnd;
  double swiftOnBonus;
  char *swiftName;
  double swiftRoll;
public:
    TargetSwiftFOV (int in_tar_id, struct ln_lnlat_posn *in_obs);
    virtual ~ TargetSwiftFOV (void);

  virtual int load ();		// find Swift pointing for observation
  virtual int getPosition (struct ln_equ_posn *pos, double JD);
  virtual int getRST (struct ln_rst_time *rst, double JD, double horizon);
  virtual moveType afterSlewProcessed ();
  virtual int considerForObserving (double JD);	// return 0, when target can be observed, otherwise modify tar_bonus..
  virtual int beforeMove ();
  virtual float getBonus (double JD);
  virtual int isContinues ();

  virtual void printExtra (std::ostream & _os, double JD);
};

class TargetIntegralFOV:public Target
{
private:
  int oldIntegralId;
  int integralId;
  struct ln_equ_posn integralFovCenter;
  time_t integralTimeStart;
  double integralOnBonus;
public:
    TargetIntegralFOV (int in_tar_id, struct ln_lnlat_posn *in_obs);
    virtual ~ TargetIntegralFOV (void);

  virtual int load ();		// find Swift pointing for observation
  virtual int getPosition (struct ln_equ_posn *pos, double JD);
  virtual int getRST (struct ln_rst_time *rst, double JD, double horizon);
  virtual moveType afterSlewProcessed ();
  virtual int considerForObserving (double JD);	// return 0, when target can be observed, otherwise modify tar_bonus..
  virtual int beforeMove ();
  virtual float getBonus (double JD);
  virtual int isContinues ();

  virtual void printExtra (std::ostream & _os, double JD);
};

class TargetGps:public ConstTarget
{
public:
  TargetGps (int in_tar_id, struct ln_lnlat_posn *in_obs);
  virtual float getBonus (double JD);
};

class TargetSkySurvey:public ConstTarget
{
public:
  TargetSkySurvey (int in_tar_id, struct ln_lnlat_posn *in_obs);
  virtual float getBonus (double JD);
};

class TargetTerestial:public ConstTarget
{
public:
  TargetTerestial (int in_tar_id, struct ln_lnlat_posn *in_obs);
  virtual int considerForObserving (double JD);
  virtual float getBonus (double JD);
  virtual moveType afterSlewProcessed ();
};

class Rts2Plan;

class TargetPlan:public Target
{
private:
  Rts2Plan * selectedPlan;
  Rts2Plan *nextPlan;

  // how long to look back for previous plan
  float hourLastSearch;
  float hourConsiderPlans;
  time_t nextTargetRefresh;
  void refreshNext ();
protected:
    virtual int getDBScript (const char *camera_name, char *script);
public:
    TargetPlan (int in_tar_id, struct ln_lnlat_posn *in_obs);
    virtual ~ TargetPlan (void);

  virtual int load ();
  virtual int load (double JD);
  virtual int getPosition (struct ln_equ_posn *pos, double JD);
  virtual int getRST (struct ln_rst_time *rst, double JD, double horizon);
  virtual int getObsTargetID ();
  virtual int considerForObserving (double JD);
  virtual float getBonus (double JD);
  virtual int isContinues ();
  virtual int beforeMove ();
  virtual moveType startSlew (struct ln_equ_posn *position);

  virtual void printExtra (std::ostream & _os, double JD);
};

// load target from DB
Target *createTarget (int in_tar_id);

Target *createTarget (int in_tar_id, struct ln_lnlat_posn *in_obs);

// send end mails
void sendEndMails (const time_t * t_from, const time_t * t_to,
		   int printImages, int printCounts,
		   struct ln_lnlat_posn *in_obs, Rts2App * master);

// print target information to stdout..
std::ostream & operator << (std::ostream & _os, Target & target);

#endif /*! __RTS_TARGET__ */
