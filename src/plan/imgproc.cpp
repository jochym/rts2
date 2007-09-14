#include "../utilsdb/rts2devicedb.h"
#include "status.h"
#include "rts2connimgprocess.h"
#include "rts2script.h"

#include <glob.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <iostream>
#include <stdio.h>

class Rts2ImageProc;

class Rts2ImageProc:public Rts2DeviceDb
{
private:
  std::list < Rts2ConnProcess * >imagesQue;
  Rts2ConnProcess *runningImage;
  Rts2ValueInteger *goodImages;
  Rts2ValueInteger *trashImages;
  Rts2ValueInteger *morningImages;

  Rts2ValueInteger *queSize;

  int sendStop;			// if stop running astrometry with stop signal; it ussually doesn't work, so we will use FIFO
    std::string defaultImgProcess;
    std::string defaultObsProcess;
    std::string defaultDarkProcess;
    std::string defaultFlatProcess;
  glob_t imageGlob;
  unsigned int globC;
  int reprocessingPossible;
protected:
    virtual int reloadConfig ();
public:
    Rts2ImageProc (int argc, char **argv);
    virtual ~ Rts2ImageProc (void);

  virtual void postEvent (Rts2Event * event);
  virtual int idle ();
  virtual int ready ()
  {
    return 0;
  }

  virtual int info ();

  virtual int changeMasterState (int new_state);

  virtual int deleteConnection (Rts2Conn * conn);

  int que (Rts2ConnProcess * newProc);

  int queImage (Rts2Conn * conn, const char *in_path);
  int doImage (Rts2Conn * conn, const char *in_path);

  int queObs (Rts2Conn * conn, int obsId);

  int queDarks (Rts2Conn * conn);
  int queFlats (Rts2Conn * conn);

  int checkNotProcessed ();
  void changeRunning (Rts2ConnProcess * newImage);

  virtual int commandAuthorized (Rts2Conn * conn);
};

Rts2ImageProc::Rts2ImageProc (int in_argc, char **in_argv):
Rts2DeviceDb (in_argc, in_argv, DEVICE_TYPE_IMGPROC, "IMGP")
{
  runningImage = NULL;

  createValue (goodImages, "good_images", "number of good images", false);
  goodImages->setValueInteger (0);

  createValue (trashImages, "trash_images",
	       "number of images which ended in trash (bad images)", false);
  trashImages->setValueInteger (0);

  createValue (morningImages, "morning_images",
	       "number of images which will be processed at morning", false);
  morningImages->setValueInteger (0);

  createValue (queSize, "que_size", "size of image que", false);

  imageGlob.gl_pathc = 0;
  imageGlob.gl_offs = 0;
  globC = 0;
  reprocessingPossible = 0;

  sendStop = 0;
}

Rts2ImageProc::~Rts2ImageProc (void)
{
  if (runningImage)
    delete runningImage;
  if (imageGlob.gl_pathc)
    globfree (&imageGlob);
}

int
Rts2ImageProc::reloadConfig ()
{
  int ret;
  ret = Rts2DeviceDb::reloadConfig ();
  if (ret)
    return ret;

  Rts2Config *config;
  config = Rts2Config::instance ();

  ret = config->getString ("imgproc", "astrometry", defaultImgProcess);
  if (ret)
    {
      logStream (MESSAGE_ERROR) <<
	"Rts2ImageProc::init cannot get astrometry string, exiting!" <<
	sendLog;
      return ret;
    }

  ret = config->getString ("imgproc", "obsprocess", defaultObsProcess);
  if (ret)
    {
      logStream (MESSAGE_ERROR) <<
	"Rts2ImageProc::init cannot get obs process script, exiting" <<
	sendLog;
      return ret;
    }

  ret = config->getString ("imgproc", "darkprocess", defaultDarkProcess);
  if (ret)
    {
      logStream (MESSAGE_ERROR) <<
	"Rts2ImageProc::init cannot get dark process script, exiting" <<
	sendLog;
      return ret;
    }

  ret = config->getString ("imgproc", "flatprocess", defaultFlatProcess);
  if (ret)
    {
      logStream (MESSAGE_ERROR) <<
	"Rts2ImageProc::init cannot get flat process script, exiting" <<
	sendLog;
    }
  return ret;
}

void
Rts2ImageProc::postEvent (Rts2Event * event)
{
  int obsId;
  switch (event->getType ())
    {
    case EVENT_ALL_PROCESSED:
      obsId = *((int *) event->getArg ());
      queObs (NULL, obsId);
      break;
    }
  Rts2DeviceDb::postEvent (event);
}

int
Rts2ImageProc::idle ()
{
  std::list < Rts2ConnProcess * >::iterator img_iter;
  if (!runningImage && imagesQue.size () != 0)
    {
      img_iter = imagesQue.begin ();
      Rts2ConnProcess *newImage = *img_iter;
      imagesQue.erase (img_iter);
      changeRunning (newImage);
    }
  return Rts2Device::idle ();
}

int
Rts2ImageProc::info ()
{
  queSize->setValueInteger ((int) imagesQue.size () + (runningImage ? 1 : 0));
  return Rts2DeviceDb::info ();
}

int
Rts2ImageProc::changeMasterState (int new_state)
{
  switch (new_state & (SERVERD_STATUS_MASK | SERVERD_STANDBY_MASK))
    {
    case SERVERD_DUSK:
    case SERVERD_DUSK | SERVERD_STANDBY_MASK:
    case SERVERD_NIGHT:
    case SERVERD_NIGHT | SERVERD_STANDBY_MASK:
    case SERVERD_DAWN:
    case SERVERD_DAWN | SERVERD_STANDBY_MASK:
      if (imageGlob.gl_pathc)
	{
	  globfree (&imageGlob);
	  imageGlob.gl_pathc = 0;
	  globC = 0;
	}
      reprocessingPossible = 0;
      break;
    default:
      reprocessingPossible = 1;
      if (!runningImage && imagesQue.size () == 0)
	checkNotProcessed ();
    }
  // start dark & flat processing
  return Rts2DeviceDb::changeMasterState (new_state);
}

int
Rts2ImageProc::deleteConnection (Rts2Conn * conn)
{
  std::list < Rts2ConnProcess * >::iterator img_iter;
  for (img_iter = imagesQue.begin (); img_iter != imagesQue.end ();
       img_iter++)
    {
      (*img_iter)->deleteConnection (conn);
      if (*img_iter == conn)
	{
	  imagesQue.erase (img_iter);
	}
    }
  if (runningImage)
    runningImage->deleteConnection (conn);
  if (conn == runningImage)
    {
      // que next image
      // Rts2Device::deleteConnection will delete runningImage
      switch (runningImage->getAstrometryStat ())
	{
	case GET:
	  goodImages->inc ();
	  break;
	case TRASH:
	  trashImages->inc ();
	  break;
	case MORNING:
	  morningImages->inc ();
	  break;
	default:
	  break;
	}
      runningImage = NULL;
      img_iter = imagesQue.begin ();
      if (img_iter != imagesQue.end ())
	{
	  imagesQue.erase (img_iter);
	  changeRunning (*img_iter);
	}
      else
	{
	  maskState (DEVICE_ERROR_MASK | IMGPROC_MASK_RUN, IMGPROC_IDLE);
	  if (reprocessingPossible)
	    {
	      if (globC < imageGlob.gl_pathc)
		{
		  queImage (NULL, imageGlob.gl_pathv[globC]);
		  globC++;
		}
	      else if (imageGlob.gl_pathc > 0)
		{
		  globfree (&imageGlob);
		  imageGlob.gl_pathc = 0;
		}
	    }
	}
    }
  return Rts2DeviceDb::deleteConnection (conn);
}

void
Rts2ImageProc::changeRunning (Rts2ConnProcess * newImage)
{
  int ret;
  if (runningImage)
    {
      if (sendStop)
	{
	  runningImage->stop ();
	  imagesQue.push_front (runningImage);
	}
      else
	{
	  imagesQue.push_front (newImage);
	  infoAll ();
	  return;
	}
    }
  runningImage = newImage;
  ret = runningImage->init ();
  if (ret < 0)
    {
      delete runningImage;
      runningImage = NULL;
      maskState (DEVICE_ERROR_MASK | IMGPROC_MASK_RUN,
		 DEVICE_ERROR_HW | IMGPROC_IDLE);
      infoAll ();
      return;
    }
  else if (ret == 0)
    {
      addConnection (runningImage);
    }
  maskState (DEVICE_ERROR_MASK | IMGPROC_MASK_RUN, IMGPROC_RUN);
  infoAll ();
}

int
Rts2ImageProc::que (Rts2ConnProcess * newProc)
{
  if (runningImage)
    imagesQue.push_front (newProc);
  else
    changeRunning (newProc);
  infoAll ();
  return 0;
}

int
Rts2ImageProc::queImage (Rts2Conn * conn, const char *in_path)
{
  Rts2ConnImgProcess *newImageConn;
  newImageConn =
    new Rts2ConnImgProcess (this, conn, defaultImgProcess.c_str (), in_path,
			    Rts2Config::instance ()->getAstrometryTimeout ());
  return que (newImageConn);
}

int
Rts2ImageProc::doImage (Rts2Conn * conn, const char *in_path)
{
  Rts2ConnImgProcess *newImageConn;
  newImageConn =
    new Rts2ConnImgProcess (this, conn, defaultImgProcess.c_str (), in_path,
			    Rts2Config::instance ()->getAstrometryTimeout ());
  changeRunning (newImageConn);
  infoAll ();
  return 0;
}

int
Rts2ImageProc::queObs (Rts2Conn * conn, int obsId)
{
  Rts2ConnObsProcess *newObsConn;
  newObsConn =
    new Rts2ConnObsProcess (this, conn, defaultObsProcess.c_str (), obsId,
			    Rts2Config::instance ()->getObsProcessTimeout ());
  return que (newObsConn);
}

int
Rts2ImageProc::queDarks (Rts2Conn * conn)
{
  Rts2ConnDarkProcess *newDarkConn;
  newDarkConn =
    new Rts2ConnDarkProcess (this, conn, defaultDarkProcess.c_str (),
			     Rts2Config::instance ()->
			     getDarkProcessTimeout ());
  return que (newDarkConn);
}

int
Rts2ImageProc::queFlats (Rts2Conn * conn)
{
  Rts2ConnFlatProcess *newFlatConn;
  newFlatConn =
    new Rts2ConnFlatProcess (this, conn, defaultFlatProcess.c_str (),
			     Rts2Config::instance ()->
			     getFlatProcessTimeout ());
  return que (newFlatConn);
}

int
Rts2ImageProc::checkNotProcessed ()
{
  std::string image_glob;
  int ret;

  Rts2Config *config;
  config = Rts2Config::instance ();

  ret = config->getString ("imgproc", "imageglob", image_glob);
  if (ret)
    return ret;

  ret = glob (image_glob.c_str (), 0, NULL, &imageGlob);
  if (ret)
    {
      globfree (&imageGlob);
      imageGlob.gl_pathc = 0;
      return -1;
    }

  globC = 0;

  // start files que..
  if (imageGlob.gl_pathc > 0)
    return queImage (NULL, imageGlob.gl_pathv[0]);
  return 0;
}

int
Rts2ImageProc::commandAuthorized (Rts2Conn * conn)
{
  if (conn->isCommand ("que_image"))
    {
      char *in_imageName;
      if (conn->paramNextString (&in_imageName) || !conn->paramEnd ())
	return -2;
      return queImage (conn, in_imageName);
    }
  else if (conn->isCommand ("do_image"))
    {
      char *in_imageName;
      if (conn->paramNextString (&in_imageName) || !conn->paramEnd ())
	return -2;
      return doImage (conn, in_imageName);
    }
  else if (conn->isCommand ("que_obs"))
    {
      int obsId;
      if (conn->paramNextInteger (&obsId) || !conn->paramEnd ())
	return -2;
      return queObs (conn, obsId);
    }
  else if (conn->isCommand ("que_darks"))
    {
      if (!conn->paramEnd ())
	return -2;
      return queDarks (conn);
    }
  else if (conn->isCommand ("que_flats"))
    {
      if (!conn->paramEnd ())
	return -2;
      return queFlats (conn);
    }

  return Rts2DeviceDb::commandAuthorized (conn);
}

int
main (int argc, char **argv)
{
  Rts2ImageProc imgproc = Rts2ImageProc (argc, argv);
  return imgproc.run ();
}
