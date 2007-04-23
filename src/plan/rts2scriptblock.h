#ifndef __RTS2_SCRIPT_BLOCK__
#define __RTS2_SCRIPT_BLOCK__

#include "rts2script.h"

/**
 * Block - text surrounded by {}.
 *
 * @author petr
 */
class Rts2ScriptElementBlock:public Rts2ScriptElement
{
private:
  std::list < Rts2ScriptElement * >blockElements;
  std::list < Rts2ScriptElement * >::iterator curr_element;
  int blockScriptRet (int ret);
  int loopCount;
protected:
    virtual bool endLoop ()
  {
    return true;
  }
  virtual bool getNextLoop ()
  {
    return endLoop ();
  }
  int getLoopCount ()
  {
    return loopCount;
  }
public:
  Rts2ScriptElementBlock (Rts2Script * in_script);
  virtual ~ Rts2ScriptElementBlock (void);

  virtual void addElement (Rts2ScriptElement * element);

  virtual void postEvent (Rts2Event * event);

  virtual int defnextCommand (Rts2DevClient * client,
			      Rts2Command ** new_command,
			      char new_device[DEVICE_NAME_SIZE]);

  virtual int nextCommand (Rts2DevClientCamera * client,
			   Rts2Command ** new_command,
			   char new_device[DEVICE_NAME_SIZE]);

  virtual int nextCommand (Rts2DevClientPhot * client,
			   Rts2Command ** new_command,
			   char new_device[DEVICE_NAME_SIZE]);

  virtual int processImage (Rts2Image * image);
  virtual int waitForSignal (int in_sig);
  virtual void cancelCommands ();
};

/**
 * Will wait for signal, after signal we will end execution of that block.
 */
class Rts2SEBSignalEnd:public Rts2ScriptElementBlock
{
private:
  // sig_num will become -1, when we get signal..
  int sig_num;
protected:
    virtual bool endLoop ()
  {
    return (sig_num == -1);
  }
public:
    Rts2SEBSignalEnd (Rts2Script * in_script, int end_sig_num);
  virtual ~ Rts2SEBSignalEnd (void);

  virtual int waitForSignal (int in_sig);
};

/**
 * Will test if target is acquired. Can be acompained with else block, that
 * will be executed when 
 */
class Rts2SEBAcquired:public Rts2ScriptElementBlock
{
private:
  Rts2ScriptElementBlock * elseBlock;
  int tar_id;
  enum
  { NOT_CALLED, ACQ_OK, ACQ_FAILED } state;
  void checkState ();
protected:
    virtual bool endLoop ();
public:
    Rts2SEBAcquired (Rts2Script * in_script, int in_tar_id);
    virtual ~ Rts2SEBAcquired (void);

  virtual void postEvent (Rts2Event * event);

  virtual int defnextCommand (Rts2DevClient * client,
			      Rts2Command ** new_command,
			      char new_device[DEVICE_NAME_SIZE]);

  virtual int nextCommand (Rts2DevClientCamera * client,
			   Rts2Command ** new_command,
			   char new_device[DEVICE_NAME_SIZE]);

  virtual int nextCommand (Rts2DevClientPhot * client,
			   Rts2Command ** new_command,
			   char new_device[DEVICE_NAME_SIZE]);

  virtual int processImage (Rts2Image * image);
  virtual int waitForSignal (int in_sig);
  virtual void cancelCommands ();

  virtual void addElseElement (Rts2ScriptElement * element);
};

/**
 * Else block for storing else path..
 */
class Rts2SEBElse:public Rts2ScriptElementBlock
{
protected:
  virtual bool endLoop ();
public:
  Rts2SEBElse (Rts2Script * in_script):Rts2ScriptElementBlock (in_script)
  {
  }
};

class Rts2SEBFor:public Rts2ScriptElementBlock
{
private:
  int max;
protected:
    virtual bool endLoop ()
  {
    return getLoopCount () >= max;
  }
public:
    Rts2SEBFor (Rts2Script * in_script,
		int in_max):Rts2ScriptElementBlock (in_script)
  {
    max = in_max;
  }
};

#endif /* !__RTS2_SCRIPT_BLOCK__ */
