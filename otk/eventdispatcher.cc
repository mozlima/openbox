// -*- mode: C++; indent-tabs-mode: nil; c-basic-offset: 2; -*-

#ifdef HAVE_CONFIG_H
# include "../config.h"
#endif

#include "eventdispatcher.hh"
#include "display.hh"
#include <iostream>

namespace otk {

OtkEventDispatcher::OtkEventDispatcher()
  : _fallback(0), _master(0)
{
}

OtkEventDispatcher::~OtkEventDispatcher()
{
}

void OtkEventDispatcher::clearAllHandlers(void)
{
  _map.clear();
}

void OtkEventDispatcher::registerHandler(Window id, OtkEventHandler *handler)
{
  _map.insert(std::pair<Window, OtkEventHandler*>(id, handler));
}

void OtkEventDispatcher::clearHandler(Window id)
{
  _map.erase(id);
}

void OtkEventDispatcher::dispatchEvents(void)
{
  XEvent e;

  while (XPending(OBDisplay::display)) {
    XNextEvent(OBDisplay::display, &e);

#if 0//defined(DEBUG)
    printf("Event %d window %lx\n", e.type, e.xany.window);
#endif

    Window win;

    // pick a window
    switch (e.type) {
    case UnmapNotify:
      win = e.xunmap.window;
      break;
    case DestroyNotify:
      win = e.xdestroywindow.window;
      break;
    case ConfigureRequest:
      win = e.xconfigurerequest.window;
      break;
    default:
      win = e.xany.window;
    }
    
    // grab the lasttime and hack up the modifiers
    switch (e.type) {
    case ButtonPress:
    case ButtonRelease:
      _lasttime = e.xbutton.time;
      e.xbutton.state &= ~(LockMask | OBDisplay::numLockMask() |
                           OBDisplay::scrollLockMask());
      break;
    case KeyPress:
      e.xkey.state &= ~(LockMask | OBDisplay::numLockMask() |
                        OBDisplay::scrollLockMask());
      break;
    case MotionNotify:
      _lasttime = e.xmotion.time;
      e.xmotion.state &= ~(LockMask | OBDisplay::numLockMask() |
                           OBDisplay::scrollLockMask());
      break;
    case PropertyNotify:
      _lasttime = e.xproperty.time;
      break;
    case EnterNotify:
    case LeaveNotify:
      _lasttime = e.xcrossing.time;
      break;
    }

    if (e.type == FocusIn || e.type == FocusOut)
      // any other types are not ones we're interested in
      if (e.xfocus.detail != NotifyNonlinear)
        continue;

    if (e.type == FocusOut) {
      XEvent fi;
      // send a FocusIn first if one exists
      while (XCheckTypedEvent(OBDisplay::display, FocusIn, &fi)) {
        // any other types are not ones we're interested in
        if (fi.xfocus.detail == NotifyNonlinear) {
          dispatch(fi.xfocus.window, fi);
          break;
        }
      }
    }
    
    dispatch(win, e);
  }
}

void OtkEventDispatcher::dispatch(Window win, const XEvent &e)
{
  OtkEventHandler *handler = 0;
  OtkEventMap::iterator it;

  // master gets everything first
  if (_master)
    _master->handle(e);

  // find handler for the chosen window
  it = _map.find(win);

  if (it != _map.end()) {
    // if we found a handler
    handler = it->second;
  } else if (e.type == ConfigureRequest) {
    // unhandled configure requests must be used to configure the window
    // directly
    XWindowChanges xwc;
      
    xwc.x = e.xconfigurerequest.x;
    xwc.y = e.xconfigurerequest.y;
    xwc.width = e.xconfigurerequest.width;
    xwc.height = e.xconfigurerequest.height;
    xwc.border_width = e.xconfigurerequest.border_width;
    xwc.sibling = e.xconfigurerequest.above;
    xwc.stack_mode = e.xconfigurerequest.detail;
      
    XConfigureWindow(otk::OBDisplay::display, e.xconfigurerequest.window,
                     e.xconfigurerequest.value_mask, &xwc);
  } else {
    // grab a falback if it exists
    handler = _fallback;
  }

  if (handler)
    handler->handle(e);
}

OtkEventHandler *OtkEventDispatcher::findHandler(Window win)
{
  OtkEventMap::iterator it = _map.find(win);
  if (it != _map.end())
    return it->second;
  return 0;
}

}
